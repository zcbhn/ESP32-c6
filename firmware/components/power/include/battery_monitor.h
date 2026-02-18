/**
 * @file battery_monitor.h
 * @brief 배터리 전압 ADC 모니터링
 */
#ifndef RBMS_BATTERY_MONITOR_H
#define RBMS_BATTERY_MONITOR_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 배터리 모니터 초기화
 * @param channel ADC 채널 (분압기 연결)
 */
esp_err_t battery_monitor_init(adc_channel_t channel);

/** @brief 배터리 전압 읽기 (V) */
float battery_monitor_read_voltage(void);

/** @brief 배터리 잔량 (%) — 선형 보간: 3.0V=0%, 4.2V=100% */
float battery_monitor_read_percent(void);

/** @brief 저전압 경고 여부 (3.2V 이하) */
bool battery_monitor_is_low(void);

/** @brief 해제 */
esp_err_t battery_monitor_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_BATTERY_MONITOR_H */
