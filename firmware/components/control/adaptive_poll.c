/**
 * @file adaptive_poll.c
 * @brief 적응형 폴링 주기 계산 (Type B)
 *
 * |delta| >= delta_high → fast (30초)
 * |delta| ≈ 0          → slow (300초)
 * 중간값: 선형 보간
 */
#include "adaptive_poll.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "adaptive_poll";

static adaptive_poll_config_t s_cfg = {
    .period_fast_s = 30,
    .period_slow_s = 300,
    .delta_high = 1.0f,
};

esp_err_t adaptive_poll_init(const adaptive_poll_config_t *cfg)
{
    if (cfg != NULL) {
        s_cfg = *cfg;
    }
    ESP_LOGI(TAG, "Adaptive poll: fast=%lus slow=%lus delta=%.1f°C",
             (unsigned long)s_cfg.period_fast_s,
             (unsigned long)s_cfg.period_slow_s,
             s_cfg.delta_high);
    return ESP_OK;
}

uint32_t adaptive_poll_calc(float current_temp, float prev_temp)
{
    float delta = fabsf(current_temp - prev_temp);

    if (s_cfg.delta_high <= 0.0f || delta >= s_cfg.delta_high) {
        return s_cfg.period_fast_s;
    }

    /* 선형 보간: delta=0 → slow, delta=high → fast */
    float ratio = delta / s_cfg.delta_high;
    uint32_t range = s_cfg.period_slow_s - s_cfg.period_fast_s;
    uint32_t period = s_cfg.period_slow_s - (uint32_t)(ratio * range);

    ESP_LOGD(TAG, "delta=%.2f°C -> period=%lus", delta, (unsigned long)period);
    return period;
}
