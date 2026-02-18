/**
 * @file cbor_codec.h
 * @brief CBOR 인코딩/디코딩
 *
 * 경량 CBOR 인코더 내장 (tinycbor 의존성 제거)
 * 센서 데이터를 CBOR Map으로 직렬화
 */
#ifndef RBMS_CBOR_CODEC_H
#define RBMS_CBOR_CODEC_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temp_hot;
    float temp_cool;
    float humidity;
    float battery_pct;    /* 0~100, 음수면 미사용 */
    float heater_duty;    /* 0~100, 음수면 미사용 (Type B) */
    float light_duty;     /* 0~100, 음수면 미사용 (Type B) */
    int   safety_status;  /* safety_status_t, 음수면 미사용 */
} sensor_report_t;

/**
 * @brief 센서 데이터를 CBOR Map으로 인코딩
 * @param report 센서 데이터
 * @param buf 출력 버퍼
 * @param buf_size 버퍼 크기
 * @param[out] out_len 인코딩된 바이트 수
 */
esp_err_t cbor_encode_report(const sensor_report_t *report,
                              uint8_t *buf, size_t buf_size, size_t *out_len);

/**
 * @brief CBOR 데이터 디코딩 (서버 명령 등)
 * @param buf 입력 버퍼
 * @param len 데이터 길이
 * @param[out] report 디코딩 결과
 */
esp_err_t cbor_decode_report(const uint8_t *buf, size_t len, sensor_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_CBOR_CODEC_H */
