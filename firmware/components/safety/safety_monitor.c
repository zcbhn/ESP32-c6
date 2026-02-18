/**
 * @file safety_monitor.c
 * @brief 과열/센서 이상 안전 감시
 *
 * 기능: 센서 유효범위, 과열, 히터 타임아웃,
 *       센서 stale 감지, 듀얼센서 불일치, 온도 변화율 감시
 */
#include "safety_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "safety";

/* 센서 유효 범위 (DS18B20 물리 한계) */
#define TEMP_VALID_MIN  (-20.0f)
#define TEMP_VALID_MAX  (85.0f)
#define HUM_VALID_MIN   (0.0f)
#define HUM_VALID_MAX   (100.0f)

static safety_config_t s_cfg = {
    .overtemp_offset   = 5.0f,
    .heater_max_sec    = 3600,
    .stale_timeout_sec = 30,
    .sensor_mismatch_c = 10.0f,
    .rate_max_per_sec  = 2.0f,
};
static uint32_t s_heater_on_sec = 0;

/* stale 감지용 타임스탬프 (마이크로초) */
static int64_t s_last_valid_us = 0;

/* 변화율 감지용 이전 값 */
static float s_prev_temp_hot  = -999.0f;
static int64_t s_prev_time_us = 0;

esp_err_t safety_init(const safety_config_t *cfg)
{
    if (cfg != NULL) {
        s_cfg = *cfg;
    }
    s_heater_on_sec = 0;
    s_last_valid_us = esp_timer_get_time();
    s_prev_temp_hot = -999.0f;
    s_prev_time_us  = 0;
    ESP_LOGI(TAG, "Safety init: overtemp=+%.1f°C heater_max=%lus stale=%lus mismatch=%.1f°C rate=%.1f°C/s",
             s_cfg.overtemp_offset, (unsigned long)s_cfg.heater_max_sec,
             (unsigned long)s_cfg.stale_timeout_sec, s_cfg.sensor_mismatch_c,
             s_cfg.rate_max_per_sec);
    return ESP_OK;
}

safety_status_t safety_check(float temp_hot, float temp_cool, float setpoint, float humidity)
{
    int64_t now_us = esp_timer_get_time();

    /* 1. 센서 유효범위 검사 */
    if (temp_hot < TEMP_VALID_MIN || temp_hot > TEMP_VALID_MAX ||
        temp_cool < TEMP_VALID_MIN || temp_cool > TEMP_VALID_MAX) {
        ESP_LOGE(TAG, "Sensor fault: hot=%.1f cool=%.1f", temp_hot, temp_cool);
        return SAFETY_FAULT_SENSOR;
    }

    /* 2. Stale 센서 감지 — 마지막 유효 읽기 이후 시간 초과 */
    if (s_cfg.stale_timeout_sec > 0) {
        int64_t elapsed_sec = (now_us - s_last_valid_us) / 1000000LL;
        if (elapsed_sec > (int64_t)s_cfg.stale_timeout_sec) {
            ESP_LOGE(TAG, "Sensor stale: %llds since last valid reading",
                     (long long)elapsed_sec);
            return SAFETY_FAULT_SENSOR_STALE;
        }
    }
    s_last_valid_us = now_us;  /* 유효 읽기 시각 갱신 */

    /* 3. 듀얼 센서 불일치 검사 (핫존 vs 쿨존) */
    if (s_cfg.sensor_mismatch_c > 0.0f) {
        float diff = temp_hot - temp_cool;
        if (diff < 0) diff = -diff;
        if (diff > s_cfg.sensor_mismatch_c) {
            ESP_LOGE(TAG, "Sensor mismatch: hot=%.1f cool=%.1f diff=%.1f > %.1f",
                     temp_hot, temp_cool, diff, s_cfg.sensor_mismatch_c);
            return SAFETY_FAULT_SENSOR_MISMATCH;
        }
    }

    /* 4. 온도 변화율 감시 (물리적으로 불가능한 변화 감지) */
    if (s_cfg.rate_max_per_sec > 0.0f && s_prev_temp_hot > -900.0f && s_prev_time_us > 0) {
        float dt_sec = (float)(now_us - s_prev_time_us) / 1000000.0f;
        if (dt_sec > 0.1f) {  /* 최소 100ms 간격 */
            float rate = (temp_hot - s_prev_temp_hot) / dt_sec;
            if (rate < 0) rate = -rate;
            if (rate > s_cfg.rate_max_per_sec) {
                ESP_LOGE(TAG, "Temp rate fault: %.2f°C/s > %.2f°C/s",
                         rate, s_cfg.rate_max_per_sec);
                return SAFETY_FAULT_SENSOR;
            }
        }
    }
    s_prev_temp_hot = temp_hot;
    s_prev_time_us  = now_us;

    /* 5. 과열 검사 */
    float overtemp_limit = setpoint + s_cfg.overtemp_offset;
    if (temp_hot > overtemp_limit) {
        ESP_LOGE(TAG, "OVERTEMP: hot=%.1f > limit=%.1f", temp_hot, overtemp_limit);
        return SAFETY_FAULT_OVERTEMP;
    }

    /* 6. 히터 연속 동작 제한 */
    if (s_cfg.heater_max_sec > 0 && s_heater_on_sec >= s_cfg.heater_max_sec) {
        ESP_LOGE(TAG, "Heater timeout: %lus >= %lus",
                 (unsigned long)s_heater_on_sec, (unsigned long)s_cfg.heater_max_sec);
        return SAFETY_FAULT_HEATER_TIMEOUT;
    }

    /* 7. 경고: 고온 접근 */
    if (temp_hot > setpoint + (s_cfg.overtemp_offset * 0.7f)) {
        return SAFETY_WARN_HIGH_TEMP;
    }

    return SAFETY_OK;
}

void safety_heater_tick(bool heater_on)
{
    if (heater_on) {
        s_heater_on_sec++;
    } else {
        s_heater_on_sec = 0;  /* OFF 되면 리셋 */
    }
}

void safety_heater_timer_reset(void)
{
    s_heater_on_sec = 0;
}

esp_err_t safety_emergency_shutdown(void)
{
    ESP_LOGE(TAG, "!!! EMERGENCY SHUTDOWN !!!");
    /* 호출자가 SSR/PWM off를 직접 수행해야 함 */
    s_heater_on_sec = 0;
    return ESP_OK;
}

const char *safety_status_str(safety_status_t status)
{
    switch (status) {
        case SAFETY_OK:                    return "OK";
        case SAFETY_WARN_HIGH_TEMP:        return "WARN_HIGH_TEMP";
        case SAFETY_WARN_LOW_TEMP:         return "WARN_LOW_TEMP";
        case SAFETY_FAULT_OVERTEMP:        return "FAULT_OVERTEMP";
        case SAFETY_FAULT_SENSOR:          return "FAULT_SENSOR";
        case SAFETY_FAULT_SENSOR_STALE:    return "FAULT_SENSOR_STALE";
        case SAFETY_FAULT_SENSOR_MISMATCH: return "FAULT_SENSOR_MISMATCH";
        case SAFETY_FAULT_HEATER_TIMEOUT:  return "FAULT_HEATER_TIMEOUT";
        default:                           return "UNKNOWN";
    }
}
