/**
 * @file thread_node.h
 * @brief OpenThread 통신 모듈
 */
#ifndef RBMS_THREAD_NODE_H
#define RBMS_THREAD_NODE_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*thread_rx_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief OpenThread 스택 초기화
 * @param is_router true=Router(Type A), false=SED(Type B)
 */
esp_err_t thread_node_init(bool is_router);

/** @brief Thread 네트워크 시작 */
esp_err_t thread_node_start(void);

/**
 * @brief 데이터 전송 (CoAP UDP)
 * @param data CBOR 인코딩된 페이로드
 * @param len 데이터 길이
 */
esp_err_t thread_node_send(const uint8_t *data, size_t len);

/** @brief 수신 콜백 등록 */
esp_err_t thread_node_set_rx_callback(thread_rx_cb_t cb);

/** @brief SED poll period 설정 (Type B) */
esp_err_t thread_node_set_poll_period(uint32_t period_ms);

/** @brief Thread 네트워크 연결 상태 */
bool thread_node_is_connected(void);

/** @brief Thread 스택 정지 */
esp_err_t thread_node_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_THREAD_NODE_H */
