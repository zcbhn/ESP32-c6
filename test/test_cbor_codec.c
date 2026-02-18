/**
 * @file test_cbor_codec.c
 * @brief CBOR encoder/decoder unit tests
 */
#include "unity.h"
#include "cbor_codec.h"
#include <math.h>
#include <string.h>

static uint8_t buf[128];
static size_t out_len;

void setUp(void) {
    memset(buf, 0, sizeof(buf));
    out_len = 0;
}
void tearDown(void) {}

void test_encode_full_report(void)
{
    sensor_report_t report = {
        .temp_hot = 32.5f,
        .temp_cool = 26.0f,
        .humidity = 60.0f,
        .battery_pct = 85.0f,
        .heater_duty = 45.0f,
        .light_duty = 100.0f,
        .safety_status = 0,
    };
    esp_err_t err = cbor_encode_report(&report, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_GREATER_THAN(0, out_len);
    /* Map header should indicate 7 fields */
    TEST_ASSERT_EQUAL(0xA7, buf[0]);  /* CBOR map(7) */
}

void test_encode_type_b_report(void)
{
    /* Type B: no heater, no light, no safety */
    sensor_report_t report = {
        .temp_hot = 30.0f,
        .temp_cool = 25.0f,
        .humidity = 55.0f,
        .battery_pct = 90.0f,
        .heater_duty = -1.0f,
        .light_duty = -1.0f,
        .safety_status = -1,
    };
    esp_err_t err = cbor_encode_report(&report, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    /* Map header should indicate 4 fields */
    TEST_ASSERT_EQUAL(0xA4, buf[0]);  /* CBOR map(4) */
}

void test_encode_decode_roundtrip(void)
{
    sensor_report_t input = {
        .temp_hot = 33.7f,
        .temp_cool = 27.2f,
        .humidity = 65.3f,
        .battery_pct = 72.1f,
        .heater_duty = 88.5f,
        .light_duty = 50.0f,
        .safety_status = 0,
    };

    esp_err_t err = cbor_encode_report(&input, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    sensor_report_t output;
    err = cbor_decode_report(buf, out_len, &output);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.temp_hot, output.temp_hot);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.temp_cool, output.temp_cool);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.humidity, output.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.battery_pct, output.battery_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.heater_duty, output.heater_duty);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, input.light_duty, output.light_duty);
    TEST_ASSERT_EQUAL(input.safety_status, output.safety_status);
}

void test_encode_null_args(void)
{
    sensor_report_t r = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_encode_report(NULL, buf, 64, &out_len));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_encode_report(&r, NULL, 64, &out_len));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_encode_report(&r, buf, 64, NULL));
}

void test_encode_buffer_too_small(void)
{
    sensor_report_t r = {.temp_hot = 1, .temp_cool = 2, .humidity = 3,
                         .battery_pct = -1, .heater_duty = -1,
                         .light_duty = -1, .safety_status = -1};
    uint8_t small_buf[8];
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM,
                      cbor_encode_report(&r, small_buf, sizeof(small_buf), &out_len));
}

void test_decode_null_args(void)
{
    sensor_report_t r;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_decode_report(NULL, 10, &r));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_decode_report(buf, 10, NULL));
}

void test_decode_too_short(void)
{
    sensor_report_t r;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, cbor_decode_report(buf, 1, &r));
}

void test_decode_optional_fields_default(void)
{
    /* Encode minimal report (3 fields) */
    sensor_report_t input = {
        .temp_hot = 30.0f,
        .temp_cool = 25.0f,
        .humidity = 50.0f,
        .battery_pct = -1.0f,
        .heater_duty = -1.0f,
        .light_duty = -1.0f,
        .safety_status = -1,
    };
    cbor_encode_report(&input, buf, sizeof(buf), &out_len);

    sensor_report_t output;
    cbor_decode_report(buf, out_len, &output);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, output.temp_hot);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -1.0f, output.battery_pct);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -1.0f, output.heater_duty);
    TEST_ASSERT_EQUAL(-1, output.safety_status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_full_report);
    RUN_TEST(test_encode_type_b_report);
    RUN_TEST(test_encode_decode_roundtrip);
    RUN_TEST(test_encode_null_args);
    RUN_TEST(test_encode_buffer_too_small);
    RUN_TEST(test_decode_null_args);
    RUN_TEST(test_decode_too_short);
    RUN_TEST(test_decode_optional_fields_default);
    return UNITY_END();
}
