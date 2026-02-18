/**
 * @file preset_manager.h
 * @brief 종별 프리셋 관리
 */
#ifndef RBMS_PRESET_MANAGER_H
#define RBMS_PRESET_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char species[32];
    struct { float target, min, max; } temp_hot;
    struct { float target, min, max; } temp_cool;
    struct { float target, min, max; } humidity;
    struct {
        uint8_t on_hour, off_hour;
        uint8_t sunrise_min, sunset_min;
    } light;
    struct { float kp, ki, kd; } pid;
    struct {
        float overtemp_offset;
        uint32_t heater_max_sec;
    } safety;
} preset_t;

/** @brief 기본 프리셋 로드 (볼파이톤) */
esp_err_t preset_load_default(preset_t *preset);

/** @brief NVS에서 프리셋 로드 */
esp_err_t preset_load(preset_t *preset);

/** @brief NVS에 프리셋 저장 */
esp_err_t preset_save(const preset_t *preset);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_PRESET_MANAGER_H */
