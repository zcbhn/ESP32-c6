/**
 * @file ssr.c
 * @brief SSR Cycle Skipping 히터/조명 제어
 *
 * 10초 주기(100 ticks x 100ms)에서 duty%만큼 ON.
 */
#include "ssr.h"
#include "esp_log.h"

static const char *TAG = "ssr";

static ssr_channel_t s_ch[SSR_MAX_CHANNELS];
static bool s_ch_inited[SSR_MAX_CHANNELS] = {false, false};

esp_err_t ssr_init(int channel, gpio_num_t gpio, const char *name)
{
    if (channel < 0 || channel >= SSR_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&conf);
    if (ret != ESP_OK) return ret;

    gpio_set_level(gpio, 0);

    s_ch[channel].gpio = gpio;
    s_ch[channel].name = name;
    s_ch[channel].duty = 0;
    s_ch[channel].enabled = true;
    s_ch_inited[channel] = true;

    ESP_LOGI(TAG, "SSR[%d] '%s' init GPIO%d", channel, name, gpio);
    return ESP_OK;
}

esp_err_t ssr_set_duty(int channel, uint8_t duty_percent)
{
    if (channel < 0 || channel >= SSR_MAX_CHANNELS || !s_ch_inited[channel]) {
        return ESP_ERR_INVALID_ARG;
    }
    if (duty_percent > 100) duty_percent = 100;
    s_ch[channel].duty = duty_percent;
    return ESP_OK;
}

uint8_t ssr_get_duty(int channel)
{
    if (channel < 0 || channel >= SSR_MAX_CHANNELS || !s_ch_inited[channel]) {
        return 0;
    }
    return s_ch[channel].duty;
}

esp_err_t ssr_force_off(int channel)
{
    if (channel < 0 || channel >= SSR_MAX_CHANNELS || !s_ch_inited[channel]) {
        return ESP_ERR_INVALID_ARG;
    }
    s_ch[channel].duty = 0;
    s_ch[channel].enabled = false;
    gpio_set_level(s_ch[channel].gpio, 0);
    ESP_LOGW(TAG, "SSR[%d] '%s' FORCE OFF", channel, s_ch[channel].name);
    return ESP_OK;
}

esp_err_t ssr_force_off_all(void)
{
    for (int i = 0; i < SSR_MAX_CHANNELS; i++) {
        if (s_ch_inited[i]) ssr_force_off(i);
    }
    return ESP_OK;
}

void ssr_tick(int tick_100ms)
{
    for (int i = 0; i < SSR_MAX_CHANNELS; i++) {
        if (!s_ch_inited[i] || !s_ch[i].enabled) continue;
        bool on = (tick_100ms < (int)s_ch[i].duty);
        gpio_set_level(s_ch[i].gpio, on ? 1 : 0);
    }
}
