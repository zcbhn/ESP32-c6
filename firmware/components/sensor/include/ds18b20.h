/**
 * @file ds18b20.h
 * @brief DS18B20 1-Wire 온도 센서 드라이버
 */
#ifndef RBMS_DS18B20_H
#define RBMS_DS18B20_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS18B20_MAX_SENSORS 2

typedef struct {
    uint8_t rom[8];     /* 64-bit ROM 코드 */
    float   temperature;
    bool    valid;
} ds18b20_sensor_t;

/**
 * @brief DS18B20 초기화
 * @param data_gpio 1-Wire 데이터 핀
 * @param power_gpio 전원 제어 핀 (Type B), GPIO_NUM_NC이면 상시 전원
 */
esp_err_t ds18b20_init(gpio_num_t data_gpio, gpio_num_t power_gpio);

/** @brief 외부 전원 ON (Type B GPIO 스위칭) */
esp_err_t ds18b20_power_on(void);

/** @brief 외부 전원 OFF (Type B GPIO 스위칭) */
esp_err_t ds18b20_power_off(void);

/**
 * @brief 버스 상의 센서 검색
 * @param[out] count 발견된 센서 수
 */
esp_err_t ds18b20_search(int *count);

/**
 * @brief 온도 변환 시작 (모든 센서 동시)
 * 변환 완료까지 약 750ms 대기 필요
 */
esp_err_t ds18b20_start_conversion(void);

/**
 * @brief 센서 온도 읽기
 * @param idx 센서 인덱스 (0 ~ count-1)
 * @param[out] temperature 섭씨 온도
 */
esp_err_t ds18b20_read_temp(int idx, float *temperature);

/**
 * @brief 전체 센서 데이터 가져오기
 */
const ds18b20_sensor_t *ds18b20_get_sensors(void);

/** @brief 드라이버 해제 */
esp_err_t ds18b20_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_DS18B20_H */
