/**
 * @file ssr.h
 * @brief SSR Cycle Skipping 히터/조명 제어
 */
#ifndef RBMS_SSR_H
#define RBMS_SSR_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSR_MAX_CHANNELS 2

typedef struct {
    gpio_num_t gpio;
    const char *name;
    uint8_t    duty;     /* 0-100 (%) */
    bool       enabled;
} ssr_channel_t;

esp_err_t ssr_init(int channel, gpio_num_t gpio, const char *name);
esp_err_t ssr_set_duty(int channel, uint8_t duty_percent);
uint8_t   ssr_get_duty(int channel);
esp_err_t ssr_force_off(int channel);
esp_err_t ssr_force_off_all(void);

/**
 * @brief Cycle Skipping 업데이트 (100ms 주기 호출)
 * @param tick_100ms 0~99 카운터 (10초 주기)
 */
void ssr_tick(int tick_100ms);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_SSR_H */
