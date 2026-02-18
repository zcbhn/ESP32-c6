/**
 * @file scheduler.h
 * @brief RTC 기반 조명/히터 스케줄러
 */
#ifndef RBMS_SCHEDULER_H
#define RBMS_SCHEDULER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  on_hour;       /* 점등 시각 (0-23) */
    uint8_t  off_hour;      /* 소등 시각 (0-23) */
    uint8_t  sunrise_min;   /* 일출 페이드 시간 (분) */
    uint8_t  sunset_min;    /* 일몰 페이드 시간 (분) */
} light_schedule_t;

esp_err_t scheduler_init(void);
esp_err_t scheduler_set_light(const light_schedule_t *sched);
esp_err_t scheduler_set_time(uint8_t hour, uint8_t minute);

/** @brief 현재 조명이 켜져야 하는지 */
bool scheduler_is_light_on(void);

/** @brief 현재 디밍 레벨 (0.0=OFF ~ 1.0=MAX) */
float scheduler_get_dimming(void);

/** @brief 매 분마다 호출하여 내부 상태 업데이트 */
void scheduler_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_SCHEDULER_H */
