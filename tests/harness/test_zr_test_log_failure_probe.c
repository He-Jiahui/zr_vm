#include <stdio.h>
#include <time.h>

#include "unity.h"
#include "zr_test_log_macros.h"

static int g_cleanupReached = 0;

void setUp(void) {
}

void tearDown(void) {
}

static void test_failure_marks_unity_without_skipping_cleanup(void) {
    SZrTestTimer timer;

    timer.startTime = clock();
    timer.endTime = timer.startTime;
    ZR_TEST_FAIL(timer, "Intentional failure probe", "expected failure");
    g_cleanupReached = 1;
}

int main(void) {
    int unityResult;

    UNITY_BEGIN();
    RUN_TEST(test_failure_marks_unity_without_skipping_cleanup);
    unityResult = UNITY_END();
    printf("ZR_TEST_FAIL_PROBE cleanup_reached=%d unity_exit=%d\n", g_cleanupReached, unityResult);
    return unityResult;
}
