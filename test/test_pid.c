/**
 * @file test_pid.c
 * @brief PID controller unit tests
 */
#include "unity.h"
#include "pid.h"
#include <math.h>

static pid_ctrl_t pid;

void setUp(void)
{
    pid_init(&pid, 2.0f, 0.5f, 1.0f);
    pid_set_setpoint(&pid, 32.0f);
}

void tearDown(void) {}

void test_pid_init_sets_coefficients(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, pid.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, pid.ki);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, pid.kd);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, pid.setpoint);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pid.out_min);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, pid.out_max);
}

void test_pid_init_null_returns_error(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, pid_init(NULL, 1, 1, 1));
}

void test_pid_positive_error_gives_positive_output(void)
{
    /* setpoint=32, measurement=28, error=+4 -> output > 0 */
    float out = pid_compute(&pid, 28.0f, 1.0f);
    TEST_ASSERT_GREATER_THAN(0.0f, out);
}

void test_pid_zero_error_zero_output(void)
{
    /* setpoint=32, measurement=32, error=0 -> output ≈ 0 (first call) */
    pid.prev_measurement = 32.0f;
    float out = pid_compute(&pid, 32.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, out);
}

void test_pid_output_clamped_to_max(void)
{
    /* Very large error should clamp to 100 */
    float out = pid_compute(&pid, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, out);
}

void test_pid_output_clamped_to_min(void)
{
    /* Negative error (measurement > setpoint) should clamp to 0 */
    pid_set_setpoint(&pid, 20.0f);
    pid.prev_measurement = 50.0f;
    float out = pid_compute(&pid, 50.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out);
}

void test_pid_custom_limits(void)
{
    pid_set_limits(&pid, 10.0f, 90.0f);
    /* Large error -> should clamp to 90 */
    float out = pid_compute(&pid, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, out);
}

void test_pid_derivative_on_measurement(void)
{
    /* Two calls with same setpoint but different measurement.
     * Derivative should be based on measurement change, not error change. */
    pid_set_setpoint(&pid, 32.0f);
    float out1 = pid_compute(&pid, 30.0f, 1.0f);
    float out2 = pid_compute(&pid, 31.0f, 1.0f);
    /* As measurement increases toward setpoint, derivative term
     * should oppose (negative contribution), reducing output slightly */
    TEST_ASSERT_LESS_THAN(out1, out2);  /* out2 < out1 due to d-term + reduced error */
}

void test_pid_anti_windup(void)
{
    /* Saturate output at max for many steps, then switch measurement above setpoint.
     * Anti-windup should prevent massive undershoot. */
    for (int i = 0; i < 100; i++) {
        pid_compute(&pid, 20.0f, 1.0f);  /* Always saturated at max */
    }
    /* Now measurement jumps above setpoint */
    float out = pid_compute(&pid, 35.0f, 1.0f);
    /* Without anti-windup, integral would be huge and output would stay at max.
     * With anti-windup, output should drop quickly toward 0 */
    TEST_ASSERT_LESS_THAN(50.0f, out);
}

void test_pid_reset(void)
{
    pid_compute(&pid, 25.0f, 1.0f);
    pid_compute(&pid, 26.0f, 1.0f);
    pid_reset(&pid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pid.prev_measurement);
}

void test_pid_null_compute_returns_zero(void)
{
    float out = pid_compute(NULL, 25.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out);
}

void test_pid_zero_dt_returns_zero(void)
{
    float out = pid_compute(&pid, 25.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pid_init_sets_coefficients);
    RUN_TEST(test_pid_init_null_returns_error);
    RUN_TEST(test_pid_positive_error_gives_positive_output);
    RUN_TEST(test_pid_zero_error_zero_output);
    RUN_TEST(test_pid_output_clamped_to_max);
    RUN_TEST(test_pid_output_clamped_to_min);
    RUN_TEST(test_pid_custom_limits);
    RUN_TEST(test_pid_derivative_on_measurement);
    RUN_TEST(test_pid_anti_windup);
    RUN_TEST(test_pid_reset);
    RUN_TEST(test_pid_null_compute_returns_zero);
    RUN_TEST(test_pid_zero_dt_returns_zero);
    return UNITY_END();
}
