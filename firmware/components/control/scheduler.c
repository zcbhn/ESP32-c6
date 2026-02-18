/**
 * @file scheduler.c
 * @brief RTC 기반 조명/히터 스케줄러
 */
#include "scheduler.h"
#include "esp_log.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "scheduler";

static light_schedule_t s_sched = {
    .on_hour = 7, .off_hour = 19,
    .sunrise_min = 30, .sunset_min = 30,
};

static bool     s_light_on = false;
static float    s_dimming = 0.0f;

/* 시스템 시간에서 현재 시/분 획득 */
static void get_current_time(uint8_t *hour, uint8_t *minute)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    *hour = (uint8_t)(timeinfo.tm_hour);
    *minute = (uint8_t)(timeinfo.tm_min);
}

esp_err_t scheduler_init(void)
{
    /* 타임존 설정 (KST = UTC+9) */
    setenv("TZ", "KST-9", 1);
    tzset();
    ESP_LOGI(TAG, "Scheduler init: ON=%02d:00, OFF=%02d:00", s_sched.on_hour, s_sched.off_hour);
    return ESP_OK;
}

esp_err_t scheduler_set_light(const light_schedule_t *sched)
{
    if (sched == NULL) return ESP_ERR_INVALID_ARG;
    s_sched = *sched;
    ESP_LOGI(TAG, "Light schedule: ON=%02d:00 OFF=%02d:00 sunrise=%dmin sunset=%dmin",
             sched->on_hour, sched->off_hour, sched->sunrise_min, sched->sunset_min);
    return ESP_OK;
}

esp_err_t scheduler_set_time(uint8_t hour, uint8_t minute)
{
    struct timeval tv;
    struct tm t = {0};
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_year = 126;  /* 2026 */
    t.tm_mon = 0;
    t.tm_mday = 1;
    tv.tv_sec = mktime(&t);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    return ESP_OK;
}

/* now_min이 [on, off) 범위 안에 있는지 판단 (자정 경과 지원) */
static bool is_in_light_period(int now_min, int on_min, int off_min)
{
    if (on_min <= off_min) {
        /* 일반: 07:00~19:00 */
        return (now_min >= on_min && now_min < off_min);
    } else {
        /* 야간: 19:00~07:00 (자정 경과) */
        return (now_min >= on_min || now_min < off_min);
    }
}

/* 자정 경과를 고려한 점등 시작으로부터의 경과 분 계산 */
static int minutes_since_on(int now_min, int on_min)
{
    int diff = now_min - on_min;
    if (diff < 0) diff += 1440;  /* 24h = 1440분 */
    return diff;
}

/* 자정 경과를 고려한 소등까지 남은 분 계산 */
static int minutes_until_off(int now_min, int off_min)
{
    int diff = off_min - now_min;
    if (diff < 0) diff += 1440;
    return diff;
}

void scheduler_tick(void)
{
    /* 시스템 시간에서 현재 시각 획득 */
    uint8_t hour, minute;
    get_current_time(&hour, &minute);

    int now_min = hour * 60 + minute;
    int on_min  = s_sched.on_hour * 60;
    int off_min = s_sched.off_hour * 60;

    if (!is_in_light_period(now_min, on_min, off_min)) {
        /* 소등 시간 */
        s_light_on = false;
        s_dimming = 0.0f;
    } else {
        int elapsed = minutes_since_on(now_min, on_min);
        int remaining = minutes_until_off(now_min, off_min);

        if (s_sched.sunrise_min > 0 && elapsed < s_sched.sunrise_min) {
            /* 일출 전이 (0→1) */
            s_light_on = true;
            s_dimming = (float)elapsed / (float)s_sched.sunrise_min;
        } else if (s_sched.sunset_min > 0 && remaining < s_sched.sunset_min) {
            /* 일몰 전이 (1→0) */
            s_light_on = true;
            s_dimming = (float)remaining / (float)s_sched.sunset_min;
        } else {
            /* 완전 점등 */
            s_light_on = true;
            s_dimming = 1.0f;
        }
    }

    /* clamp */
    if (s_dimming < 0.0f) s_dimming = 0.0f;
    if (s_dimming > 1.0f) s_dimming = 1.0f;
}

bool scheduler_is_light_on(void)
{
    return s_light_on;
}

float scheduler_get_dimming(void)
{
    return s_dimming;
}
