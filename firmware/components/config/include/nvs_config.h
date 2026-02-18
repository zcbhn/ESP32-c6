/**
 * @file nvs_config.h
 * @brief NVS 설정 저장/복원
 */
#ifndef RBMS_NVS_CONFIG_H
#define RBMS_NVS_CONFIG_H

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NVS_NAMESPACE "rbms"

esp_err_t nvs_config_init(void);
esp_err_t nvs_config_save_blob(const char *key, const void *data, size_t len);
esp_err_t nvs_config_load_blob(const char *key, void *data, size_t max_len, size_t *out_len);
esp_err_t nvs_config_save_u32(const char *key, uint32_t value);
esp_err_t nvs_config_load_u32(const char *key, uint32_t *value);
esp_err_t nvs_config_erase(const char *key);
esp_err_t nvs_config_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_NVS_CONFIG_H */
