#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
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

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operand0,
                                                TZrUInt16 operand1) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand0;
    instruction.instruction.operand.operand1[1] = operand1;
    return instruction;
}

static TZrInstruction test_create_instruction_call_1(EZrInstructionCode opcode,
                                                     TZrUInt16 resultSlot,
                                                     TZrUInt16 functionSlot,
                                                     TZrUInt16 argumentCount) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = resultSlot;
    instruction.instruction.operand.operand1[0] = functionSlot;
    instruction.instruction.operand.operand1[1] = argumentCount;
    return instruction;
}

static TZrInstruction test_create_instruction_return_1(TZrUInt16 returnCount, TZrUInt16 resultSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = resultSlot;
    instruction.instruction.operand.operand1[1] = 0u;
    return instruction;
}

static TZrInstruction test_create_instruction_member_call_1(TZrUInt16 resultSlot, TZrUInt16 cacheIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL);
    instruction.instruction.operandExtra = resultSlot;
    instruction.instruction.operand.operand1[0] = cacheIndex;
    return instruction;
}

static SZrFunction *test_create_noop_function_with_signature(SZrState *state, TZrUInt16 parameterCount) {
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
    function->stackSize = parameterCount;
    function->parameterCount = parameterCount;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *test_create_noop_function(SZrState *state) {
    return test_create_noop_function_with_signature(state, 0u);
}

static SZrFunction *test_create_get_constant_return_function(SZrState *state, const SZrTypeValue *constantValue) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(constantValue);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = test_create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = test_create_instruction_return_1(1u, 0u);
    function->instructionsLength = 2u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
    ZrCore_Value_Copy(state, &function->constantValueList[0], constantValue);
    function->constantValueLength = 1u;

    function->stackSize = 1u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *test_create_get_constant_set_stack_return_function(SZrState *state,
                                                                       const SZrTypeValue *constantValue) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(constantValue);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = test_create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = test_create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 1u, 0);
    function->instructionsList[2] = test_create_instruction_return_1(1u, 1u);
    function->instructionsLength = 3u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
    ZrCore_Value_Copy(state, &function->constantValueList[0], constantValue);
    function->constantValueLength = 1u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *test_create_get_stack_return_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = test_create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), 1u, 0);
    function->instructionsList[1] = test_create_instruction_return_1(1u, 1u);
    function->instructionsLength = 2u;

    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *test_create_native_callable(SZrState *state, FZrNativeFunction nativeFunction) {
    SZrClosureNative *closure;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(nativeFunction);

    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);

    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    return ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
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
    const SZrFunction *expectedFunction;
    const TZrInstruction *expectedProgramCounter;
} TestDispatchCallableCapture;

static TestDispatchCallableCapture *gDebugHookCapture = ZR_NULL;

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
    ZR_UNUSED_PARAMETER(instructionOffset);
    ZR_UNUSED_PARAMETER(sourceLine);

    if (state == ZR_NULL || capture == ZR_NULL || capture->observedCount != 0u) {
        return ZR_DEBUG_SIGNAL_NONE;
    }
    if (capture->expectedFunction != ZR_NULL && function != capture->expectedFunction) {
        return ZR_DEBUG_SIGNAL_NONE;
    }
    if (capture->expectedProgramCounter != ZR_NULL && programCounter != capture->expectedProgramCounter) {
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

static void test_capture_call_hook_frame_callable(struct SZrState *state, SZrDebugInfo *debugInfo) {
    SZrCallInfo *callInfo;
    SZrTypeValue *callableValue;
    TestDispatchCallableCapture *capture = gDebugHookCapture;

    if (state == ZR_NULL || debugInfo == ZR_NULL || capture == ZR_NULL || capture->observedCount != 0u ||
        debugInfo->event != ZR_DEBUG_HOOK_EVENT_CALL) {
        return;
    }

    callInfo = debugInfo->callInfo;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return;
    }
    if (capture->expectedProgramCounter != ZR_NULL &&
        callInfo->context.context.programCounter != capture->expectedProgramCounter) {
        return;
    }

    callableValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callableValue == ZR_NULL) {
        return;
    }

    capture->observedType = callableValue->type;
    capture->observedObject = callableValue->value.object;
    capture->observedCount = 1u;
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
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_CALL;
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

static void test_known_vm_call_refreshes_forwarded_stateless_function_without_materializing_closure(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalCalleeFunction;
    SZrFunction *forwardedCalleeFunction;
    SZrFunction *callerFunction;
    SZrCallInfo *callInfo;
    SZrTypeValue callerCallableValue;
    SZrTypeValue *calleeSlotValue;
    TZrInstruction callerInstructions[1];
    TestDispatchCallableCapture capture;

    TEST_ASSERT_NOT_NULL(state);

    originalCalleeFunction = ZrCore_Function_New(state);
    forwardedCalleeFunction = test_create_noop_function(state);
    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(originalCalleeFunction);
    TEST_ASSERT_NOT_NULL(forwardedCalleeFunction);
    TEST_ASSERT_NOT_NULL(callerFunction);

    originalCalleeFunction->super.garbageCollectMark.forwardingAddress = &forwardedCalleeFunction->super;
    callerInstructions[0] =
            test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0u, 0u, 0u);
    callerFunction->instructionsList = callerInstructions;
    callerFunction->instructionsLength = ZR_ARRAY_COUNT(callerInstructions);
    callerFunction->stackSize = 1u;
    callerFunction->parameterCount = 0u;
    callerFunction->hasVariableArguments = ZR_FALSE;
    callerFunction->closureValueLength = 0u;

    ZrCore_Value_ResetAsNull(&callerCallableValue);
    ZrCore_Value_InitAsRawObject(state, &callerCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    callerCallableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callerCallableValue.isGarbageCollectable = ZR_TRUE;
    callerCallableValue.isNative = ZR_FALSE;

    memset(&capture, 0, sizeof(capture));
    capture.expectedProgramCounter = forwardedCalleeFunction->instructionsList;
    gDebugHookCapture = &capture;
    state->debugHook = test_capture_call_hook_frame_callable;
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_CALL;
    callInfo = test_prepare_execute_call(state, &callerCallableValue, callerFunction);
    TEST_ASSERT_NOT_NULL(callInfo);
    calleeSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    TEST_ASSERT_NOT_NULL(calleeSlotValue);
    ZrCore_Value_InitAsRawObject(state, calleeSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalCalleeFunction));
    calleeSlotValue->type = ZR_VALUE_TYPE_FUNCTION;
    calleeSlotValue->isGarbageCollectable = ZR_TRUE;
    calleeSlotValue->isNative = ZR_FALSE;

    ZrCore_Execute(state, callInfo);

    gDebugHookCapture = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, capture.observedType);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedCalleeFunction), capture.observedObject);
    TEST_ASSERT_NULL(originalCalleeFunction->cachedStatelessClosure);
    TEST_ASSERT_NULL(forwardedCalleeFunction->cachedStatelessClosure);

    ZrTests_Runtime_State_Destroy(state);
}

static SZrFunction *test_create_simple_caller_function(SZrState *state,
                                                       const TZrInstruction *instructions,
                                                       TZrUInt32 instructionCount,
                                                       TZrUInt32 stackSize) {
    SZrFunction *function = ZrCore_Function_New(state);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(instructions);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, instructionCount);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * instructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    memcpy(function->instructionsList, instructions, sizeof(TZrInstruction) * instructionCount);
    function->instructionsLength = instructionCount;
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = stackSize;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void test_enable_helper_profiling(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

static void test_disable_helper_profiling(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_Profile_SetCurrentState(ZR_NULL);
        state->global->profileRuntime = ZR_NULL;
    }
}

static TZrInt64 test_super_dyn_call_cached_native_returns_constant(SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValueNoProfile(base), 123);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrUInt64 test_execute_known_vm_call_stack_get_helper_count(SZrState *state,
                                                                   SZrFunction *callerFunction,
                                                                   SZrFunction *calleeFunction,
                                                                   SZrObject *argumentObject,
                                                                   SZrProfileRuntime *profileRuntime) {
    SZrCallInfo *callInfo;
    SZrTypeValue callerCallableValue;
    SZrTypeValue *calleeSlotValue;
    SZrTypeValue *argumentSlotValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);
    TEST_ASSERT_NOT_NULL(argumentObject);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    ZrCore_Value_ResetAsNull(&callerCallableValue);
    ZrCore_Value_InitAsRawObject(state, &callerCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    callerCallableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callerCallableValue.isGarbageCollectable = ZR_TRUE;
    callerCallableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callerCallableValue, callerFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    calleeSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    argumentSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(calleeSlotValue);
    TEST_ASSERT_NOT_NULL(argumentSlotValue);

    ZrCore_Value_InitAsRawObject(state, calleeSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(calleeFunction));
    calleeSlotValue->type = ZR_VALUE_TYPE_FUNCTION;
    calleeSlotValue->isGarbageCollectable = ZR_TRUE;
    calleeSlotValue->isNative = ZR_FALSE;

    ZrCore_Value_InitAsRawObject(state, argumentSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject));
    argumentSlotValue->type = ZR_VALUE_TYPE_OBJECT;
    argumentSlotValue->isGarbageCollectable = ZR_TRUE;
    argumentSlotValue->isNative = ZR_FALSE;

    test_enable_helper_profiling(state, profileRuntime);
    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    {
        TZrUInt64 stackGetCount = profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE];
        test_disable_helper_profiling(state);
        return stackGetCount;
    }
}

static TZrUInt64 test_execute_function_call_stack_get_helper_count(SZrState *state,
                                                                   EZrInstructionCode opcode,
                                                                   SZrFunction *callerFunction,
                                                                   SZrFunction *calleeFunction,
                                                                   SZrObject *argumentObject,
                                                                   SZrProfileRuntime *profileRuntime) {
    SZrCallInfo *callInfo;
    SZrTypeValue callerCallableValue;
    SZrTypeValue *calleeSlotValue;
    SZrTypeValue *argumentSlotValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);
    TEST_ASSERT_NOT_NULL(argumentObject);
    TEST_ASSERT_NOT_NULL(profileRuntime);
    TEST_ASSERT_TRUE(opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
                     opcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL));

    ZrCore_Value_ResetAsNull(&callerCallableValue);
    ZrCore_Value_InitAsRawObject(state, &callerCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    callerCallableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callerCallableValue.isGarbageCollectable = ZR_TRUE;
    callerCallableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callerCallableValue, callerFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    calleeSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    argumentSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(calleeSlotValue);
    TEST_ASSERT_NOT_NULL(argumentSlotValue);

    ZrCore_Value_InitAsRawObject(state, calleeSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(calleeFunction));
    calleeSlotValue->type = ZR_VALUE_TYPE_FUNCTION;
    calleeSlotValue->isGarbageCollectable = ZR_TRUE;
    calleeSlotValue->isNative = ZR_FALSE;

    ZrCore_Value_InitAsRawObject(state, argumentSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject));
    argumentSlotValue->type = ZR_VALUE_TYPE_OBJECT;
    argumentSlotValue->isGarbageCollectable = ZR_TRUE;
    argumentSlotValue->isNative = ZR_FALSE;

    test_enable_helper_profiling(state, profileRuntime);
    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    {
        TZrUInt64 stackGetCount = profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE];
        test_disable_helper_profiling(state);
        return stackGetCount;
    }
}

static TZrUInt64 test_execute_upvalue_open_capture_stack_get_helper_count(SZrState *state,
                                                                          EZrInstructionCode opcode,
                                                                          TZrInt64 openCaptureInitialValue,
                                                                          TZrInt64 sourceValue,
                                                                          TZrInt64 *outObservedValue,
                                                                          SZrProfileRuntime *profileRuntime) {
    TZrInstruction instructions[1];
    SZrFunction *function;
    SZrClosure *closure;
    SZrClosureValue *openCapture;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    TZrStackValuePointer openCaptureSlot;
    SZrTypeValue *openCaptureStackValue;
    SZrTypeValue *sourceSlotValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(profileRuntime);
    TEST_ASSERT_TRUE(opcode == ZR_INSTRUCTION_ENUM(GETUPVAL) || opcode == ZR_INSTRUCTION_ENUM(SETUPVAL));

    instructions[0] = test_create_instruction_2(opcode, 0u, 0u, 0u);
    function = test_create_simple_caller_function(state, instructions, ZR_ARRAY_COUNT(instructions), 2u);
    TEST_ASSERT_NOT_NULL(function);

    closure = ZrCore_Closure_New(state, 1u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callableValue.type = ZR_VALUE_TYPE_CLOSURE;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callableValue, function);
    TEST_ASSERT_NOT_NULL(callInfo);

    openCaptureSlot = callInfo->functionBase.valuePointer + 2;
    openCaptureStackValue = ZrCore_Stack_GetValue(openCaptureSlot);
    TEST_ASSERT_NOT_NULL(openCaptureStackValue);
    ZrCore_Value_InitAsInt(state, openCaptureStackValue, openCaptureInitialValue);

    openCapture = ZrCore_Closure_FindOrCreateValue(state, openCaptureSlot);
    TEST_ASSERT_NOT_NULL(openCapture);
    closure->closureValuesExtend[0] = openCapture;
    ZrCore_RawObject_Barrier(state,
                             ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(openCapture));

    if (opcode == ZR_INSTRUCTION_ENUM(SETUPVAL)) {
        sourceSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
        TEST_ASSERT_NOT_NULL(sourceSlotValue);
        ZrCore_Value_InitAsInt(state, sourceSlotValue, sourceValue);
    }

    test_enable_helper_profiling(state, profileRuntime);
    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    {
        TZrUInt64 stackGetCount = profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE];
        test_disable_helper_profiling(state);

        if (outObservedValue != ZR_NULL) {
            if (opcode == ZR_INSTRUCTION_ENUM(GETUPVAL)) {
                SZrTypeValue *resultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
                TEST_ASSERT_NOT_NULL(resultValue);
                TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue->type));
                *outObservedValue = resultValue->value.nativeObject.nativeInt64;
            } else {
                SZrTypeValue *capturedValue = ZrCore_ClosureValue_GetValue(openCapture);
                TEST_ASSERT_NOT_NULL(capturedValue);
                TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(capturedValue->type));
                *outObservedValue = capturedValue->value.nativeObject.nativeInt64;
            }
        }
        return stackGetCount;
    }
}

static TZrUInt64 test_execute_super_dyn_call_cached_stack_get_helper_count(
        SZrState *state,
        SZrFunction *callerFunction,
        SZrObject *receiverObject,
        SZrProfileRuntime *profileRuntime) {
    SZrCallInfo *callInfo;
    SZrTypeValue callerCallableValue;
    SZrTypeValue *callableSlotValue;
    SZrTypeValue *argumentSlotValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(receiverObject);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    ZrCore_Value_ResetAsNull(&callerCallableValue);
    ZrCore_Value_InitAsRawObject(state, &callerCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    callerCallableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callerCallableValue.isGarbageCollectable = ZR_TRUE;
    callerCallableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callerCallableValue, callerFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    callableSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    argumentSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(callableSlotValue);
    TEST_ASSERT_NOT_NULL(argumentSlotValue);

    ZrCore_Value_InitAsRawObject(state, callableSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    callableSlotValue->type = ZR_VALUE_TYPE_OBJECT;
    callableSlotValue->isGarbageCollectable = ZR_TRUE;
    callableSlotValue->isNative = ZR_FALSE;
    ZrCore_Value_InitAsInt(state, argumentSlotValue, 7);

    test_enable_helper_profiling(state, profileRuntime);
    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    {
        TZrUInt64 stackGetCount = profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE];
        test_disable_helper_profiling(state);
        return stackGetCount;
    }
}

static TZrUInt64 test_execute_known_vm_member_call_stack_get_helper_count(
        SZrState *state,
        SZrFunction *callerFunction,
        SZrFunction *calleeFunction,
        SZrObject *receiverObject,
        SZrObjectPrototype *receiverPrototype,
        SZrProfileRuntime *profileRuntime) {
    SZrCallInfo *callInfo;
    SZrTypeValue callerCallableValue;
    SZrTypeValue *receiverSlotValue;
    SZrFunctionCallSiteCacheEntry *cacheEntry;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(calleeFunction);
    TEST_ASSERT_NOT_NULL(receiverObject);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    TEST_ASSERT_NOT_NULL(profileRuntime);
    TEST_ASSERT_NOT_NULL(callerFunction->callSiteCaches);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, callerFunction->callSiteCacheLength);

    cacheEntry = &callerFunction->callSiteCaches[0];
    memset(cacheEntry, 0, sizeof(*cacheEntry));
    cacheEntry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
    cacheEntry->argumentCount = 1u;
    cacheEntry->picSlotCount = 1u;
    cacheEntry->picSlots[0].cachedReceiverPrototype = receiverPrototype;
    cacheEntry->picSlots[0].cachedOwnerPrototype = receiverPrototype;
    cacheEntry->picSlots[0].cachedReceiverObject = receiverObject;
    cacheEntry->picSlots[0].cachedFunction = calleeFunction;
    cacheEntry->picSlots[0].cachedReceiverVersion = receiverPrototype->super.memberVersion;
    cacheEntry->picSlots[0].cachedOwnerVersion = receiverPrototype->super.memberVersion;

    ZrCore_Value_ResetAsNull(&callerCallableValue);
    ZrCore_Value_InitAsRawObject(state, &callerCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
    callerCallableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callerCallableValue.isGarbageCollectable = ZR_TRUE;
    callerCallableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callerCallableValue, callerFunction);
    TEST_ASSERT_NOT_NULL(callInfo);

    receiverSlotValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 2);
    TEST_ASSERT_NOT_NULL(receiverSlotValue);
    ZrCore_Value_InitAsRawObject(state, receiverSlotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverSlotValue->type = ZR_VALUE_TYPE_OBJECT;
    receiverSlotValue->isGarbageCollectable = ZR_TRUE;
    receiverSlotValue->isNative = ZR_FALSE;

    test_enable_helper_profiling(state, profileRuntime);
    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    {
        TZrUInt64 stackGetCount = profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE];
        test_disable_helper_profiling(state);
        return stackGetCount;
    }
}

static void test_known_vm_call_direct_path_avoids_dispatch_bookkeeping_stack_get_value_helpers(void) {
    TZrInstruction knownVmCallInstructions[1];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *calleeFunction;
    SZrFunction *callerFunction;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    calleeFunction = test_create_noop_function_with_signature(state, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    knownVmCallInstructions[0] = test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0u, 0u, 1u);
    callerFunction =
            test_create_simple_caller_function(state, knownVmCallInstructions, ZR_ARRAY_COUNT(knownVmCallInstructions), 2u);
    TEST_ASSERT_NOT_NULL(callerFunction);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_known_vm_call_stack_get_helper_count(state,
                                                                               callerFunction,
                                                                               calleeFunction,
                                                                               ZrCore_Object_New(state, ZR_NULL),
                                                                               &profileRuntime));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_call_direct_function_value_avoids_extra_stack_get_value_helpers(void) {
    TZrInstruction functionCallInstructions[1];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *calleeFunction;
    SZrFunction *callerFunction;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    calleeFunction = test_create_noop_function_with_signature(state, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    functionCallInstructions[0] = test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0u, 0u, 1u);
    callerFunction =
            test_create_simple_caller_function(state, functionCallInstructions, ZR_ARRAY_COUNT(functionCallInstructions), 2u);
    TEST_ASSERT_NOT_NULL(callerFunction);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_function_call_stack_get_helper_count(state,
                                                                               ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                                               callerFunction,
                                                                               calleeFunction,
                                                                               ZrCore_Object_New(state, ZR_NULL),
                                                                               &profileRuntime));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_function_tail_call_direct_function_value_avoids_extra_stack_get_value_helpers(void) {
    TZrInstruction functionTailCallInstructions[1];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *calleeFunction;
    SZrFunction *callerFunction;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    calleeFunction = test_create_noop_function_with_signature(state, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    functionTailCallInstructions[0] =
            test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL), 0u, 0u, 1u);
    callerFunction = test_create_simple_caller_function(
            state,
            functionTailCallInstructions,
            ZR_ARRAY_COUNT(functionTailCallInstructions),
            2u);
    TEST_ASSERT_NOT_NULL(callerFunction);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_function_call_stack_get_helper_count(state,
                                                                               ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                                                               callerFunction,
                                                                               calleeFunction,
                                                                               ZrCore_Object_New(state, ZR_NULL),
                                                                               &profileRuntime));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_getupval_open_capture_path_avoids_stack_get_value_helpers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    TZrInt64 observedValue = 0;

    TEST_ASSERT_NOT_NULL(state);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_upvalue_open_capture_stack_get_helper_count(state,
                                                                                     ZR_INSTRUCTION_ENUM(GETUPVAL),
                                                                                     77,
                                                                                     0,
                                                                                     &observedValue,
                                                                                     &profileRuntime));
    TEST_ASSERT_EQUAL_INT64(77, observedValue);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_setupval_open_capture_path_avoids_stack_get_value_helpers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    TZrInt64 observedValue = 0;

    TEST_ASSERT_NOT_NULL(state);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_upvalue_open_capture_stack_get_helper_count(state,
                                                                                     ZR_INSTRUCTION_ENUM(SETUPVAL),
                                                                                     -1,
                                                                                     66,
                                                                                     &observedValue,
                                                                                     &profileRuntime));
    TEST_ASSERT_EQUAL_INT64(66, observedValue);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_known_vm_member_call_exact_cache_path_avoids_extra_stack_get_value_helpers(void) {
    TZrInstruction knownVmCallInstructions[1];
    TZrInstruction knownVmMemberCallInstructions[1];
    SZrState *knownVmState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrState *memberCallState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *calleeFunction;
    SZrFunction *knownVmCallerFunction;
    SZrFunction *memberCallCallerFunction;
    SZrString *prototypeName;
    SZrObjectPrototype *receiverPrototype;
    SZrObject *receiverObject;
    SZrProfileRuntime knownVmProfileRuntime;
    SZrProfileRuntime memberCallProfileRuntime;
    TZrUInt64 knownVmStackGetCount;
    TZrUInt64 memberCallStackGetCount;

    TEST_ASSERT_NOT_NULL(knownVmState);
    TEST_ASSERT_NOT_NULL(memberCallState);

    calleeFunction = test_create_noop_function_with_signature(knownVmState, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    knownVmCallInstructions[0] = test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0u, 0u, 1u);
    knownVmCallerFunction =
            test_create_simple_caller_function(knownVmState, knownVmCallInstructions, ZR_ARRAY_COUNT(knownVmCallInstructions), 2u);
    TEST_ASSERT_NOT_NULL(knownVmCallerFunction);

    knownVmStackGetCount = test_execute_known_vm_call_stack_get_helper_count(knownVmState,
                                                                             knownVmCallerFunction,
                                                                             calleeFunction,
                                                                             ZrCore_Object_New(knownVmState, ZR_NULL),
                                                                             &knownVmProfileRuntime);

    calleeFunction = test_create_noop_function_with_signature(memberCallState, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    knownVmMemberCallInstructions[0] = test_create_instruction_member_call_1(0u, 0u);
    memberCallCallerFunction = test_create_simple_caller_function(
            memberCallState,
            knownVmMemberCallInstructions,
            ZR_ARRAY_COUNT(knownVmMemberCallInstructions),
            2u);
    TEST_ASSERT_NOT_NULL(memberCallCallerFunction);
    memberCallCallerFunction->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            memberCallState->global,
            sizeof(SZrFunctionCallSiteCacheEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(memberCallCallerFunction->callSiteCaches);
    memset(memberCallCallerFunction->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
    memberCallCallerFunction->callSiteCacheLength = 1u;

    prototypeName = ZrCore_String_CreateFromNative(memberCallState, "DispatchWorker");
    TEST_ASSERT_NOT_NULL(prototypeName);
    receiverPrototype = ZrCore_ObjectPrototype_New(memberCallState, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    receiverObject = ZrCore_Object_New(memberCallState, receiverPrototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(memberCallState, receiverObject);

    memberCallStackGetCount = test_execute_known_vm_member_call_stack_get_helper_count(memberCallState,
                                                                                        memberCallCallerFunction,
                                                                                        calleeFunction,
                                                                                        receiverObject,
                                                                                        receiverPrototype,
                                                                                        &memberCallProfileRuntime);

    TEST_ASSERT_EQUAL_UINT64(0u, knownVmStackGetCount);
    TEST_ASSERT_EQUAL_UINT64(0u, memberCallStackGetCount);

    ZrTests_Runtime_State_Destroy(knownVmState);
    ZrTests_Runtime_State_Destroy(memberCallState);
}

static void test_super_dyn_call_cached_hit_path_avoids_stack_get_value_helpers(void) {
    TZrInstruction dynCallInstructions[1];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *cachedCallableFunction;
    SZrFunction *callerFunction;
    SZrString *prototypeName;
    SZrObjectPrototype *receiverPrototype;
    SZrObject *receiverObject;
    SZrFunctionCallSiteCacheEntry *cacheEntry;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    cachedCallableFunction = test_create_native_callable(state, test_super_dyn_call_cached_native_returns_constant);
    TEST_ASSERT_NOT_NULL(cachedCallableFunction);

    dynCallInstructions[0] = test_create_instruction_call_1(ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), 0u, 0u, 0u);
    callerFunction = test_create_simple_caller_function(state, dynCallInstructions, ZR_ARRAY_COUNT(dynCallInstructions), 2u);
    TEST_ASSERT_NOT_NULL(callerFunction);

    callerFunction->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallSiteCacheEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(callerFunction->callSiteCaches);
    memset(callerFunction->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
    callerFunction->callSiteCacheLength = 1u;

    prototypeName = ZrCore_String_CreateFromNative(state, "DispatchDynCallableBox");
    TEST_ASSERT_NOT_NULL(prototypeName);
    receiverPrototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    receiverObject = ZrCore_Object_New(state, receiverPrototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    cacheEntry = &callerFunction->callSiteCaches[0];
    cacheEntry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL;
    cacheEntry->argumentCount = 1u;
    cacheEntry->picSlotCount = 1u;
    cacheEntry->picSlots[0].cachedReceiverPrototype = receiverPrototype;
    cacheEntry->picSlots[0].cachedOwnerPrototype = receiverPrototype;
    cacheEntry->picSlots[0].cachedFunction = cachedCallableFunction;
    cacheEntry->picSlots[0].cachedReceiverVersion = receiverPrototype->super.memberVersion;
    cacheEntry->picSlots[0].cachedOwnerVersion = receiverPrototype->super.memberVersion;

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_super_dyn_call_cached_stack_get_helper_count(state,
                                                                                       callerFunction,
                                                                                       receiverObject,
                                                                                       &profileRuntime));
    TEST_ASSERT_EQUAL_UINT64(1u, cacheEntry->runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT64(0u, cacheEntry->runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_known_vm_member_call_exact_cache_path_runs_without_member_metadata_fallback(void) {
    TZrInstruction knownVmMemberCallInstructions[1];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *calleeFunction;
    SZrFunction *callerFunction;
    SZrString *prototypeName;
    SZrObjectPrototype *receiverPrototype;
    SZrObject *receiverObject;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    calleeFunction = test_create_noop_function_with_signature(state, 1u);
    TEST_ASSERT_NOT_NULL(calleeFunction);

    knownVmMemberCallInstructions[0] = test_create_instruction_member_call_1(0u, 0u);
    callerFunction = test_create_simple_caller_function(
            state,
            knownVmMemberCallInstructions,
            ZR_ARRAY_COUNT(knownVmMemberCallInstructions),
            2u);
    TEST_ASSERT_NOT_NULL(callerFunction);
    callerFunction->memberEntries = ZR_NULL;
    callerFunction->memberEntryLength = 0u;
    callerFunction->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallSiteCacheEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(callerFunction->callSiteCaches);
    memset(callerFunction->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
    callerFunction->callSiteCacheLength = 1u;

    prototypeName = ZrCore_String_CreateFromNative(state, "DispatchWorkerNoMemberMetadata");
    TEST_ASSERT_NOT_NULL(prototypeName);
    receiverPrototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(receiverPrototype);
    receiverObject = ZrCore_Object_New(state, receiverPrototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    TEST_ASSERT_EQUAL_UINT64(0u,
                             test_execute_known_vm_member_call_stack_get_helper_count(state,
                                                                                      callerFunction,
                                                                                      calleeFunction,
                                                                                      receiverObject,
                                                                                      receiverPrototype,
                                                                                      &profileRuntime));
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]);
    TEST_ASSERT_EQUAL_UINT64(1u, callerFunction->callSiteCaches[0].runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT64(0u, callerFunction->callSiteCaches[0].runtimeMissCount);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_constant_profiled_stack_destination_records_one_value_copy_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    SZrTypeValue constantValue;
    SZrTypeValue *resultValue;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsInt(state, &constantValue, 321);
    function = test_create_get_constant_return_function(state, &constantValue);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callableValue, function);
    TEST_ASSERT_NOT_NULL(callInfo);

    test_enable_helper_profiling(state, &profileRuntime);
    ZrCore_Execute(state, callInfo);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    test_disable_helper_profiling(state);

    resultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue->type));
    TEST_ASSERT_EQUAL_INT64(321, resultValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_stack_profiled_stack_destination_records_one_value_copy_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    SZrTypeValue *sourceValue;
    SZrTypeValue *resultValue;
    SZrProfileRuntime profileRuntime;

    TEST_ASSERT_NOT_NULL(state);

    function = test_create_get_stack_return_function(state);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    callInfo = test_prepare_execute_call(state, &callableValue, function);
    TEST_ASSERT_NOT_NULL(callInfo);

    sourceValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    TEST_ASSERT_NOT_NULL(sourceValue);
    ZrCore_Value_InitAsInt(state, sourceValue, 654);

    test_enable_helper_profiling(state, &profileRuntime);
    ZrCore_Execute(state, callInfo);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT64(1u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    test_disable_helper_profiling(state);

    resultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue->type));
    TEST_ASSERT_EQUAL_INT64(654, resultValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_constant_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrFunction *function;
    SZrTypeValue constantValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    ZrCore_Value_InitAsRawObject(state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    constantValue.type = ZR_VALUE_TYPE_OBJECT;
    function = test_create_get_constant_return_function(state, &constantValue);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(constantValue.value.object, result.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_constant_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrFunction *function;
    SZrTypeValue constantValue;
    SZrTypeValue result;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "ExecuteGetConstantStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    sourceObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    constantValue.type = ZR_VALUE_TYPE_OBJECT;
    function = test_create_get_constant_return_function(state, &constantValue);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(constantValue.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(sourceObject->prototype, copiedObject->prototype);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_constant_then_set_stack_plain_heap_object_reuses_original_object(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrFunction *function;
    SZrTypeValue constantValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    ZrCore_Value_InitAsRawObject(state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    constantValue.type = ZR_VALUE_TYPE_OBJECT;
    function = test_create_get_constant_set_stack_return_function(state, &constantValue);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(constantValue.value.object, result.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, result.ownershipKind);
    TEST_ASSERT_NULL(result.ownershipControl);
    TEST_ASSERT_NULL(result.ownershipWeakRef);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execute_get_constant_then_set_stack_struct_object_still_clones_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    SZrFunction *function;
    SZrTypeValue constantValue;
    SZrTypeValue result;
    SZrObject *copiedObject;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "ExecuteGetConstantSetStackStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    sourceObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    constantValue.type = ZR_VALUE_TYPE_OBJECT;
    function = test_create_get_constant_set_stack_return_function(state, &constantValue);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_EQUAL(constantValue.value.object, result.value.object);
    copiedObject = ZR_CAST_OBJECT(state, result.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_EQUAL_PTR(sourceObject->prototype, copiedObject->prototype);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_execute_refreshes_forwarded_function_base_value_object);
    RUN_TEST(test_execute_refreshes_forwarded_closure_base_value_object);
    RUN_TEST(test_known_vm_call_refreshes_forwarded_stateless_function_without_materializing_closure);
    RUN_TEST(test_known_vm_call_direct_path_avoids_dispatch_bookkeeping_stack_get_value_helpers);
    RUN_TEST(test_function_call_direct_function_value_avoids_extra_stack_get_value_helpers);
    RUN_TEST(test_function_tail_call_direct_function_value_avoids_extra_stack_get_value_helpers);
    RUN_TEST(test_getupval_open_capture_path_avoids_stack_get_value_helpers);
    RUN_TEST(test_setupval_open_capture_path_avoids_stack_get_value_helpers);
    RUN_TEST(test_known_vm_member_call_exact_cache_path_avoids_extra_stack_get_value_helpers);
    RUN_TEST(test_super_dyn_call_cached_hit_path_avoids_stack_get_value_helpers);
    RUN_TEST(test_known_vm_member_call_exact_cache_path_runs_without_member_metadata_fallback);
    RUN_TEST(test_execute_get_constant_profiled_stack_destination_records_one_value_copy_helper);
    RUN_TEST(test_execute_get_stack_profiled_stack_destination_records_one_value_copy_helper);
    RUN_TEST(test_execute_get_constant_plain_heap_object_reuses_original_object);
    RUN_TEST(test_execute_get_constant_struct_object_still_clones_result);
    RUN_TEST(test_execute_get_constant_then_set_stack_plain_heap_object_reuses_original_object);
    RUN_TEST(test_execute_get_constant_then_set_stack_struct_object_still_clones_result);

    return UNITY_END();
}
