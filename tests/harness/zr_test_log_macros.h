#ifndef ZR_VM_TESTS_ZR_TEST_LOG_MACROS_H
#define ZR_VM_TESTS_ZR_TEST_LOG_MACROS_H

#include <stdio.h>
#include <time.h>

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define ZR_TEST_START(summary)                                                                                       \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_INFO(summary, details)                                                                                 \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_PASS(timer, summary)                                                                                   \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Pass - Cost Time:%.3fms - %s\n", zrTestElapsed, summary);                                             \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_FAIL(timer, summary, reason)                                                                           \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", zrTestElapsed, summary, reason);                               \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_DIVIDER()                                                                                              \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_MODULE_DIVIDER()                                                                                       \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#endif
