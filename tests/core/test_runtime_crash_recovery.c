#include <string.h>
#include <signal.h>

#include "unity.h"
#include "test_support.h"
#include "unity_crash_guard.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static volatile int g_survivor_test_ran = 0;

static void seed_current_exception(SZrState *state) {
    SZrString *message;
    SZrTypeValue payload;
    TZrNativeString literal = "crash recovery panic payload";

    TEST_ASSERT_NOT_NULL(state);

    message = ZrCore_String_Create(state, literal, strlen(literal));
    TEST_ASSERT_NOT_NULL(message);

    ZrCore_Value_InitAsRawObject(state, &payload, ZR_CAST_RAW_OBJECT_AS_SUPER(message));
    payload.type = ZR_VALUE_TYPE_STRING;
    payload.isGarbageCollectable = ZR_TRUE;
    payload.isNative = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(state,
                                                           &payload,
                                                           state->callInfoList,
                                                           ZR_THREAD_STATUS_RUNTIME_ERROR));
    TEST_ASSERT_TRUE(state->hasCurrentException);
}

static void test_runtime_crash_recovery_interrupts_current_test(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    ZrTests_Unity_ResetLastCrashInfo();
    ZrTests_Unity_ExpectRecoveredCrash();
    TEST_ASSERT_NOT_NULL(state);
    seed_current_exception(state);

    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);

    ZrTests_Runtime_State_Destroy(state);
    TEST_FAIL_MESSAGE("fatal VM panic should interrupt the current Unity test");
}

static void test_runtime_crash_recovery_continues_with_later_tests(void) {
    const ZrTestsUnityCrashInfo *crashInfo = ZrTests_Unity_GetLastCrashInfo();

    g_survivor_test_ran = 1;
    TEST_ASSERT_NOT_NULL(crashInfo);
    TEST_ASSERT_EQUAL_INT(1, crashInfo->recovered);
    TEST_ASSERT_EQUAL_INT(SIGABRT, crashInfo->signalNumber);
    TEST_ASSERT_EQUAL_INT(1, crashInfo->hadActiveVmState);
    TEST_ASSERT_EQUAL_INT(1, crashInfo->printedVmException);
    TEST_ASSERT_EQUAL_INT(1, g_survivor_test_ran);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_runtime_crash_recovery_interrupts_current_test);
    RUN_TEST(test_runtime_crash_recovery_continues_with_later_tests);
    return UNITY_END();
}
