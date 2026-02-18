/**
 * @file battery_monitor.c
 * @brief 배터리 전압 ADC 모니터링
 *
 * 분압기 (100k/100k) → ADC 읽기 → x2 보정 → 전압/잔량 변환
 * 21700 리튬이온: 4.2V(만충) ~ 3.0V(방전)
 */
#include "battery_monitor.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "battery";

#define BATT_FULL_MV     4200   /* 만충 전압 (mV) */
#define BATT_EMPTY_MV    3000   /* 방전 전압 (mV) */
#define BATT_LOW_MV      3200   /* 저전압 경고 (mV) */
#define DIVIDER_RATIO    2      /* 분압비 (100k/100k) */
#define ADC_UNIT         ADC_UNIT_1
#define ADC_ATTEN        ADC_ATTEN_DB_12
#define ADC_SAMPLES      16     /* 멀티샘플링 횟수 (노이즈 4x 감소) */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static adc_channel_t s_channel;
static bool s_initialized = false;

esp_err_t battery_monitor_init(adc_channel_t channel)
{
    s_channel = channel;

    /* ADC 유닛 초기화 */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (ret != ESP_OK) return ret;

    /* 채널 설정 */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, channel, &chan_cfg);
    if (ret != ESP_OK) {
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }

    /* 캘리브레이션 — ESP32-C6는 Curve Fitting 사용 */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT,
        .chan    = channel,
        .atten  = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT,
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
#else
    ret = ESP_ERR_NOT_SUPPORTED;
#endif
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
        s_cali_handle = NULL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Battery monitor init, ADC ch=%d", channel);
    return ESP_OK;
}

float battery_monitor_read_voltage(void)
{
    if (!s_initialized) return 0.0f;

    /* 16회 멀티샘플링 후 평균 (노이즈 sqrt(16)=4배 감소) */
    int64_t raw_sum = 0;
    int valid_count = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_handle, s_channel, &raw) == ESP_OK) {
            raw_sum += raw;
            valid_count++;
        }
    }
    if (valid_count == 0) {
        ESP_LOGE(TAG, "All ADC reads failed");
        return 0.0f;
    }
    int avg_raw = (int)(raw_sum / valid_count);

    int voltage_mv;
    if (s_cali_handle) {
        adc_cali_raw_to_voltage(s_cali_handle, avg_raw, &voltage_mv);
    } else {
        voltage_mv = avg_raw * 3300 / 4095;
    }

    /* 분압기 보정 */
    float batt_mv = (float)voltage_mv * DIVIDER_RATIO;
    return batt_mv / 1000.0f;
}

float battery_monitor_read_percent(void)
{
    float mv = battery_monitor_read_voltage() * 1000.0f;

    if (mv >= BATT_FULL_MV) return 100.0f;
    if (mv <= BATT_EMPTY_MV) return 0.0f;

    return (mv - BATT_EMPTY_MV) * 100.0f / (BATT_FULL_MV - BATT_EMPTY_MV);
}

bool battery_monitor_is_low(void)
{
    float mv = battery_monitor_read_voltage() * 1000.0f;
    return (mv < BATT_LOW_MV);
}

esp_err_t battery_monitor_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    if (s_cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(s_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(s_cali_handle);
#endif
        s_cali_handle = NULL;
    }
    if (s_adc_handle) {
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
    }
    s_initialized = false;
    return ESP_OK;
}
