/**
 * @file pwm_dimmer.h
 * @brief MOSFET PWM LED 디밍 드라이버
 */
#ifndef RBMS_PWM_DIMMER_H
#define RBMS_PWM_DIMMER_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t pwm_dimmer_init(gpio_num_t gpio);
esp_err_t pwm_dimmer_set(uint16_t brightness);        /* 0~1000 */
esp_err_t pwm_dimmer_fade_to(uint16_t target, uint32_t duration_ms);
uint16_t  pwm_dimmer_get(void);
esp_err_t pwm_dimmer_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_PWM_DIMMER_H */
