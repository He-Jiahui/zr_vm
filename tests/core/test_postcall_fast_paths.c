#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static TZrStackValuePointer reserve_postcall_window(SZrState *state, TZrSize slotCount) {
    TZrStackValuePointer windowBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(slotCount >= 6u);

    windowBase = state->stackTail.valuePointer - slotCount;
    TEST_ASSERT_TRUE(windowBase >= state->stackBase.valuePointer);
    return windowBase;
}

static void init_plain_call_info(SZrCallInfo *callInfo,
                                 SZrCallInfo *previous,
                                 TZrStackValuePointer functionBase,
                                 TZrStackValuePointer functionTop,
                                 TZrSize expectedReturnCount,
                                 TZrStackValuePointer returnDestination,
                                 TZrBool hasReturnDestination) {
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_NOT_NULL(functionBase);
    TEST_ASSERT_NOT_NULL(functionTop);

    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionTop;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->previous = previous;
    callInfo->next = ZR_NULL;
    callInfo->context = (TZrCallInfoContext){0};
    callInfo->yieldContext = (TZrCallInfoYieldContext){0};
    callInfo->expectedReturnCount = expectedReturnCount;
    callInfo->returnDestination = returnDestination;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = hasReturnDestination;
}

static void test_postcall_prepared_single_result_fast_moves_return_to_explicit_destination(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer windowBase;
    SZrCallInfo callInfo;
    SZrTypeValue *destinationValue;
    SZrTypeValue *returnValue;

    TEST_ASSERT_NOT_NULL(state);
    windowBase = reserve_postcall_window(state, 8u);
    destinationValue = ZrCore_Stack_GetValue(windowBase + 1);
    returnValue = ZrCore_Stack_GetValue(windowBase + 6);
    TEST_ASSERT_NOT_NULL(destinationValue);
    TEST_ASSERT_NOT_NULL(returnValue);

    ZrCore_Value_ResetAsNull(destinationValue);
    ZrCore_Value_InitAsInt(state, returnValue, 91);
    state->baseCallInfo.functionBase.valuePointer = windowBase;
    state->baseCallInfo.functionTop.valuePointer = windowBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    init_plain_call_info(&callInfo,
                         &state->baseCallInfo,
                         windowBase + 3,
                         windowBase + 7,
                         1u,
                         windowBase + 1,
                         ZR_TRUE);
    state->callInfoList = &callInfo;
    state->stackTop.valuePointer = windowBase + 7;
    state->debugHookSignal = 0u;

    ZrCore_Function_PostCallPreparedSingleResultFast(state, &callInfo, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, destinationValue->type);
    TEST_ASSERT_EQUAL_INT64(91, destinationValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(windowBase + 2, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_postcall_prepared_single_result_fast_resets_null_when_no_return_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer windowBase;
    SZrCallInfo callInfo;
    SZrTypeValue *destinationValue;

    TEST_ASSERT_NOT_NULL(state);
    windowBase = reserve_postcall_window(state, 8u);
    destinationValue = ZrCore_Stack_GetValue(windowBase + 1);
    TEST_ASSERT_NOT_NULL(destinationValue);

    ZrCore_Value_InitAsInt(state, destinationValue, 17);
    state->baseCallInfo.functionBase.valuePointer = windowBase;
    state->baseCallInfo.functionTop.valuePointer = windowBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    init_plain_call_info(&callInfo,
                         &state->baseCallInfo,
                         windowBase + 3,
                         windowBase + 7,
                         1u,
                         windowBase + 1,
                         ZR_TRUE);
    state->callInfoList = &callInfo;
    state->stackTop.valuePointer = windowBase + 7;
    state->debugHookSignal = 0u;

    ZrCore_Function_PostCallPreparedSingleResultFast(state, &callInfo, 0u);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, destinationValue->type);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(windowBase + 2, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_postcall_prepared_single_result_fast_falls_back_to_function_base_without_return_destination(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer windowBase;
    SZrCallInfo callInfo;
    SZrTypeValue *functionBaseValue;
    SZrTypeValue *returnValue;

    TEST_ASSERT_NOT_NULL(state);
    windowBase = reserve_postcall_window(state, 8u);
    functionBaseValue = ZrCore_Stack_GetValue(windowBase + 3);
    returnValue = ZrCore_Stack_GetValue(windowBase + 6);
    TEST_ASSERT_NOT_NULL(functionBaseValue);
    TEST_ASSERT_NOT_NULL(returnValue);

    ZrCore_Value_ResetAsNull(functionBaseValue);
    ZrCore_Value_InitAsInt(state, returnValue, 123);
    state->baseCallInfo.functionBase.valuePointer = windowBase;
    state->baseCallInfo.functionTop.valuePointer = windowBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    init_plain_call_info(&callInfo,
                         &state->baseCallInfo,
                         windowBase + 3,
                         windowBase + 7,
                         1u,
                         ZR_NULL,
                         ZR_FALSE);
    state->callInfoList = &callInfo;
    state->stackTop.valuePointer = windowBase + 7;
    state->debugHookSignal = 0u;

    ZrCore_Function_PostCallPreparedSingleResultFast(state, &callInfo, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, functionBaseValue->type);
    TEST_ASSERT_EQUAL_INT64(123, functionBaseValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(windowBase + 4, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_postcall_prepared_single_result_fast_moves_return_to_explicit_destination);
    RUN_TEST(test_postcall_prepared_single_result_fast_resets_null_when_no_return_value);
    RUN_TEST(test_postcall_prepared_single_result_fast_falls_back_to_function_base_without_return_destination);

    return UNITY_END();
}
