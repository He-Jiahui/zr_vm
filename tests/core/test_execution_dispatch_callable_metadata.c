#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static TZrInstruction test_create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) {
    TZrInstruction instruction;

    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static SZrFunction *test_create_noop_function(SZrState *state) {
    SZrFunction *function;
    TZrInstruction instruction;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    instruction = test_create_instruction_1(ZR_INSTRUCTION_ENUM(NOP), 0, 0);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = instruction;
    function->instructionsLength = 1u;

    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = 0u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrCallInfo *test_prepare_execute_call(SZrState *state,
                                              const SZrTypeValue *callableValue,
                                              SZrFunction *entryFunction) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *functionBaseValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callableValue);
    TEST_ASSERT_NOT_NULL(entryFunction);

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 1 + entryFunction->stackSize, functionBase);
    TEST_ASSERT_NOT_NULL(functionBase);

    functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(functionBaseValue);
    ZrCore_Value_Copy(state, functionBaseValue, callableValue);

    state->stackTop.valuePointer = functionBase + 1 + entryFunction->stackSize;

    callInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(callInfo);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionBase + 1 + entryFunction->stackSize;
    callInfo->context.context.programCounter = entryFunction->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;

    state->callInfoList = callInfo;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return callInfo;
}

typedef struct TestDispatchCallableCapture {
    TZrUInt32 observedCount;
    EZrValueType observedType;
    SZrRawObject *observedObject;
} TestDispatchCallableCapture;

static TZrDebugSignal test_capture_frame_callable(struct SZrState *state,
                                                  struct SZrFunction *function,
                                                  const TZrInstruction *programCounter,
                                                  TZrUInt32 instructionOffset,
                                                  TZrUInt32 sourceLine,
                                                  TZrPtr userData) {
    TestDispatchCallableCapture *capture = (TestDispatchCallableCapture *)userData;
    SZrCallInfo *callInfo;
    SZrTypeValue *callableValue;

    ZR_UNUSED_PARAMETER(function);
    ZR_UNUSED_PARAMETER(programCounter);
    ZR_UNUSED_PARAMETER(instructionOffset);
    ZR_UNUSED_PARAMETER(sourceLine);

    if (state == ZR_NULL || capture == ZR_NULL || capture->observedCount != 0u) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    callInfo = state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    callableValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callableValue == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    capture->observedType = callableValue->type;
    capture->observedObject = callableValue->value.object;
    capture->observedCount = 1u;
    return ZR_DEBUG_SIGNAL_NONE;
}

static void test_execute_refreshes_forwarded_function_base_value_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    TestDispatchCallableCapture capture;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = test_create_noop_function(state);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);
    originalFunction->super.garbageCollectMark.forwardingAddress = &forwardedFunction->super;

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalFunction));
    callableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    memset(&capture, 0, sizeof(capture));
    ZrCore_Debug_SetTraceObserver(state, test_capture_frame_callable, &capture);
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
    callInfo = test_prepare_execute_call(state, &callableValue, forwardedFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, capture.observedType);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedFunction), capture.observedObject);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_refreshes_forwarded_closure_base_value_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    SZrClosure *originalClosure;
    SZrClosure *forwardedClosure;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    TestDispatchCallableCapture capture;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = test_create_noop_function(state);
    originalClosure = ZrCore_Closure_New(state, 0);
    forwardedClosure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);
    TEST_ASSERT_NOT_NULL(originalClosure);
    TEST_ASSERT_NOT_NULL(forwardedClosure);

    originalClosure->function = originalFunction;
    forwardedClosure->function = forwardedFunction;
    originalClosure->super.garbageCollectMark.forwardingAddress = &forwardedClosure->super;

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalClosure));
    callableValue.type = ZR_VALUE_TYPE_CLOSURE;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    memset(&capture, 0, sizeof(capture));
    ZrCore_Debug_SetTraceObserver(state, test_capture_frame_callable, &capture);
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
    callInfo = test_prepare_execute_call(state, &callableValue, forwardedFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, capture.observedType);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedClosure), capture.observedObject);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_execute_refreshes_forwarded_function_base_value_object);
    RUN_TEST(test_execute_refreshes_forwarded_closure_base_value_object);

    return UNITY_END();
}
