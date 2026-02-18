/**
 * @file adaptive_poll.h
 * @brief 적응형 폴링 주기 계산 (Type B)
 */
#ifndef RBMS_ADAPTIVE_POLL_H
#define RBMS_ADAPTIVE_POLL_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t period_fast_s;  /* 빠른 폴링 (초) */
    uint32_t period_slow_s;  /* 느린 폴링 (초) */
    float    delta_high;     /* 급변 임계값 (°C) */
} adaptive_poll_config_t;

esp_err_t adaptive_poll_init(const adaptive_poll_config_t *cfg);

/**
 * @brief 다음 폴링 주기를 계산
 * @param current_temp 현재 온도
 * @param prev_temp 이전 온도 (RTC 메모리에서 복원)
 * @return 다음 sleep 시간 (초)
 */
uint32_t adaptive_poll_calc(float current_temp, float prev_temp);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_ADAPTIVE_POLL_H */
