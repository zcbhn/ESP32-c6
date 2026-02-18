/**
 * @file thread_node.c
 * @brief OpenThread 통신 모듈
 *
 * ESP-IDF의 openthread 컴포넌트를 사용하여
 * Thread 네트워크 초기화, 데이터 송수신을 수행.
 *
 * 주의: FreeRTOS 태스크에서 OT API 호출 시
 * esp_openthread_lock_acquire/release 필수.
 */
#include "thread_node.h"

#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_types.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_vfs_eventfd.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/udp.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "thread_node";

static bool s_is_router = false;
static volatile bool s_connected = false;
static volatile thread_rx_cb_t s_rx_cb = NULL;
static otInstance *s_instance = NULL;
static otUdpSocket s_socket;
static esp_netif_t *s_netif = NULL;

#define THREAD_UDP_PORT 5684

/* ESP32-C6 네이티브 802.15.4 라디오 설정 */
static const esp_openthread_platform_config_t s_platform_config = {
    .radio_config = {
        .radio_mode = RADIO_MODE_NATIVE,
    },
    .host_config = {
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,
    },
    .port_config = {
        .storage_partition_name = "nvs",
        .netif_queue_size = 10,
        .task_queue_size = 10,
    },
};

/* OT 콜백 컨텍스트에서 호출 — lock 불필요 */
static void thread_state_changed_cb(otChangedFlags flags, void *context)
{
    if (flags & OT_CHANGED_THREAD_ROLE) {
        otDeviceRole role = otThreadGetDeviceRole(s_instance);
        switch (role) {
            case OT_DEVICE_ROLE_LEADER:
            case OT_DEVICE_ROLE_ROUTER:
            case OT_DEVICE_ROLE_CHILD:
                s_connected = true;
                ESP_LOGI(TAG, "Thread attached, role=%d", role);
                break;
            default:
                s_connected = false;
                ESP_LOGW(TAG, "Thread detached, role=%d", role);
                break;
        }
    }
}

/* OT 콜백 컨텍스트에서 호출 — lock 불필요 */
static void udp_receive_cb(void *context, otMessage *message,
                            const otMessageInfo *message_info)
{
    uint16_t len = otMessageGetLength(message) - otMessageGetOffset(message);
    if (len > 512) len = 512;

    uint8_t buf[512];
    otMessageRead(message, otMessageGetOffset(message), buf, len);

    if (s_rx_cb) {
        s_rx_cb(buf, len);
    }
}

static void thread_main_task(void *param)
{
    esp_openthread_launch_mainloop();
    /* mainloop exited */
    esp_openthread_netif_glue_deinit();
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    vTaskDelete(NULL);
}

esp_err_t thread_node_init(bool is_router)
{
    s_is_router = is_router;

    /* eventfd 초기화 (OpenThread에 필요) */
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };
    esp_vfs_eventfd_register(&eventfd_config);  /* 재호출 시 무시 */

    /* esp_netif 초기화 (재호출 안전) */
    esp_netif_init();
    esp_event_loop_create_default();  /* ESP_ERR_INVALID_STATE는 정상 (이미 존재) */

    /* OpenThread 스택 초기화 */
    esp_err_t ret = esp_openthread_init(&s_platform_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OpenThread init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_instance = esp_openthread_get_instance();

    /* OpenThread netif 바인딩 */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    s_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_attach(s_netif, esp_openthread_netif_glue_init(&s_platform_config)));

    /* 상태 변경 콜백 등록 (mainloop 시작 전이므로 lock 불필요) */
    otSetStateChangedCallback(s_instance, thread_state_changed_cb, NULL);

    /* UDP 소켓 열기 (mainloop 시작 전이므로 lock 불필요) */
    otUdpOpen(s_instance, &s_socket, udp_receive_cb, NULL);
    otSockAddr bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.mPort = THREAD_UDP_PORT;
    otUdpBind(s_instance, &s_socket, &bind_addr, OT_NETIF_THREAD);

    ESP_LOGI(TAG, "Thread node init: %s", is_router ? "Router" : "SED");
    return ESP_OK;
}

esp_err_t thread_node_start(void)
{
    if (s_instance == NULL) return ESP_ERR_INVALID_STATE;

    /* mainloop 시작 전이므로 lock 불필요 */
    otOperationalDataset dataset;
    if (otDatasetGetActive(s_instance, &dataset) != OT_ERROR_NONE) {
        ESP_LOGI(TAG, "No active dataset, creating default...");
        otDatasetCreateNewNetwork(s_instance, &dataset);
        otDatasetSetActive(s_instance, &dataset);
    }

    otThreadSetEnabled(s_instance, true);

    if (!s_is_router) {
        otLinkModeConfig mode = {0};
        mode.mRxOnWhenIdle = false;
        mode.mNetworkData = false;
        otThreadSetLinkMode(s_instance, mode);

        /* Child timeout: 라우터가 이 시간 내 응답 없으면 자식 제거 */
        otThreadSetChildTimeout(s_instance, 300);  /* 5분 (최대 sleep 간격 이상) */
    }

    /* 메인 루프 태스크 시작 — 이후부터 외부 호출은 lock 필수 */
    xTaskCreate(thread_main_task, "ot_main", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Thread started");
    return ESP_OK;
}

esp_err_t thread_node_send(const uint8_t *data, size_t len)
{
    if (s_instance == NULL || data == NULL) return ESP_ERR_INVALID_ARG;

    /* 외부 태스크에서 호출 — lock 필수 */
    if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
        ESP_LOGW(TAG, "Failed to acquire OT lock for send");
        return ESP_ERR_TIMEOUT;
    }

    otMessage *msg = otUdpNewMessage(s_instance, NULL);
    if (msg == NULL) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "Failed to allocate UDP message");
        return ESP_ERR_NO_MEM;
    }

    if (otMessageAppend(msg, data, (uint16_t)len) != OT_ERROR_NONE) {
        otMessageFree(msg);
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "Failed to append UDP message data");
        return ESP_ERR_NO_MEM;
    }

    otMessageInfo info;
    memset(&info, 0, sizeof(info));
    otIp6AddressFromString("ff03::1", &info.mPeerAddr);
    info.mPeerPort = THREAD_UDP_PORT;

    otError err = otUdpSend(s_instance, &s_socket, msg, &info);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "UDP send failed: %d", err);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sent %d bytes via Thread UDP", (int)len);
    return ESP_OK;
}

esp_err_t thread_node_set_rx_callback(thread_rx_cb_t cb)
{
    s_rx_cb = cb;
    return ESP_OK;
}

esp_err_t thread_node_set_poll_period(uint32_t period_ms)
{
    if (s_instance == NULL) return ESP_ERR_INVALID_STATE;

    if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
        return ESP_ERR_TIMEOUT;
    }
    otLinkSetPollPeriod(s_instance, period_ms);
    esp_openthread_lock_release();

    ESP_LOGD(TAG, "SED poll period = %lums", (unsigned long)period_ms);
    return ESP_OK;
}

bool thread_node_is_connected(void)
{
    return s_connected;
}

esp_err_t thread_node_stop(void)
{
    if (s_instance == NULL) return ESP_OK;

    if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
        return ESP_ERR_TIMEOUT;
    }
    otThreadSetEnabled(s_instance, false);
    otUdpClose(s_instance, &s_socket);
    esp_openthread_lock_release();

    esp_openthread_deinit();
    s_instance = NULL;
    s_connected = false;
    ESP_LOGI(TAG, "Thread stopped");
    return ESP_OK;
}
