/**
 * @file ota_update.h
 * @brief OTA Firmware Update via MQTT
 *
 * MQTT 토픽 rbms/<node_id>/ota 로 수신된 펌웨어 청크를
 * OTA 파티션에 기록하고 재부팅.
 *
 * 프로토콜:
 *   1. 서버 → 노드: OTA 시작 명령 (CBOR: {size, sha256, chunk_size})
 *   2. 서버 → 노드: 펌웨어 청크 전송 (순차)
 *   3. 노드: SHA-256 검증 후 OTA 파티션 전환 및 재부팅
 */
#ifndef RBMS_OTA_UPDATE_H
#define RBMS_OTA_UPDATE_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_IN_PROGRESS,
    OTA_STATE_VERIFYING,
    OTA_STATE_COMPLETE,
    OTA_STATE_ERROR,
} ota_state_t;

typedef void (*ota_progress_cb_t)(uint32_t received, uint32_t total);

/**
 * @brief OTA 모듈 초기화
 * @param progress_cb 진행률 콜백 (NULL 가능)
 */
esp_err_t ota_update_init(ota_progress_cb_t progress_cb);

/**
 * @brief OTA 시작 (다음 OTA 파티션에 쓰기 준비)
 * @param firmware_size 전체 펌웨어 크기 (바이트)
 */
esp_err_t ota_update_begin(uint32_t firmware_size);

/**
 * @brief 펌웨어 청크 쓰기
 * @param data 청크 데이터
 * @param len 청크 크기
 */
esp_err_t ota_update_write(const uint8_t *data, size_t len);

/**
 * @brief OTA 완료 및 검증
 * 성공 시 다음 부트 파티션을 OTA 파티션으로 설정
 */
esp_err_t ota_update_finish(void);

/**
 * @brief OTA 중단 및 정리
 */
esp_err_t ota_update_abort(void);

/**
 * @brief 현재 OTA 상태 조회
 */
ota_state_t ota_update_get_state(void);

/**
 * @brief 현재 실행 중인 펌웨어 버전 확인
 */
const char *ota_update_get_version(void);

/**
 * @brief OTA 파티션 전환 후 첫 부팅에서 정상 동작 확인
 *
 * 호출하지 않으면 다음 재부팅 시 이전 펌웨어로 롤백.
 */
esp_err_t ota_update_mark_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_OTA_UPDATE_H */
