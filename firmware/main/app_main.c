/**
 * @file app_main.c
 * @brief RBMS Firmware Entry Point
 *
 * 노드 타입(Kconfig)에 따라 Type A 또는 Type B 애플리케이션을 시작합니다.
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "RBMS";

/* 외부 선언 - 노드 타입별 엔트리 */
extern void app_type_a_start(void);
extern void app_type_b_start(void);

void app_main(void)
{
    ESP_LOGI(TAG, "=== RBMS Firmware v0.1.0 ===");

    /* NVS 초기화 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 노드 타입에 따라 분기 */
#if defined(CONFIG_NODE_TYPE_A)
    ESP_LOGI(TAG, "Node Type: A (Full Node)");
    app_type_a_start();
#elif defined(CONFIG_NODE_TYPE_B)
    ESP_LOGI(TAG, "Node Type: B (Battery Sensor)");
    app_type_b_start();
#else
    ESP_LOGE(TAG, "No node type selected! Run idf.py menuconfig");
#endif
}
