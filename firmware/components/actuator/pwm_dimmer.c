/**
 * @file pwm_dimmer.c
 * @brief MOSFET PWM LED 디밍 드라이버 (LEDC)
 */
#include "pwm_dimmer.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "pwm_dimmer";

#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_FREQUENCY   1000
#define BRIGHTNESS_MAX   1000

static bool s_initialized = false;
static uint16_t s_brightness = 0;

static uint32_t brightness_to_duty(uint16_t b)
{
    if (b >= BRIGHTNESS_MAX) return 1023;
    return (uint32_t)b * 1023 / BRIGHTNESS_MAX;
}

esp_err_t pwm_dimmer_init(gpio_num_t gpio)
{
    ledc_timer_config_t tc = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_USE_XTAL_CLK,
    };
    esp_err_t ret = ledc_timer_config(&tc);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t cc = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .gpio_num   = gpio,
        .duty       = 0,
        .hpoint     = 0,
    };
    ret = ledc_channel_config(&cc);
    if (ret != ESP_OK) return ret;

    ledc_fade_func_install(0);
    s_brightness = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "PWM dimmer init GPIO%d", gpio);
    return ESP_OK;
}

esp_err_t pwm_dimmer_set(uint16_t brightness)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (brightness > BRIGHTNESS_MAX) brightness = BRIGHTNESS_MAX;

    uint32_t duty = brightness_to_duty(brightness);
    esp_err_t ret = ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty, 0);
    if (ret == ESP_OK) s_brightness = brightness;
    return ret;
}

esp_err_t pwm_dimmer_fade_to(uint16_t target, uint32_t duration_ms)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (target > BRIGHTNESS_MAX) target = BRIGHTNESS_MAX;

    uint32_t duty = brightness_to_duty(target);
    esp_err_t ret = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL,
                                             duty, duration_ms);
    if (ret == ESP_OK) {
        ret = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
    }
    if (ret == ESP_OK) s_brightness = target;
    return ret;
}

uint16_t pwm_dimmer_get(void)
{
    return s_brightness;
}

esp_err_t pwm_dimmer_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    ledc_fade_func_uninstall();
    s_initialized = false;
    return ESP_OK;
}
