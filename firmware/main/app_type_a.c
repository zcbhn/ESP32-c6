/**
 * @file app_type_a.c
 * @brief Type A 풀 노드 애플리케이션
 *
 * 콘센트 전원, Thread Router, FreeRTOS 태스크 기반
 */

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#include "sht30.h"
#include "ds18b20.h"
#include "ssr.h"
#include "pwm_dimmer.h"
#include "pid.h"
#include "scheduler.h"
#include "safety_monitor.h"
#include "thread_node.h"
#include "cbor_codec.h"
#include "nvs_config.h"
#include "preset_manager.h"

static const char *TAG = "APP_A";

/* 공유 데이터 (태스크 간) — volatile로 컴파일러 최적화 방지 */
static volatile float s_temp_hot  = 0.0f;
static volatile float s_temp_cool = 0.0f;
static volatile float s_humidity  = 0.0f;
static pid_ctrl_t s_pid;
static preset_t s_preset;
static volatile safety_status_t s_safety = SAFETY_OK;
static uint32_t s_ssr_tick_counter = 0;

/* --- 태스크: 센서 읽기 (1초) --- */
static void sensor_task(void *param)
{
    esp_task_wdt_add(NULL);

    int ds_count = 0;
    ds18b20_search(&ds_count);

    while (1) {
        esp_task_wdt_reset();

        /* SHT30 온습도 */
        sht30_data_t sht;
        if (sht30_read(&sht) == ESP_OK) {
            s_humidity = sht.humidity;
        }

        /* DS18B20 온도 (핫존/쿨존) */
        ds18b20_start_conversion();
        vTaskDelay(pdMS_TO_TICKS(750));

        float th = 0, tc = 0;
        if (ds_count >= 1) ds18b20_read_temp(0, &th);
        if (ds_count >= 2) ds18b20_read_temp(1, &tc);
        s_temp_hot = th;
        s_temp_cool = tc;

        ESP_LOGI(TAG, "T_hot=%.1f T_cool=%.1f H=%.1f%%",
                 th, tc, (float)s_humidity);

        vTaskDelay(pdMS_TO_TICKS(250));  /* 총 ~1초 */
    }
}

/* --- 태스크: PID 제어 + SSR 출력 (1초) --- */
static void control_task(void *param)
{
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        if (s_safety >= SAFETY_FAULT_OVERTEMP) {
            /* 안전 이상 시 출력 차단 */
            ssr_force_off_all();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        float output = pid_compute(&s_pid, s_temp_hot, 1.0f);
        if (isnanf(output) || output < 0.0f) output = 0.0f;
        if (output > 100.0f) output = 100.0f;
        ssr_set_duty(0, (uint8_t)output);  /* 히터 */

        /* 스케줄러 기반 조명 */
        scheduler_tick();
        if (scheduler_is_light_on()) {
            float dim = scheduler_get_dimming();
            pwm_dimmer_set((uint16_t)(dim * 1000.0f));
        } else {
            pwm_dimmer_set(0);
        }

        /* SSR Cycle Skipping tick (10초 = 100 ticks @ 100ms) */
        ssr_tick((int)(s_ssr_tick_counter % 100));
        s_ssr_tick_counter++;
        if (s_ssr_tick_counter >= 10000) s_ssr_tick_counter = 0;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* --- 태스크: 안전 감시 (1초) --- */
static void safety_task(void *param)
{
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        s_safety = safety_check(s_temp_hot, s_temp_cool,
                                 s_preset.temp_hot.target,
                                 s_humidity);

        if (s_safety >= SAFETY_FAULT_OVERTEMP) {
            ESP_LOGE(TAG, "SAFETY FAULT: %s", safety_status_str(s_safety));
            safety_emergency_shutdown();
            ssr_force_off_all();
            pwm_dimmer_set(0);
        }

        bool heater_on = (ssr_get_duty(0) > 0);
        safety_heater_tick(heater_on);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* --- 태스크: Thread 리포트 (10초) --- */
static void thread_task(void *param)
{
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        if (thread_node_is_connected()) {
            sensor_report_t report = {
                .temp_hot = s_temp_hot,
                .temp_cool = s_temp_cool,
                .humidity = s_humidity,
                .battery_pct = -1.0f,  /* Type A: 배터리 없음 */
                .heater_duty = (float)ssr_get_duty(0),
                .light_duty  = (float)pwm_dimmer_get() / 10.0f,  /* 0-1000 → 0-100 */
                .safety_status = (int)s_safety,
            };
            uint8_t buf[64];
            size_t len = 0;
            if (cbor_encode_report(&report, buf, sizeof(buf), &len) == ESP_OK) {
                thread_node_send(buf, len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* --- 엔트리 포인트 --- */
void app_type_a_start(void)
{
    ESP_LOGI(TAG, "Type A application starting...");

    /* 설정 로드 */
    nvs_config_init();
    preset_load(&s_preset);

    /* 센서 초기화 */
    sht30_init(I2C_NUM_0, GPIO_NUM_6, GPIO_NUM_7, CONFIG_SENSOR_SHT30_ADDR);
    ds18b20_init(CONFIG_SENSOR_DS18B20_GPIO, GPIO_NUM_NC);  /* Type A: 상시 전원 */

    /* 액추에이터 초기화 */
    ssr_init(0, CONFIG_SSR_HEATER_GPIO, "heater");
    ssr_init(1, CONFIG_SSR_LIGHT_GPIO, "uv_light");
    pwm_dimmer_init(CONFIG_PWM_DIMMING_GPIO);

    /* PID 초기화 */
    pid_init(&s_pid, s_preset.pid.kp, s_preset.pid.ki, s_preset.pid.kd);
    pid_set_setpoint(&s_pid, s_preset.temp_hot.target);
    pid_set_limits(&s_pid, 0.0f, 100.0f);

    /* 스케줄러 초기화 */
    scheduler_init();
    light_schedule_t lsched = {
        .on_hour = s_preset.light.on_hour,
        .off_hour = s_preset.light.off_hour,
        .sunrise_min = s_preset.light.sunrise_min,
        .sunset_min = s_preset.light.sunset_min,
    };
    scheduler_set_light(&lsched);

    /* 안전 감시 초기화 */
    safety_config_t scfg = {
        .overtemp_offset   = s_preset.safety.overtemp_offset,
        .heater_max_sec    = s_preset.safety.heater_max_sec,
        .stale_timeout_sec = 30,
        .sensor_mismatch_c = 15.0f,
        .rate_max_per_sec  = 2.0f,
    };
    safety_init(&scfg);

    /* Thread 초기화 (Router) */
    thread_node_init(true);
    thread_node_start();

    /* FreeRTOS 태스크 생성 (safety > sensor > control > thread 우선순위) */
    BaseType_t ret;
    ret = xTaskCreate(sensor_task,  "sensor",  4096, NULL, 5, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create sensor task");
    ret = xTaskCreate(control_task, "control", 4096, NULL, 4, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create control task");
    ret = xTaskCreate(safety_task,  "safety",  4096, NULL, 6, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create safety task");
    ret = xTaskCreate(thread_task,  "thread",  4096, NULL, 3, NULL);
    if (ret != pdPASS) ESP_LOGE(TAG, "Failed to create thread task");

    ESP_LOGI(TAG, "Type A running — %s preset", s_preset.species);
}
