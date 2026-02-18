/**
 * @file preset_manager.c
 * @brief 종별 프리셋 관리
 */
#include "preset_manager.h"
#include "nvs_config.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>
#include <math.h>

static const char *TAG = "preset";
#define PRESET_NVS_KEY "preset"

/* 기본 프리셋: 볼파이톤 */
static const preset_t s_default_preset = {
    .species = "Ball Python",
    .temp_hot  = { .target = 32.0f, .min = 30.0f, .max = 34.0f },
    .temp_cool = { .target = 26.0f, .min = 24.0f, .max = 28.0f },
    .humidity  = { .target = 60.0f, .min = 50.0f, .max = 70.0f },
    .light = {
        .on_hour = 7, .off_hour = 19,
        .sunrise_min = 30, .sunset_min = 30,
    },
    .pid = { .kp = 2.0f, .ki = 0.5f, .kd = 1.0f },
    .safety = { .overtemp_offset = 5.0f, .heater_max_sec = 3600 },
};

esp_err_t preset_load_default(preset_t *preset)
{
    if (preset == NULL) return ESP_ERR_INVALID_ARG;
    *preset = s_default_preset;
    ESP_LOGI(TAG, "Default preset loaded: %s", preset->species);
    return ESP_OK;
}

/* NVS에서 읽은 프리셋 데이터 유효성 검증 */
static bool preset_validate(const preset_t *p)
{
    /* species null-termination 보장 확인 */
    bool has_null = false;
    for (int i = 0; i < (int)sizeof(p->species); i++) {
        if (p->species[i] == '\0') { has_null = true; break; }
    }
    if (!has_null) return false;

    /* 온도 범위 검사 (DS18B20 물리적 한계 내) */
    if (isnanf(p->temp_hot.target) || p->temp_hot.target < -20.0f || p->temp_hot.target > 80.0f) return false;
    if (isnanf(p->temp_cool.target) || p->temp_cool.target < -20.0f || p->temp_cool.target > 80.0f) return false;
    if (p->temp_hot.min >= p->temp_hot.max) return false;
    if (p->temp_cool.min >= p->temp_cool.max) return false;

    /* 습도 범위 검사 */
    if (isnanf(p->humidity.target) || p->humidity.target < 0.0f || p->humidity.target > 100.0f) return false;

    /* PID 계수 검사 (음수 불가, NaN 불가) */
    if (isnanf(p->pid.kp) || p->pid.kp < 0.0f) return false;
    if (isnanf(p->pid.ki) || p->pid.ki < 0.0f) return false;
    if (isnanf(p->pid.kd) || p->pid.kd < 0.0f) return false;

    /* 안전 설정 검사 */
    if (isnanf(p->safety.overtemp_offset) || p->safety.overtemp_offset <= 0.0f) return false;

    /* 조명 스케줄 검사 (시간: 0~23) */
    if (p->light.on_hour > 23 || p->light.off_hour > 23) return false;

    return true;
}

esp_err_t preset_load(preset_t *preset)
{
    if (preset == NULL) return ESP_ERR_INVALID_ARG;

    size_t len = 0;
    esp_err_t ret = nvs_config_load_blob(PRESET_NVS_KEY, preset, sizeof(preset_t), &len);
    if (ret != ESP_OK || len != sizeof(preset_t)) {
        ESP_LOGW(TAG, "No saved preset, loading default");
        return preset_load_default(preset);
    }

    /* 방어적 null-termination 보장 */
    preset->species[sizeof(preset->species) - 1] = '\0';

    if (!preset_validate(preset)) {
        ESP_LOGW(TAG, "Invalid preset data in NVS, loading default");
        return preset_load_default(preset);
    }

    ESP_LOGI(TAG, "Preset loaded from NVS: %s", preset->species);
    return ESP_OK;
}

esp_err_t preset_save(const preset_t *preset)
{
    if (preset == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = nvs_config_save_blob(PRESET_NVS_KEY, preset, sizeof(preset_t));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Preset saved: %s", preset->species);
    }
    return ret;
}
