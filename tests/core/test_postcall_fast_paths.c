#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

// 快速返回测试借用已分配栈的尾部窗口；不能跨出当前栈基址或假设运行时会扩容。
static TZrStackValuePointer reserve_postcall_window(SZrState *state, TZrSize slotCount) {
    TZrStackValuePointer windowBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(slotCount >= 6u);

    windowBase = state->stackTail.valuePointer - slotCount;
    TEST_ASSERT_TRUE(windowBase >= state->stackBase.valuePointer);
    return windowBase;
}

// 构造无调试/异常状态的调用帧，这是单结果快速返回入口的使用前提。
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

    memset(callInfo, 0, sizeof(*callInfo));
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

// 调用者预留独立目的槽时，返回值应直接落在该槽并恢复调用者帧和栈顶。
static void test_postcall_prepared_single_result_fast_moves_return_to_explicit_destination(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer windowBase;
    SZrCallInfo callInfo;
    SZrFunctionFrameSlotLayout layout = {0};
    SZrFunction function = {0};
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
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.directValueFrameSlotCountPlusOne = 2u;
    callInfo.metadataFunction = &function;
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

// 空返回不能遗留调用者目的槽中的旧值，否则下次读取会看到上一次调用结果。
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

// 未指定目的槽的调用沿用函数基址作为结果槽，栈顶随该位置恢复。
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

/* Native/AOT frames carry VM metadata without activating its VALUE mirrors.
 * A mirror starting inside a physical slot must neither supply nor receive
 * the result, and the caller's whole reserved window remains live. */
static void assert_native_metadata_return(TZrBool fast, TZrBool nativeCallee) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrCallInfo callInfo;
    SZrFunction callerFunction = {0};
    SZrFunction calleeFunction = {0};
    SZrFunctionFrameSlotLayout callerLayout = {0};
    SZrFunctionFrameSlotLayout calleeLayout = {0};
    SZrTypeValueOnStack untouched[2];
    TZrStackValuePointer windowBase;
    SZrTypeValue *destination;

    TEST_ASSERT_NOT_NULL(state);
    windowBase = reserve_postcall_window(state, 16u);
    for (TZrSize slot = 0u; slot < 16u; ++slot) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(windowBase + slot));
    }
    callerLayout.stackSlot = 0u;
    callerLayout.byteOffset = (TZrUInt32)(2u * sizeof(SZrTypeValueOnStack) + 16u);
    callerLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    callerLayout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    callerLayout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    callerFunction.stackSize = 2u;
    callerFunction.frameSlotLayouts = &callerLayout;
    callerFunction.frameSlotLayoutLength = 1u;
    callerFunction.frameByteSize = callerLayout.byteOffset + callerLayout.byteSize;
    callerFunction.frameByteAlign = callerLayout.byteAlign;
    state->baseCallInfo.functionBase.valuePointer = windowBase;
    state->baseCallInfo.functionTop.valuePointer = windowBase + 7;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    state->baseCallInfo.metadataFunction = &callerFunction;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    TEST_ASSERT_EQUAL_PTR(windowBase + 7,
                         ZrCore_Function_GetCallInfoFrameStorageTop(state, &state->baseCallInfo));

    init_plain_call_info(&callInfo, &state->baseCallInfo, windowBase + 8,
                         windowBase + 16, 1u, windowBase + 1, ZR_TRUE);
    if (nativeCallee) {
        calleeLayout = callerLayout;
        calleeLayout.stackSlot = 6u;
        calleeFunction.stackSize = 7u;
        calleeFunction.frameSlotLayouts = &calleeLayout;
        calleeFunction.frameSlotLayoutLength = 1u;
        calleeFunction.frameByteSize = callerFunction.frameByteSize;
        calleeFunction.frameByteAlign = callerFunction.frameByteAlign;
        callInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
        callInfo.metadataFunction = &calleeFunction;
    }
    destination = ZrCore_Stack_GetValue(windowBase + 1);
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(windowBase + 15), 42);
    memcpy(untouched, windowBase + 3, sizeof(untouched));
    state->callInfoList = &callInfo;
    state->stackTop.valuePointer = windowBase + 16;
    state->debugHookSignal = 0u;

    if (fast) {
        ZrCore_Function_PostCallPreparedSingleResultFast(state, &callInfo, 1u);
    } else {
        ZrCore_Function_PostCall(state, &callInfo, 1u);
    }

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, destination->type);
    TEST_ASSERT_EQUAL_INT64(42, destination->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_MEMORY(untouched, windowBase + 3, sizeof(untouched));
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    state->baseCallInfo.metadataFunction = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_postcall_preserves_inactive_native_value_mirrors(void) {
    assert_native_metadata_return(ZR_FALSE, ZR_FALSE);
    assert_native_metadata_return(ZR_FALSE, ZR_TRUE);
}

static void test_postcall_fast_preserves_inactive_native_value_mirrors(void) {
    assert_native_metadata_return(ZR_TRUE, ZR_FALSE);
    assert_native_metadata_return(ZR_TRUE, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_postcall_prepared_single_result_fast_moves_return_to_explicit_destination);
    RUN_TEST(test_postcall_prepared_single_result_fast_resets_null_when_no_return_value);
    RUN_TEST(test_postcall_prepared_single_result_fast_falls_back_to_function_base_without_return_destination);
    RUN_TEST(test_postcall_preserves_inactive_native_value_mirrors);
    RUN_TEST(test_postcall_fast_preserves_inactive_native_value_mirrors);

    return UNITY_END();
}
