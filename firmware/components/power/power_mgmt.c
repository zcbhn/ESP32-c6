/**
 * @file power_mgmt.c
 * @brief Deep Sleep 전원 관리 (Type B)
 *
 * Deep Sleep 진입 전 등록된 GPIO를 LOW로 고정하여
 * 히터/LED 등이 의도치 않게 켜지는 것을 방지.
 */
#include "power_mgmt.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "power_mgmt";

#define MAX_HOLD_GPIOS 8
static int  s_hold_gpios[MAX_HOLD_GPIOS];
static int  s_hold_count = 0;

esp_err_t power_mgmt_init(void)
{
    s_hold_count = 0;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Wakeup from Deep Sleep (timer)");
    } else {
        ESP_LOGI(TAG, "First boot (cause=%d)", cause);
    }
    return ESP_OK;
}

esp_sleep_wakeup_cause_t power_mgmt_wakeup_cause(void)
{
    return esp_sleep_get_wakeup_cause();
}

bool power_mgmt_is_first_boot(void)
{
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED);
}

esp_err_t power_mgmt_register_hold_gpio(int gpio)
{
    if (s_hold_count >= MAX_HOLD_GPIOS) {
        ESP_LOGE(TAG, "Hold GPIO list full");
        return ESP_ERR_NO_MEM;
    }
    s_hold_gpios[s_hold_count++] = gpio;
    ESP_LOGD(TAG, "Registered hold GPIO%d", gpio);
    return ESP_OK;
}

void power_mgmt_deep_sleep(uint32_t sleep_sec)
{
    ESP_LOGI(TAG, "Entering Deep Sleep for %lu seconds", (unsigned long)sleep_sec);

    /* 등록된 GPIO를 LOW로 설정 후 hold */
    for (int i = 0; i < s_hold_count; i++) {
        gpio_set_level((gpio_num_t)s_hold_gpios[i], 0);
        gpio_hold_en((gpio_num_t)s_hold_gpios[i]);
    }
    if (s_hold_count > 0) {
        ESP_LOGI(TAG, "GPIO hold enabled for %d pins", s_hold_count);
    }

    /* 미사용 RTC 도메인 전원 차단 (전류 소모 최소화) */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

    esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);
    esp_deep_sleep_start();
    /* 이 아래는 실행되지 않음 */
}
