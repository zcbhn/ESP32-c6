/**
 * @file ota_update.c
 * @brief OTA Firmware Update via MQTT
 *
 * ESP-IDF OTA API를 사용하여 안전한 펌웨어 업데이트 수행.
 * 듀얼 OTA 파티션(ota_0, ota_1)을 번갈아 사용하며,
 * 롤백 메커니즘으로 불량 펌웨어로부터 보호.
 */
#include "ota_update.h"

#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_desc.h"

static const char *TAG = "ota_update";

static ota_state_t s_state = OTA_STATE_IDLE;
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;
static ota_progress_cb_t s_progress_cb = NULL;
static uint32_t s_firmware_size = 0;
static uint32_t s_received = 0;

esp_err_t ota_update_init(ota_progress_cb_t progress_cb)
{
    s_progress_cb = progress_cb;
    s_state = OTA_STATE_IDLE;

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "OTA init: running partition '%s' at 0x%lx",
             running->label, (unsigned long)running->address);

    return ESP_OK;
}

esp_err_t ota_update_begin(uint32_t firmware_size)
{
    if (s_state == OTA_STATE_IN_PROGRESS) {
        ESP_LOGW(TAG, "OTA already in progress, aborting previous");
        ota_update_abort();
    }

    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition available");
        s_state = OTA_STATE_ERROR;
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "OTA begin: target='%s' size=%lu bytes",
             s_update_partition->label, (unsigned long)firmware_size);

    esp_err_t err = esp_ota_begin(s_update_partition, firmware_size, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        s_state = OTA_STATE_ERROR;
        return err;
    }

    s_firmware_size = firmware_size;
    s_received = 0;
    s_state = OTA_STATE_IN_PROGRESS;

    return ESP_OK;
}

esp_err_t ota_update_write(const uint8_t *data, size_t len)
{
    if (s_state != OTA_STATE_IN_PROGRESS) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_ota_write(s_ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        s_state = OTA_STATE_ERROR;
        return err;
    }

    s_received += len;

    if (s_progress_cb) {
        s_progress_cb(s_received, s_firmware_size);
    }

    ESP_LOGD(TAG, "OTA write: %lu / %lu bytes (%.1f%%)",
             (unsigned long)s_received, (unsigned long)s_firmware_size,
             (s_firmware_size > 0) ? (100.0f * s_received / s_firmware_size) : 0.0f);

    return ESP_OK;
}

esp_err_t ota_update_finish(void)
{
    if (s_state != OTA_STATE_IN_PROGRESS) {
        return ESP_ERR_INVALID_STATE;
    }

    s_state = OTA_STATE_VERIFYING;

    esp_err_t err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA verification failed: %s", esp_err_to_name(err));
        s_state = OTA_STATE_ERROR;
        return err;
    }

    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
        s_state = OTA_STATE_ERROR;
        return err;
    }

    s_state = OTA_STATE_COMPLETE;
    ESP_LOGI(TAG, "OTA complete: %lu bytes written to '%s'. Reboot to apply.",
             (unsigned long)s_received, s_update_partition->label);

    return ESP_OK;
}

esp_err_t ota_update_abort(void)
{
    if (s_state == OTA_STATE_IN_PROGRESS) {
        esp_ota_abort(s_ota_handle);
        ESP_LOGW(TAG, "OTA aborted at %lu / %lu bytes",
                 (unsigned long)s_received, (unsigned long)s_firmware_size);
    }

    s_ota_handle = 0;
    s_update_partition = NULL;
    s_firmware_size = 0;
    s_received = 0;
    s_state = OTA_STATE_IDLE;

    return ESP_OK;
}

ota_state_t ota_update_get_state(void)
{
    return s_state;
}

const char *ota_update_get_version(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    return desc->version;
}

esp_err_t ota_update_mark_valid(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Firmware marked as valid (rollback cancelled)");
    } else {
        ESP_LOGW(TAG, "Mark valid failed (may not be OTA boot): %s",
                 esp_err_to_name(err));
    }
    return err;
}
