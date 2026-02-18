/**
 * @file cbor_codec.c
 * @brief 경량 CBOR 인코더/디코더 (외부 라이브러리 불필요)
 *
 * CBOR 정수 키 매핑 (IoT 효율):
 *   1: temp_hot, 2: temp_cool, 3: humidity, 4: battery_v,
 *   5: heater_duty, 6: light_duty, 7: safety_status
 *
 * 서버 bridge (mqtt_influx_bridge.py)의 FIELD_MAP과 동일.
 */
#include "cbor_codec.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "cbor_codec";

/* CBOR Major Types */
#define CBOR_UINT    (0 << 5)
#define CBOR_NEGINT  (1 << 5)
#define CBOR_MAP     (5 << 5)
#define CBOR_FLOAT32 0xFA

/* CBOR 정수 키 (서버 bridge와 동일) */
#define KEY_TEMP_HOT     1
#define KEY_TEMP_COOL    2
#define KEY_HUMIDITY     3
#define KEY_BATTERY      4
#define KEY_HEATER_DUTY  5
#define KEY_LIGHT_DUTY   6
#define KEY_SAFETY       7

static size_t cbor_write_uint(uint8_t *buf, uint8_t major, uint32_t val)
{
    if (val < 24) {
        buf[0] = major | (uint8_t)val;
        return 1;
    } else if (val <= 0xFF) {
        buf[0] = major | 24;
        buf[1] = (uint8_t)val;
        return 2;
    } else {
        buf[0] = major | 25;
        buf[1] = (val >> 8) & 0xFF;
        buf[2] = val & 0xFF;
        return 3;
    }
}

static size_t cbor_write_float(uint8_t *buf, float val)
{
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    buf[0] = CBOR_FLOAT32;
    buf[1] = (bits >> 24) & 0xFF;
    buf[2] = (bits >> 16) & 0xFF;
    buf[3] = (bits >> 8) & 0xFF;
    buf[4] = bits & 0xFF;
    return 5;
}

esp_err_t cbor_encode_report(const sensor_report_t *report,
                              uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (report == NULL || buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 필드 수 계산 */
    int field_count = 3;  /* temp_hot, temp_cool, humidity (항상) */
    if (report->battery_pct >= 0.0f) field_count++;
    if (report->heater_duty >= 0.0f) field_count++;
    if (report->light_duty >= 0.0f)  field_count++;
    if (report->safety_status >= 0)  field_count++;

    /* 최소 버퍼: 1(map) + 7*(1+5) = 43, 64이면 충분 */
    if (buf_size < 64) {
        return ESP_ERR_NO_MEM;
    }

    size_t pos = 0;

    /* Map 헤더 */
    buf[pos++] = CBOR_MAP | (uint8_t)field_count;

    /* 1: temp_hot */
    pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_TEMP_HOT);
    pos += cbor_write_float(buf + pos, report->temp_hot);

    /* 2: temp_cool */
    pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_TEMP_COOL);
    pos += cbor_write_float(buf + pos, report->temp_cool);

    /* 3: humidity */
    pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_HUMIDITY);
    pos += cbor_write_float(buf + pos, report->humidity);

    /* 4: battery (옵션) */
    if (report->battery_pct >= 0.0f) {
        pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_BATTERY);
        pos += cbor_write_float(buf + pos, report->battery_pct);
    }

    /* 5: heater_duty (옵션, Type A) */
    if (report->heater_duty >= 0.0f) {
        pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_HEATER_DUTY);
        pos += cbor_write_float(buf + pos, report->heater_duty);
    }

    /* 6: light_duty (옵션, Type A) */
    if (report->light_duty >= 0.0f) {
        pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_LIGHT_DUTY);
        pos += cbor_write_float(buf + pos, report->light_duty);
    }

    /* 7: safety (옵션) */
    if (report->safety_status >= 0) {
        pos += cbor_write_uint(buf + pos, CBOR_UINT, KEY_SAFETY);
        pos += cbor_write_uint(buf + pos, CBOR_UINT, (uint32_t)report->safety_status);
    }

    *out_len = pos;
    ESP_LOGD(TAG, "Encoded %d bytes (%d fields)", (int)pos, field_count);
    return ESP_OK;
}

esp_err_t cbor_decode_report(const uint8_t *buf, size_t len, sensor_report_t *report)
{
    if (buf == NULL || report == NULL || len < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 기본값 */
    report->temp_hot = 0;
    report->temp_cool = 0;
    report->humidity = 0;
    report->battery_pct = -1.0f;
    report->heater_duty = -1.0f;
    report->light_duty = -1.0f;
    report->safety_status = -1;

    size_t pos = 0;
    int map_count = buf[pos] & 0x1F;
    pos++;

    for (int i = 0; i < map_count && pos < len; i++) {
        /* 정수 키 파싱 */
        uint32_t key = 0;
        if ((buf[pos] & 0xE0) == CBOR_UINT) {
            uint8_t ai = buf[pos] & 0x1F;
            pos++;
            if (ai < 24) {
                key = ai;
            } else if (ai == 24 && pos < len) {
                key = buf[pos++];
            }
        } else {
            break;  /* 예상치 못한 타입 */
        }

        if (pos >= len) break;

        /* 값 파싱: float32 또는 uint */
        float fval = 0;
        if (buf[pos] == CBOR_FLOAT32 && pos + 5 <= len) {
            pos++;
            uint32_t bits = ((uint32_t)buf[pos] << 24) |
                            ((uint32_t)buf[pos+1] << 16) |
                            ((uint32_t)buf[pos+2] << 8) |
                            (uint32_t)buf[pos+3];
            memcpy(&fval, &bits, sizeof(fval));
            pos += 4;
        } else if ((buf[pos] & 0xE0) == CBOR_UINT) {
            uint8_t ai = buf[pos] & 0x1F;
            pos++;
            if (ai < 24) {
                fval = (float)ai;
            } else if (ai == 24 && pos < len) {
                fval = (float)buf[pos++];
            }
        } else {
            break;
        }

        switch (key) {
            case KEY_TEMP_HOT:    report->temp_hot = fval; break;
            case KEY_TEMP_COOL:   report->temp_cool = fval; break;
            case KEY_HUMIDITY:    report->humidity = fval; break;
            case KEY_BATTERY:     report->battery_pct = fval; break;
            case KEY_HEATER_DUTY: report->heater_duty = fval; break;
            case KEY_LIGHT_DUTY:  report->light_duty = fval; break;
            case KEY_SAFETY:      report->safety_status = (int)fval; break;
            default: break;
        }
    }

    return ESP_OK;
}
