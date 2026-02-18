/**
 * @file sht30.c
 * @brief SHT30 I2C 온습도 센서 드라이버
 */
#include "sht30.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <string.h>

static const char *TAG = "sht30";

/* SHT30 커맨드 */
#define SHT30_CMD_MEASURE_HIGH_H  0x24  /* Single-Shot, High repeatability */
#define SHT30_CMD_MEASURE_HIGH_L  0x00
#define SHT30_CMD_SOFT_RESET_H    0x30
#define SHT30_CMD_SOFT_RESET_L    0xA2
#define SHT30_MEASURE_DELAY_MS    20    /* 최대 15.5ms */
#define SHT30_MAX_RETRY           2     /* 통신 실패 시 최대 재시도 */

static i2c_port_t s_port = I2C_NUM_0;
static uint8_t    s_addr = 0x44;
static bool       s_initialized = false;
static gpio_num_t s_sda_gpio = GPIO_NUM_NC;
static gpio_num_t s_scl_gpio = GPIO_NUM_NC;

/* CRC-8 (polynomial 0x31, init 0xFF) */
static uint8_t sht30_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

/**
 * @brief I2C 버스 복구 (9 clock pulses)
 * SDA가 LOW에 고정된 경우 SCL 클럭 9회 → STOP 조건 생성
 */
static void i2c_bus_recovery(void)
{
    ESP_LOGW(TAG, "I2C bus recovery: sending 9 clock pulses");

    i2c_driver_delete(s_port);

    gpio_set_direction(s_scl_gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(s_sda_gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(s_sda_gpio, 1);
    gpio_set_level(s_scl_gpio, 1);

    /* 9 clock pulses (I2C 스펙 준수) */
    for (int i = 0; i < 9; i++) {
        gpio_set_level(s_scl_gpio, 0);
        esp_rom_delay_us(5);
        gpio_set_level(s_scl_gpio, 1);
        esp_rom_delay_us(5);
    }

    /* STOP 조건 생성: SDA LOW → SCL HIGH → SDA HIGH */
    gpio_set_level(s_sda_gpio, 0);
    esp_rom_delay_us(5);
    gpio_set_level(s_scl_gpio, 1);
    esp_rom_delay_us(5);
    gpio_set_level(s_sda_gpio, 1);
    esp_rom_delay_us(5);

    /* I2C 드라이버 재설치 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = s_sda_gpio,
        .scl_io_num = s_scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(s_port, &conf);
    i2c_driver_install(s_port, I2C_MODE_MASTER, 0, 0, 0);

    ESP_LOGI(TAG, "I2C bus recovery done");
}

esp_err_t sht30_init(i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio, uint8_t addr)
{
    s_port = port;
    s_addr = addr;
    s_sda_gpio = sda_gpio;
    s_scl_gpio = scl_gpio;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    esp_err_t ret = i2c_param_config(port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SHT30 initialized on I2C%d, addr=0x%02X", port, addr);
    return ESP_OK;
}

esp_err_t sht30_read(sht30_data_t *data)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int retry = 0; retry <= SHT30_MAX_RETRY; retry++) {
        /* Single-Shot 측정 커맨드 전송 */
        uint8_t cmd[2] = { SHT30_CMD_MEASURE_HIGH_H, SHT30_CMD_MEASURE_HIGH_L };
        esp_err_t ret = i2c_master_write_to_device(s_port, s_addr, cmd, sizeof(cmd),
                                                    pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Measure cmd failed (try %d): %s", retry, esp_err_to_name(ret));
            if (retry < SHT30_MAX_RETRY) {
                i2c_bus_recovery();
                sht30_reset();
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(SHT30_MEASURE_DELAY_MS));

        /* 6바이트 읽기: [temp_H, temp_L, temp_CRC, hum_H, hum_L, hum_CRC] */
        uint8_t raw[6];
        ret = i2c_master_read_from_device(s_port, s_addr, raw, sizeof(raw),
                                           pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Read failed (try %d): %s", retry, esp_err_to_name(ret));
            if (retry < SHT30_MAX_RETRY) {
                i2c_bus_recovery();
                sht30_reset();
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            return ret;
        }

        /* CRC 검증 */
        if (sht30_crc8(raw, 2) != raw[2] || sht30_crc8(raw + 3, 2) != raw[5]) {
            ESP_LOGW(TAG, "CRC mismatch (try %d)", retry);
            if (retry < SHT30_MAX_RETRY) {
                sht30_reset();
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            return ESP_ERR_INVALID_CRC;
        }

        /* 원시값 → 물리값 변환 */
        uint16_t raw_temp = (raw[0] << 8) | raw[1];
        uint16_t raw_hum  = (raw[3] << 8) | raw[4];

        data->temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
        data->humidity    = 100.0f * ((float)raw_hum / 65535.0f);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t sht30_reset(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t cmd[2] = { SHT30_CMD_SOFT_RESET_H, SHT30_CMD_SOFT_RESET_L };
    esp_err_t ret = i2c_master_write_to_device(s_port, s_addr, cmd, sizeof(cmd),
                                                pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_LOGI(TAG, "SHT30 soft reset done");
    return ESP_OK;
}

esp_err_t sht30_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    s_initialized = false;
    return i2c_driver_delete(s_port);
}
