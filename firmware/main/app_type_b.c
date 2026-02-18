/**
 * @file app_type_b.c
 * @brief Type B 배터리 센서 노드 애플리케이션
 *
 * Deep Sleep 중심 순차 실행:
 * Wake → DS18B20 ON → 센서 측정 → CBOR 인코딩 → Thread TX
 * → 적응형 폴링 주기 계산 → DS18B20 OFF → Deep Sleep
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sht30.h"
#include "ds18b20.h"
#include "adaptive_poll.h"
#include "safety_monitor.h"
#include "thread_node.h"
#include "cbor_codec.h"
#include "power_mgmt.h"
#include "battery_monitor.h"
#include "nvs_config.h"
#include "preset_manager.h"

static const char *TAG = "APP_B";

/* RTC 메모리 — Deep Sleep을 걸쳐도 유지 */
static RTC_DATA_ATTR float s_prev_temp = 0.0f;
static RTC_DATA_ATTR uint32_t s_boot_count = 0;
static RTC_DATA_ATTR uint32_t s_battery_check_counter = 0;

void app_type_b_start(void)
{
    s_boot_count++;
    ESP_LOGI(TAG, "Type B starting (boot #%lu)", (unsigned long)s_boot_count);

    /* 1. 전원 관리 초기화 + wake-up 원인 */
    power_mgmt_init();
    bool first_boot = power_mgmt_is_first_boot();

    /* 2. 설정 로드 */
    nvs_config_init();
    preset_t preset;
    preset_load(&preset);

    /* 3. DS18B20 전원 ON */
    ds18b20_init(CONFIG_SENSOR_DS18B20_GPIO, CONFIG_DS18B20_POWER_GPIO);
    ds18b20_power_on();

    /* 4. SHT30 초기화 + 측정 */
    sht30_init(I2C_NUM_0, GPIO_NUM_6, GPIO_NUM_7, CONFIG_SENSOR_SHT30_ADDR);
    sht30_data_t sht_data = {0};
    sht30_read(&sht_data);

    /* 5. DS18B20 변환 시작 + 대기 */
    int ds_count = 0;
    ds18b20_search(&ds_count);
    ds18b20_start_conversion();
    vTaskDelay(pdMS_TO_TICKS(750));

    /* 6. DS18B20 온도 읽기 */
    float temp_hot = 0.0f, temp_cool = 0.0f;
    if (ds_count >= 1) ds18b20_read_temp(0, &temp_hot);
    if (ds_count >= 2) ds18b20_read_temp(1, &temp_cool);

    ESP_LOGI(TAG, "T_hot=%.1f T_cool=%.1f H=%.1f%%",
             temp_hot, temp_cool, sht_data.humidity);

    /* 7. 안전 검사 */
    safety_config_t scfg = {
        .overtemp_offset   = preset.safety.overtemp_offset,
        .heater_max_sec    = 0,     /* Type B: 히터 없음 */
        .stale_timeout_sec = 0,     /* Type B: 단발성 읽기, stale 검사 불필요 */
        .sensor_mismatch_c = 15.0f,
        .rate_max_per_sec  = 0.0f,  /* Type B: 이전 값 없으므로 비활성 */
    };
    safety_init(&scfg);
    safety_status_t status = safety_check(temp_hot, temp_cool,
                                           preset.temp_hot.target,
                                           sht_data.humidity);
    if (status != SAFETY_OK) {
        ESP_LOGW(TAG, "Safety: %s", safety_status_str(status));
    }

    /* 8. CBOR 인코딩 → Thread 전송 */
    float batt_pct = -1.0f;

    /* 10. (조건부) 배터리 ADC 읽기 */
    s_battery_check_counter++;
    if (first_boot || s_battery_check_counter >= CONFIG_BATTERY_CHECK_INTERVAL) {
        battery_monitor_init(ADC_CHANNEL_0);
        batt_pct = battery_monitor_read_percent();
        ESP_LOGI(TAG, "Battery: %.1f%%", batt_pct);
        battery_monitor_deinit();
        s_battery_check_counter = 0;
    }

    sensor_report_t report = {
        .temp_hot = temp_hot,
        .temp_cool = temp_cool,
        .humidity = sht_data.humidity,
        .battery_pct = batt_pct,
        .heater_duty = -1.0f,    /* Type B: 액추에이터 없음 */
        .light_duty = -1.0f,
        .safety_status = (int)status,
    };

    uint8_t cbor_buf[64];
    size_t cbor_len = 0;
    if (cbor_encode_report(&report, cbor_buf, sizeof(cbor_buf), &cbor_len) == ESP_OK) {
        thread_node_init(false);  /* SED 모드 */
        thread_node_start();

        /* 네트워크 연결 대기 (최대 5초) */
        for (int i = 0; i < 50 && !thread_node_is_connected(); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (thread_node_is_connected()) {
            thread_node_send(cbor_buf, cbor_len);
            vTaskDelay(pdMS_TO_TICKS(100));  /* 전송 완료 대기 */
        } else {
            ESP_LOGW(TAG, "Thread not connected, data lost");
        }
        thread_node_stop();
    }

    /* 9. 적응형 폴링 주기 계산 */
    adaptive_poll_config_t apcfg = {
        .period_fast_s = CONFIG_POLL_PERIOD_FAST,
        .period_slow_s = CONFIG_POLL_PERIOD_SLOW,
        .delta_high = (float)CONFIG_POLL_DELTA_HIGH / 10.0f,
    };
    adaptive_poll_init(&apcfg);
    uint32_t sleep_sec = adaptive_poll_calc(temp_hot, s_prev_temp);
    s_prev_temp = temp_hot;

    /* 11. DS18B20 전원 OFF + 센서 해제 */
    ds18b20_power_off();
    ds18b20_deinit();
    sht30_deinit();

    /* 12. Deep Sleep 진입 */
    ESP_LOGI(TAG, "Sleeping %lu seconds...", (unsigned long)sleep_sec);
    power_mgmt_deep_sleep(sleep_sec);
}
