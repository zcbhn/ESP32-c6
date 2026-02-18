/**
 * @file test_adaptive_poll.c
 * @brief Adaptive polling period unit tests
 */
#include "unity.h"
#include "adaptive_poll.h"
#include <math.h>

void setUp(void)
{
    adaptive_poll_config_t cfg = {
        .period_fast_s = 30,
        .period_slow_s = 300,
        .delta_high = 1.0f,
    };
    adaptive_poll_init(&cfg);
}

void tearDown(void) {}

void test_no_change_returns_slow(void)
{
    uint32_t period = adaptive_poll_calc(25.0f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(300, period);
}

void test_large_change_returns_fast(void)
{
    uint32_t period = adaptive_poll_calc(26.0f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(30, period);
}

void test_very_large_change_returns_fast(void)
{
    uint32_t period = adaptive_poll_calc(30.0f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(30, period);
}

void test_half_delta_returns_middle(void)
{
    /* delta=0.5, ratio=0.5, range=270, period=300-135=165 */
    uint32_t period = adaptive_poll_calc(25.5f, 25.0f);
    TEST_ASSERT_UINT32_WITHIN(5, 165, period);
}

void test_negative_delta_same_as_positive(void)
{
    uint32_t p1 = adaptive_poll_calc(25.5f, 25.0f);
    uint32_t p2 = adaptive_poll_calc(24.5f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(p1, p2);
}

void test_default_config(void)
{
    adaptive_poll_init(NULL);  /* Use defaults */
    uint32_t period = adaptive_poll_calc(25.0f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(300, period);
}

void test_custom_config(void)
{
    adaptive_poll_config_t cfg = {
        .period_fast_s = 10,
        .period_slow_s = 600,
        .delta_high = 2.0f,
    };
    adaptive_poll_init(&cfg);

    /* No change -> slow */
    TEST_ASSERT_EQUAL_UINT32(600, adaptive_poll_calc(25.0f, 25.0f));
    /* delta >= 2.0 -> fast */
    TEST_ASSERT_EQUAL_UINT32(10, adaptive_poll_calc(27.0f, 25.0f));
}

void test_zero_delta_high_returns_fast(void)
{
    adaptive_poll_config_t cfg = {
        .period_fast_s = 30,
        .period_slow_s = 300,
        .delta_high = 0.0f,  /* Edge case */
    };
    adaptive_poll_init(&cfg);
    uint32_t period = adaptive_poll_calc(25.0f, 25.0f);
    TEST_ASSERT_EQUAL_UINT32(30, period);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_change_returns_slow);
    RUN_TEST(test_large_change_returns_fast);
    RUN_TEST(test_very_large_change_returns_fast);
    RUN_TEST(test_half_delta_returns_middle);
    RUN_TEST(test_negative_delta_same_as_positive);
    RUN_TEST(test_default_config);
    RUN_TEST(test_custom_config);
    RUN_TEST(test_zero_delta_high_returns_fast);
    return UNITY_END();
}
