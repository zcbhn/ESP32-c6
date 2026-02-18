/**
 * @file safety_monitor.h
 * @brief 과열/센서 이상 안전 감시
 */
#ifndef RBMS_SAFETY_MONITOR_H
#define RBMS_SAFETY_MONITOR_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SAFETY_OK = 0,
    SAFETY_WARN_HIGH_TEMP,
    SAFETY_WARN_LOW_TEMP,
    SAFETY_FAULT_OVERTEMP,
    SAFETY_FAULT_SENSOR,
    SAFETY_FAULT_SENSOR_STALE,
    SAFETY_FAULT_SENSOR_MISMATCH,
    SAFETY_FAULT_HEATER_TIMEOUT,
} safety_status_t;

typedef struct {
    float    overtemp_offset;    /* 과열 오프셋 (°C) */
    uint32_t heater_max_sec;     /* 히터 최대 연속 시간 (초) */
    uint32_t stale_timeout_sec;  /* 센서 읽기 stale 타임아웃 (초), 0=비활성 */
    float    sensor_mismatch_c;  /* 듀얼 센서 불일치 허용 범위 (°C), 0=비활성 */
    float    rate_max_per_sec;   /* 최대 온도 변화율 (°C/초), 0=비활성 */
} safety_config_t;

esp_err_t safety_init(const safety_config_t *cfg);

/**
 * @brief 안전 검사 수행
 * @param temp_hot 핫존 온도
 * @param temp_cool 쿨존 온도
 * @param setpoint 목표 온도 (핫존)
 * @param humidity 습도
 * @return 안전 상태
 */
safety_status_t safety_check(float temp_hot, float temp_cool, float setpoint, float humidity);

/** @brief 히터 동작 시간 업데이트 (1초 주기 호출) */
void safety_heater_tick(bool heater_on);

/** @brief 히터 타이머 리셋 */
void safety_heater_timer_reset(void);

/** @brief 긴급 차단 (모든 출력 OFF) */
esp_err_t safety_emergency_shutdown(void);

/** @brief 안전 상태 문자열 반환 */
const char *safety_status_str(safety_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_SAFETY_MONITOR_H */
