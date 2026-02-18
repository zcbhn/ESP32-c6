/**
 * @file ds18b20.c
 * @brief DS18B20 1-Wire 온도 센서 드라이버
 *
 * 1-Wire 프로토콜을 GPIO bit-bang으로 구현.
 * Type B에서는 power_gpio로 VCC를 제어하여 배터리 절약.
 */
#include "ds18b20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static portMUX_TYPE s_ow_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *TAG = "ds18b20";

/* 1-Wire 타이밍 (마이크로초) */
#define OW_RESET_PULSE_US     480
#define OW_PRESENCE_WAIT_US   70
#define OW_PRESENCE_SAMPLE_US 410
#define OW_WRITE_SLOT_US      60
#define OW_WRITE_1_LOW_US     6
#define OW_WRITE_0_LOW_US     60
#define OW_READ_INIT_US       6
#define OW_READ_SAMPLE_US     9
#define OW_READ_SLOT_US       55

/* DS18B20 ROM 커맨드 */
#define CMD_SEARCH_ROM   0xF0
#define CMD_SKIP_ROM     0xCC
#define CMD_MATCH_ROM    0x55
/* DS18B20 Function 커맨드 */
#define CMD_CONVERT_T    0x44
#define CMD_READ_SCRATCH 0xBE

#define CONVERSION_DELAY_MS  750  /* 12비트 해상도 */

static gpio_num_t s_data_gpio  = GPIO_NUM_NC;
static gpio_num_t s_power_gpio = GPIO_NUM_NC;
static ds18b20_sensor_t s_sensors[DS18B20_MAX_SENSORS];
static int s_sensor_count = 0;
static bool s_initialized = false;

/* --- 1-Wire 저수준 --- */

static inline void ow_delay_us(uint32_t us)
{
    uint64_t end = esp_timer_get_time() + us;
    while (esp_timer_get_time() < end) { }
}

static void ow_set_output(int level)
{
    gpio_set_direction(s_data_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_data_gpio, level);
}

static int ow_read_input(void)
{
    gpio_set_direction(s_data_gpio, GPIO_MODE_INPUT);
    return gpio_get_level(s_data_gpio);
}

static bool ow_reset(void)
{
    portENTER_CRITICAL(&s_ow_mux);
    ow_set_output(0);
    ow_delay_us(OW_RESET_PULSE_US);
    ow_read_input();  /* 릴리즈 (풀업) */
    ow_delay_us(OW_PRESENCE_WAIT_US);
    int presence = ow_read_input();
    ow_delay_us(OW_PRESENCE_SAMPLE_US);
    portEXIT_CRITICAL(&s_ow_mux);
    return (presence == 0);  /* 0 = 디바이스 응답 */
}

static void ow_write_bit(int bit)
{
    portENTER_CRITICAL(&s_ow_mux);
    ow_set_output(0);
    if (bit) {
        ow_delay_us(OW_WRITE_1_LOW_US);
        ow_set_output(1);
        ow_delay_us(OW_WRITE_SLOT_US - OW_WRITE_1_LOW_US);
    } else {
        ow_delay_us(OW_WRITE_0_LOW_US);
        ow_set_output(1);
    }
    ow_delay_us(2);
    portEXIT_CRITICAL(&s_ow_mux);
}

static int ow_read_bit(void)
{
    portENTER_CRITICAL(&s_ow_mux);
    ow_set_output(0);
    ow_delay_us(OW_READ_INIT_US);
    ow_read_input();
    ow_delay_us(OW_READ_SAMPLE_US - OW_READ_INIT_US);
    int val = ow_read_input();
    ow_delay_us(OW_READ_SLOT_US);
    portEXIT_CRITICAL(&s_ow_mux);
    return val;
}

static void ow_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte |= (ow_read_bit() << i);
    }
    return byte;
}

/* Dallas CRC8 (polynomial 0x8C reflected) */
static uint8_t ds_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

/* --- 공개 API --- */

esp_err_t ds18b20_init(gpio_num_t data_gpio, gpio_num_t power_gpio)
{
    s_data_gpio = data_gpio;
    s_power_gpio = power_gpio;
    s_sensor_count = 0;
    memset(s_sensors, 0, sizeof(s_sensors));

    /* 데이터 핀: 오픈 드레인 + 외부 풀업 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << data_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* 전원 제어 핀 (Type B) */
    if (power_gpio != GPIO_NUM_NC) {
        gpio_config_t pwr_conf = {
            .pin_bit_mask = (1ULL << power_gpio),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&pwr_conf);
        gpio_set_level(power_gpio, 0);  /* 초기: OFF */
    }

    s_initialized = true;
    ESP_LOGI(TAG, "DS18B20 initialized, data=GPIO%d, power=GPIO%d",
             data_gpio, (power_gpio != GPIO_NUM_NC) ? power_gpio : -1);
    return ESP_OK;
}

esp_err_t ds18b20_power_on(void)
{
    if (s_power_gpio == GPIO_NUM_NC) {
        return ESP_OK;  /* 상시 전원 */
    }
    gpio_set_level(s_power_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  /* 센서 안정화 대기 */
    ESP_LOGD(TAG, "DS18B20 power ON");
    return ESP_OK;
}

esp_err_t ds18b20_power_off(void)
{
    if (s_power_gpio == GPIO_NUM_NC) {
        return ESP_OK;
    }
    gpio_set_level(s_power_gpio, 0);
    ESP_LOGD(TAG, "DS18B20 power OFF");
    return ESP_OK;
}

esp_err_t ds18b20_search(int *count)
{
    if (!s_initialized || count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_sensor_count = 0;
    uint8_t last_discrepancy = 0;
    bool search_done = false;
    uint8_t rom[8] = {0};

    while (!search_done && s_sensor_count < DS18B20_MAX_SENSORS) {
        if (!ow_reset()) {
            ESP_LOGW(TAG, "No device response on bus");
            break;
        }

        ow_write_byte(CMD_SEARCH_ROM);
        uint8_t discrepancy_marker = 0;

        for (int bit_num = 1; bit_num <= 64; bit_num++) {
            int id_bit     = ow_read_bit();
            int cmp_id_bit = ow_read_bit();

            int byte_idx = (bit_num - 1) / 8;
            int bit_mask = 1 << ((bit_num - 1) % 8);

            if (id_bit == 1 && cmp_id_bit == 1) {
                /* 디바이스 없음 */
                break;
            }

            int direction;
            if (id_bit != cmp_id_bit) {
                direction = id_bit;
            } else {
                /* 충돌 */
                if (bit_num < last_discrepancy) {
                    direction = (rom[byte_idx] & bit_mask) ? 1 : 0;
                } else if (bit_num == last_discrepancy) {
                    direction = 1;
                } else {
                    direction = 0;
                }
                if (direction == 0) {
                    discrepancy_marker = bit_num;
                }
            }

            if (direction) {
                rom[byte_idx] |= bit_mask;
            } else {
                rom[byte_idx] &= ~bit_mask;
            }

            ow_write_bit(direction);
        }

        /* CRC 검증 */
        if (ds_crc8(rom, 7) == rom[7]) {
            memcpy(s_sensors[s_sensor_count].rom, rom, 8);
            s_sensors[s_sensor_count].valid = true;
            ESP_LOGI(TAG, "Found sensor %d: %02X%02X%02X%02X%02X%02X%02X%02X",
                     s_sensor_count,
                     rom[0], rom[1], rom[2], rom[3],
                     rom[4], rom[5], rom[6], rom[7]);
            s_sensor_count++;
        }

        last_discrepancy = discrepancy_marker;
        if (last_discrepancy == 0) {
            search_done = true;
        }
    }

    *count = s_sensor_count;
    ESP_LOGI(TAG, "Search complete: %d sensor(s) found", s_sensor_count);
    return ESP_OK;
}

esp_err_t ds18b20_start_conversion(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ow_reset()) {
        return ESP_ERR_NOT_FOUND;
    }

    ow_write_byte(CMD_SKIP_ROM);   /* 모든 센서에 동시 명령 */
    ow_write_byte(CMD_CONVERT_T);
    return ESP_OK;
}

esp_err_t ds18b20_read_temp(int idx, float *temperature)
{
    if (!s_initialized || idx < 0 || idx >= s_sensor_count || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ow_reset()) {
        return ESP_ERR_NOT_FOUND;
    }

    /* 특정 센서 선택 */
    ow_write_byte(CMD_MATCH_ROM);
    for (int i = 0; i < 8; i++) {
        ow_write_byte(s_sensors[idx].rom[i]);
    }

    /* Scratchpad 읽기 */
    ow_write_byte(CMD_READ_SCRATCH);
    uint8_t scratch[9];
    for (int i = 0; i < 9; i++) {
        scratch[i] = ow_read_byte();
    }

    /* CRC 검증 */
    if (ds_crc8(scratch, 8) != scratch[8]) {
        ESP_LOGE(TAG, "Scratchpad CRC mismatch (sensor %d)", idx);
        return ESP_ERR_INVALID_CRC;
    }

    /* 12비트 해상도 변환 */
    int16_t raw = (scratch[1] << 8) | scratch[0];
    *temperature = (float)raw / 16.0f;
    s_sensors[idx].temperature = *temperature;

    return ESP_OK;
}

const ds18b20_sensor_t *ds18b20_get_sensors(void)
{
    return s_sensors;
}

esp_err_t ds18b20_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    ds18b20_power_off();
    s_initialized = false;
    s_sensor_count = 0;
    return ESP_OK;
}
