/**
 * @file nvs_config.c
 * @brief NVS 설정 저장/복원
 */
#include "nvs_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "nvs_config";

static nvs_handle_t s_handle;
static bool s_opened = false;

esp_err_t nvs_config_init(void)
{
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_opened = true;
    ESP_LOGI(TAG, "NVS config initialized (ns=%s)", NVS_NAMESPACE);
    return ESP_OK;
}

esp_err_t nvs_config_save_blob(const char *key, const void *data, size_t len)
{
    if (!s_opened) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = nvs_set_blob(s_handle, key, data, len);
    if (ret == ESP_OK) ret = nvs_commit(s_handle);
    return ret;
}

esp_err_t nvs_config_load_blob(const char *key, void *data, size_t max_len, size_t *out_len)
{
    if (!s_opened) return ESP_ERR_INVALID_STATE;
    size_t len = max_len;
    esp_err_t ret = nvs_get_blob(s_handle, key, data, &len);
    if (ret == ESP_OK && out_len) *out_len = len;
    return ret;
}

esp_err_t nvs_config_save_u32(const char *key, uint32_t value)
{
    if (!s_opened) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = nvs_set_u32(s_handle, key, value);
    if (ret == ESP_OK) ret = nvs_commit(s_handle);
    return ret;
}

esp_err_t nvs_config_load_u32(const char *key, uint32_t *value)
{
    if (!s_opened) return ESP_ERR_INVALID_STATE;
    return nvs_get_u32(s_handle, key, value);
}

esp_err_t nvs_config_erase(const char *key)
{
    if (!s_opened) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = nvs_erase_key(s_handle, key);
    if (ret == ESP_OK) ret = nvs_commit(s_handle);
    return ret;
}

esp_err_t nvs_config_deinit(void)
{
    if (s_opened) {
        nvs_close(s_handle);
        s_opened = false;
        ESP_LOGI(TAG, "NVS config closed");
    }
    return ESP_OK;
}
