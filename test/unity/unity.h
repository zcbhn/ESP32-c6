/* Unity Test Framework - Minimal header for RBMS host tests
 * Based on ThrowTheSwitch/Unity (MIT License)
 * https://github.com/ThrowTheSwitch/Unity
 */
#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* --- Internal state --- */
extern int unity_test_count;
extern int unity_test_failures;
extern const char *unity_current_test;

/* --- API --- */
#define UNITY_BEGIN() (unity_test_count = 0, unity_test_failures = 0, 0)

#define UNITY_END() (printf("\n%d Tests %d Failures\n", \
    unity_test_count, unity_test_failures), unity_test_failures)

#define RUN_TEST(func) do { \
    unity_current_test = #func; \
    unity_test_count++; \
    func(); \
    printf("  PASS: %s\n", #func); \
} while(0)

/* --- Assertions --- */
#define TEST_FAIL_MESSAGE(msg) do { \
    printf("  FAIL: %s (line %d): %s\n", unity_current_test, __LINE__, msg); \
    unity_test_failures++; \
    return; \
} while(0)

#define TEST_ASSERT(cond) do { \
    if (!(cond)) TEST_FAIL_MESSAGE(#cond " was FALSE"); \
} while(0)

#define TEST_ASSERT_TRUE(cond) TEST_ASSERT(cond)

#define TEST_ASSERT_EQUAL(expected, actual) do { \
    if ((expected) != (actual)) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %d, got %d", (int)(expected), (int)(actual)); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) do { \
    if ((expected) != (actual)) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %u, got %u", \
            (unsigned)(expected), (unsigned)(actual)); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) do { \
    if (fabsf((float)(expected) - (float)(actual)) > (float)(delta)) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %.4f +/- %.4f, got %.4f", \
            (double)(expected), (double)(delta), (double)(actual)); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#define TEST_ASSERT_GREATER_THAN(threshold, actual) do { \
    if (!((actual) > (threshold))) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected > %.4f, got %.4f", \
            (double)(threshold), (double)(actual)); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#define TEST_ASSERT_LESS_THAN(threshold, actual) do { \
    if (!((actual) < (threshold))) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected < %.4f, got %.4f", \
            (double)(threshold), (double)(actual)); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#define TEST_ASSERT_UINT32_WITHIN(delta, expected, actual) do { \
    unsigned _d = (unsigned)(delta); \
    unsigned _e = (unsigned)(expected); \
    unsigned _a = (unsigned)(actual); \
    unsigned _diff = (_a > _e) ? (_a - _e) : (_e - _a); \
    if (_diff > _d) { \
        char _msg[128]; \
        snprintf(_msg, sizeof(_msg), "Expected %u +/- %u, got %u", _e, _d, _a); \
        TEST_FAIL_MESSAGE(_msg); \
    } \
} while(0)

#endif /* UNITY_H */
