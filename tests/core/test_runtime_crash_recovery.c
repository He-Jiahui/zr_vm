#include <string.h>
#include <signal.h>

#include "unity.h"
#include "test_support.h"
#include "unity_crash_guard.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static volatile int g_survivor_test_ran = 0;

static void run_crash_guarded_test(UnityTestFunction func, const char *funcName, int funcLineNum) {
    Unity.CurrentTestName = funcName;
    Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE) funcLineNum;
    Unity.NumberOfTests++;
#ifndef UNITY_EXCLUDE_DETAILS
    #ifdef UNITY_DETAIL_STACK_SIZE
    Unity.CurrentDetailStackSize = 0;
    #else
    UNITY_CLR_DETAILS();
    #endif
#endif
    UNITY_EXEC_TIME_START();
    if (ZrTests_Unity_TestProtect_Begin() && (setjmp(Unity.AbortFrame) == 0)) {
        setUp();
        func();
    }
    ZrTests_Unity_TestProtect_End();
    if (ZrTests_Unity_TestProtect_Begin() && (setjmp(Unity.AbortFrame) == 0)) {
        tearDown();
    }
    ZrTests_Unity_TestProtect_End();
    UNITY_EXEC_TIME_STOP();
    UnityConcludeTest();
}

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
    run_crash_guarded_test(test_runtime_crash_recovery_interrupts_current_test,
                           "test_runtime_crash_recovery_interrupts_current_test",
                           __LINE__ - 1);
    run_crash_guarded_test(test_runtime_crash_recovery_continues_with_later_tests,
                           "test_runtime_crash_recovery_continues_with_later_tests",
                           __LINE__ - 1);
    return UNITY_END();
}
