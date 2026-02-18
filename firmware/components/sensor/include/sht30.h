/**
 * @file sht30.h
 * @brief SHT30 I2C 온습도 센서 드라이버
 */
#ifndef RBMS_SHT30_H
#define RBMS_SHT30_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;  /* 섭씨 (°C) */
    float humidity;     /* 상대습도 (%) */
} sht30_data_t;

/**
 * @brief SHT30 초기화 (I2C 마스터 설정)
 * @param port I2C 포트 번호
 * @param sda_gpio SDA 핀
 * @param scl_gpio SCL 핀
 * @param addr I2C 주소 (보통 0x44 또는 0x45)
 */
esp_err_t sht30_init(i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio, uint8_t addr);

/**
 * @brief 온습도 1회 측정 (Single-Shot, High repeatability)
 * @param[out] data 측정 결과
 */
esp_err_t sht30_read(sht30_data_t *data);

/**
 * @brief SHT30 소프트 리셋
 */
esp_err_t sht30_reset(void);

/**
 * @brief I2C 버스 해제
 */
esp_err_t sht30_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_SHT30_H */
