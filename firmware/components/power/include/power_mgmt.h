/**
 * @file power_mgmt.h
 * @brief Deep Sleep 전원 관리 (Type B)
 */
#ifndef RBMS_POWER_MGMT_H
#define RBMS_POWER_MGMT_H

#include "esp_err.h"
#include "esp_sleep.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 전원 관리 초기화 */
esp_err_t power_mgmt_init(void);

/** @brief 마지막 wake-up 원인 반환 */
esp_sleep_wakeup_cause_t power_mgmt_wakeup_cause(void);

/** @brief 첫 부팅인지 (Deep Sleep 아닌) 확인 */
bool power_mgmt_is_first_boot(void);

/**
 * @brief Deep Sleep 중 LOW로 유지할 GPIO 등록
 * @param gpio 등록할 GPIO 핀 번호
 */
esp_err_t power_mgmt_register_hold_gpio(int gpio);

/**
 * @brief Deep Sleep 진입
 * @param sleep_sec 수면 시간 (초)
 */
void power_mgmt_deep_sleep(uint32_t sleep_sec);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_POWER_MGMT_H */
