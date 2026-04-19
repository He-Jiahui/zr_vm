#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

typedef struct TestDirtyAllocatorContext {
    TZrUInt32 moveCount;
} TestDirtyAllocatorContext;

void setUp(void) {}

void tearDown(void) {}

static TZrPtr test_dirty_allocator(TZrPtr userData,
                                   TZrPtr pointer,
                                   TZrSize originalSize,
                                   TZrSize newSize,
                                   TZrInt64 flag) {
    TestDirtyAllocatorContext *context = (TestDirtyAllocatorContext *)userData;
    TZrPtr newPointer;

    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL || pointer < (TZrPtr)0x1000) {
        TZrPtr freshPointer = malloc(newSize);
        if (freshPointer != ZR_NULL) {
            memset(freshPointer, 0xCD, newSize);
        }
        return freshPointer;
    }

    newPointer = malloc(newSize);
    if (newPointer == ZR_NULL) {
        return ZR_NULL;
    }

    memset(newPointer, 0xCD, newSize);
    if (originalSize > 0) {
        memcpy(newPointer, pointer, originalSize < newSize ? originalSize : newSize);
    }
    free(pointer);
    if (context != ZR_NULL) {
        context->moveCount++;
    }
    return newPointer;
}

static SZrState *test_create_state_with_dirty_allocator(TestDirtyAllocatorContext *context) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_dirty_allocator, context, 12345, &callbacks);
    SZrState *state;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    state = global->mainThreadState;
    if (state != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(state, global);
    }
    return state;
}

static void assign_entry_local_metadata(SZrState *state,
                                        SZrFunction *function,
                                        const TZrUInt32 *stackSlots,
                                        TZrSize localCount) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    if (localCount == 0) {
        function->localVariableList = ZR_NULL;
        function->localVariableLength = 0;
        return;
    }

    function->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionLocalVariable) * localCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->localVariableList);
    function->localVariableLength = (TZrUInt32)localCount;

    for (TZrSize index = 0; index < localCount; index++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[index];
        ZrCore_Memory_RawSet(local, 0, sizeof(*local));
        local->stackSlot = stackSlots[index];
        local->offsetActivate = 0;
        local->offsetDead = function->instructionsLength;
    }
}

static SZrTypeValue *init_function_callable_value(SZrState *state,
                                                  TZrStackValuePointer callBase,
                                                  SZrFunction *function) {
    SZrTypeValue *callableValue = ZrCore_Stack_GetValue(callBase);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callBase);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);
    TEST_ASSERT_FALSE(callableValue->isNative);
    return callableValue;
}

static SZrTypeValue *init_vm_closure_callable_value(SZrState *state,
                                                    TZrStackValuePointer callBase,
                                                    SZrClosure *closure) {
    SZrTypeValue *callableValue = ZrCore_Stack_GetValue(callBase);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callBase);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callableValue->type = ZR_VALUE_TYPE_CLOSURE;
    callableValue->isGarbageCollectable = ZR_TRUE;
    callableValue->isNative = ZR_FALSE;
    return callableValue;
}

static void write_int_argument_slot(SZrState *state,
                                    TZrStackValuePointer callBase,
                                    TZrSize argumentIndex,
                                    TZrInt64 value,
                                    TZrUInt32 toBeClosedOffset) {
    SZrTypeValueOnStack *slot = callBase + 1 + argumentIndex;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(callBase);
    TEST_ASSERT_NOT_NULL(slot);

    ZrCore_Value_InitAsInt(state, &slot->value, value);
    slot->toBeClosedValueOffset = toBeClosedOffset;
}

static void assert_int_argument_slot(TZrStackValuePointer functionBase,
                                     TZrSize argumentIndex,
                                     TZrInt64 expectedValue,
                                     TZrUInt32 expectedOffset,
                                     const char *valueMessage,
                                     const char *offsetMessage) {
    SZrTypeValueOnStack *slot = functionBase + 1 + argumentIndex;

    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(expectedValue,
                                    ZrCore_Stack_GetValue(slot)->value.nativeObject.nativeInt64,
                                    valueMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expectedOffset,
                                     slot->toBeClosedValueOffset,
                                     offsetMessage);
}

static void assert_reset_frame_slots(TZrStackValuePointer functionBase,
                                     TZrSize firstResetSlot,
                                     TZrSize stackSize,
                                     const char *offsetMessage,
                                     const char *valueMessage) {
    for (TZrSize slotIndex = firstResetSlot; slotIndex < stackSize; slotIndex++) {
        SZrTypeValueOnStack *slot = functionBase + 1 + slotIndex;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, slot->toBeClosedValueOffset, offsetMessage);
        TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_NULL(slot->value.type), valueMessage);
    }
}

static void test_precall_clears_reused_frame_slot_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    const TZrUInt32 localSlots[] = {0u, 1u, 2u};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 3;
    function->parameterCount = 0;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    for (TZrSize index = 0; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callBase + 1 + index;
        ZrCore_Value_InitAsInt(state, &slot->value, (TZrInt64)(100 + index));
        slot->toBeClosedValueOffset = (TZrUInt32)(index + 1);
    }

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, callableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(4u, function->vmEntryClearStackSizePlusOne);

    for (TZrSize index = 0; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callInfo->functionBase.valuePointer + 1 + index;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                         slot->toBeClosedValueOffset,
                                         "precall must clear stale to-be-closed metadata");
        TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_NULL(slot->value.type),
                                 "precall must reset frame locals to null");
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_clears_reused_frame_slot_metadata_with_explicit_argument_count(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    const TZrUInt32 localSlots[] = {1u, 2u};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 3;
    function->parameterCount = 1;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 77);
    callBase[1].toBeClosedValueOffset = 9u;
    ZrCore_Value_InitAsInt(state, &callBase[2].value, 88);
    callBase[2].toBeClosedValueOffset = 10u;
    ZrCore_Value_InitAsInt(state, &callBase[3].value, 99);
    callBase[3].toBeClosedValueOffset = 11u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 1, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(4u, function->vmEntryClearStackSizePlusOne);

    TEST_ASSERT_EQUAL_INT64_MESSAGE(77,
                                    ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1)->value.nativeObject.nativeInt64,
                                    "resolved precall must preserve explicit argument slots");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9u,
                                     (callInfo->functionBase.valuePointer + 1)->toBeClosedValueOffset,
                                     "resolved precall must leave explicit argument metadata intact");

    for (TZrSize index = 1; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callInfo->functionBase.valuePointer + 1 + index;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                         slot->toBeClosedValueOffset,
                                         "resolved precall must clear stale to-be-closed metadata past arguments");
        TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_NULL(slot->value.type),
                                 "resolved precall must reset frame locals past explicit arguments to null");
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_keeps_transient_temp_slots_intact_when_no_entry_locals_need_null_reset(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 4;
    function->parameterCount = 1;
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 77);
    callBase[1].toBeClosedValueOffset = 9u;
    ZrCore_Value_InitAsInt(state, &callBase[2].value, 88);
    callBase[2].toBeClosedValueOffset = 10u;
    ZrCore_Value_InitAsInt(state, &callBase[3].value, 99);
    callBase[3].toBeClosedValueOffset = 11u;
    ZrCore_Value_InitAsInt(state, &callBase[4].value, 111);
    callBase[4].toBeClosedValueOffset = 12u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 1, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(2u, function->vmEntryClearStackSizePlusOne);

    TEST_ASSERT_EQUAL_INT64(77,
                            ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1)->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(9u, (callInfo->functionBase.valuePointer + 1)->toBeClosedValueOffset);

    for (TZrSize index = 1; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callInfo->functionBase.valuePointer + 1 + index;
        TZrInt64 expectedValue = ((TZrInt64[]){88, 99, 111})[index - 1];
        TZrUInt32 expectedOffset = ((TZrUInt32[]){10u, 11u, 12u})[index - 1];
        TEST_ASSERT_EQUAL_INT64_MESSAGE(expectedValue,
                                        ZrCore_Stack_GetValue(slot)->value.nativeObject.nativeInt64,
                                        "resolved precall should leave transient temp slot values untouched");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(expectedOffset,
                                         slot->toBeClosedValueOffset,
                                         "resolved precall should not scrub transient temp slot metadata");
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction instructions[1] = {0};
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    function->instructionsList = instructions;
    function->instructionsLength = ZR_ARRAY_COUNT(instructions);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 77);
    callBase[1].toBeClosedValueOffset = 9u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_UINT32(2u, function->vmEntryClearStackSizePlusOne);

    callInfo->callStatus = (EZrCallStatus)(ZR_CALL_STATUS_NATIVE_CALL | ZR_CALL_STATUS_ALLOW_HOOK);
    callInfo->context.nativeContext.previousErrorFunction = 77u;
    callInfo->context.nativeContext.continuationArguments = (TZrNativePtr)0x1234;
    callInfo->yieldContext.returnValueCount = 99u;
    callInfo->returnDestinationReusableOffset = 55u;
    callInfo->hasReturnDestination = ZR_FALSE;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_CALL_STATUS_NONE, callInfo->callStatus);
    TEST_ASSERT_EQUAL_PTR(instructions, callInfo->context.context.programCounter);
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_SIGNAL_NONE, callInfo->context.context.trap);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->context.context.variableArgumentCount);
    TEST_ASSERT_EQUAL_UINT64(0u, callInfo->yieldContext.returnValueCount);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, callInfo->returnDestination);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->returnDestinationReusableOffset);
    TEST_ASSERT_TRUE(callInfo->hasReturnDestination);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_prepared_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction instructions[1] = {0};
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    function->instructionsList = instructions;
    function->instructionsLength = ZR_ARRAY_COUNT(instructions);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 177);
    callBase[1].toBeClosedValueOffset = 19u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_UINT32(2u, function->vmEntryClearStackSizePlusOne);

    callInfo->callStatus = (EZrCallStatus)(ZR_CALL_STATUS_NATIVE_CALL | ZR_CALL_STATUS_ALLOW_HOOK);
    callInfo->context.nativeContext.previousErrorFunction = 177u;
    callInfo->context.nativeContext.continuationArguments = (TZrNativePtr)0x5678;
    callInfo->yieldContext.returnValueCount = 199u;
    callInfo->returnDestinationReusableOffset = 75u;
    callInfo->hasReturnDestination = ZR_FALSE;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_CALL_STATUS_NONE, callInfo->callStatus);
    TEST_ASSERT_EQUAL_PTR(instructions, callInfo->context.context.programCounter);
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_SIGNAL_NONE, callInfo->context.context.trap);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->context.context.variableArgumentCount);
    TEST_ASSERT_EQUAL_UINT64(0u, callInfo->yieldContext.returnValueCount);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, callInfo->returnDestination);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->returnDestinationReusableOffset);
    TEST_ASSERT_TRUE(callInfo->hasReturnDestination);
    TEST_ASSERT_EQUAL_PTR(callBase + 2, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_prepared_resolved_vm_precall_try_exact_args_steady_state_hits_on_cached_path(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction instructions[1] = {0};
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    function->instructionsList = instructions;
    function->instructionsLength = ZR_ARRAY_COUNT(instructions);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 177);
    callBase[1].toBeClosedValueOffset = 19u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_UINT32(2u, function->vmEntryClearStackSizePlusOne);

    callInfo->callStatus = (EZrCallStatus)(ZR_CALL_STATUS_NATIVE_CALL | ZR_CALL_STATUS_ALLOW_HOOK);
    callInfo->context.nativeContext.previousErrorFunction = 177u;
    callInfo->context.nativeContext.continuationArguments = (TZrNativePtr)0x5678;
    callInfo->yieldContext.returnValueCount = 199u;
    callInfo->returnDestinationReusableOffset = 75u;
    callInfo->hasReturnDestination = ZR_FALSE;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_TryPreCallPreparedResolvedVmFunctionExactArgsSteadyState(
            state, callBase, function, 1, 1, callBase + 1);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_INT(ZR_CALL_STATUS_NONE, callInfo->callStatus);
    TEST_ASSERT_EQUAL_PTR(instructions, callInfo->context.context.programCounter);
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_SIGNAL_NONE, callInfo->context.context.trap);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->context.context.variableArgumentCount);
    TEST_ASSERT_EQUAL_UINT64(0u, callInfo->yieldContext.returnValueCount);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, callInfo->returnDestination);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->returnDestinationReusableOffset);
    TEST_ASSERT_TRUE(callInfo->hasReturnDestination);
    TEST_ASSERT_EQUAL_PTR(callBase + 2, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_precall_growth_clears_newly_exposed_entry_local_slots_with_dirty_allocator(void) {
    TestDirtyAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_dirty_allocator(&allocatorContext);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    const TZrUInt32 localSlots[] = {1u, 2u, 3u};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 4;
    function->parameterCount = 1;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));

    callBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(callBase >= state->stackBase.valuePointer);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);

    ZrCore_Value_InitAsInt(state, &callBase[1].value, 77);
    callBase[1].toBeClosedValueOffset = 55u;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, callableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(5u, function->vmEntryClearStackSizePlusOne);
    TEST_ASSERT_EQUAL_INT64(77,
                            ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1)->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(55u, (callInfo->functionBase.valuePointer + 1)->toBeClosedValueOffset);

    for (TZrSize index = 1; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callInfo->functionBase.valuePointer + 1 + index;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                         slot->toBeClosedValueOffset,
                                         "growth path must clear stale to-be-closed metadata for activated locals");
        TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_NULL(slot->value.type),
                                 "growth path must reset newly exposed entry locals to null");
    }

    ZrCore_GlobalState_Free(state->global);
}

static void test_precall_growth_reuses_cached_zero_capture_closure_across_repeated_growths_with_dirty_allocator(void) {
    TestDirtyAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_dirty_allocator(&allocatorContext);
    SZrFunction *function;
    TZrStackValuePointer firstCallBase;
    TZrStackValuePointer secondCallBase;
    SZrTypeValue *firstCallableValue;
    SZrTypeValue *secondCallableValue;
    SZrTypeValue *firstPreparedCallable;
    SZrTypeValue *secondPreparedCallable;
    SZrCallInfo *firstCallInfo;
    SZrCallInfo *secondCallInfo;
    TZrUInt32 previousMoveCount;
    const TZrUInt32 localSlots[] = {1u, 2u, 3u};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 4;
    function->parameterCount = 1;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));

    firstCallBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(firstCallBase >= state->stackBase.valuePointer);
    firstCallableValue = init_function_callable_value(state, firstCallBase, function);
    write_int_argument_slot(state, firstCallBase, 0, 77, 55u);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = firstCallBase + 2;
    firstCallInfo = ZrCore_Function_PreCallKnownValue(state, firstCallBase, firstCallableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(firstCallInfo);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_NOT_NULL(function->cachedStatelessClosure);
    firstPreparedCallable = ZrCore_Stack_GetValue(firstCallInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(firstPreparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, firstPreparedCallable->type);
    TEST_ASSERT_FALSE(firstPreparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(function->cachedStatelessClosure,
                          ZR_CAST_VM_CLOSURE(state, firstPreparedCallable->value.object));
    assert_int_argument_slot(firstCallInfo->functionBase.valuePointer,
                             0,
                             77,
                             55u,
                             "first growth must preserve the explicit argument value",
                             "first growth must preserve the explicit argument metadata");
    assert_reset_frame_slots(firstCallInfo->functionBase.valuePointer,
                             1,
                             function->stackSize,
                             "first growth must clear stale to-be-closed metadata past the explicit argument",
                             "first growth must reset newly exposed entry locals past the explicit argument");

    previousMoveCount = allocatorContext.moveCount;
    secondCallBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(secondCallBase >= state->stackBase.valuePointer);
    secondCallableValue = init_function_callable_value(state, secondCallBase, function);
    write_int_argument_slot(state, secondCallBase, 0, 88, 66u);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = secondCallBase + 2;
    secondCallInfo = ZrCore_Function_PreCallKnownValue(state, secondCallBase, secondCallableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(secondCallInfo);
    TEST_ASSERT_GREATER_THAN_UINT32(previousMoveCount, allocatorContext.moveCount);
    secondPreparedCallable = ZrCore_Stack_GetValue(secondCallInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(secondPreparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, secondPreparedCallable->type);
    TEST_ASSERT_FALSE(secondPreparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(function->cachedStatelessClosure,
                          ZR_CAST_VM_CLOSURE(state, secondPreparedCallable->value.object));
    assert_int_argument_slot(secondCallInfo->functionBase.valuePointer,
                             0,
                             88,
                             66u,
                             "cached-closure growth must preserve the explicit argument value",
                             "cached-closure growth must preserve the explicit argument metadata");
    assert_reset_frame_slots(secondCallInfo->functionBase.valuePointer,
                             1,
                             function->stackSize,
                             "cached-closure growth must clear stale to-be-closed metadata past the explicit argument",
                             "cached-closure growth must reset newly exposed entry locals past the explicit argument");

    ZrCore_GlobalState_Free(state->global);
}

static void test_precall_growth_with_existing_vm_closure_clears_newly_exposed_entry_local_slots_with_dirty_allocator(void) {
    TestDirtyAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_dirty_allocator(&allocatorContext);
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrTypeValue *preparedCallable;
    SZrCallInfo *callInfo;
    const TZrUInt32 localSlots[] = {1u, 2u, 3u};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    closure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(closure);
    function->stackSize = 4;
    function->parameterCount = 1;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));
    closure->function = function;

    callBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(callBase >= state->stackBase.valuePointer);
    callableValue = init_vm_closure_callable_value(state, callBase, closure);
    write_int_argument_slot(state, callBase, 0, 177, 155u);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, callableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    preparedCallable = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(preparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, preparedCallable->type);
    TEST_ASSERT_FALSE(preparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, preparedCallable->value.object));
    TEST_ASSERT_NULL_MESSAGE(function->cachedStatelessClosure,
                             "existing VM closure growth must not backfill the stateless function cache");
    assert_int_argument_slot(callInfo->functionBase.valuePointer,
                             0,
                             177,
                             155u,
                             "existing VM closure growth must preserve the explicit argument value",
                             "existing VM closure growth must preserve the explicit argument metadata");
    assert_reset_frame_slots(callInfo->functionBase.valuePointer,
                             1,
                             function->stackSize,
                             "existing VM closure growth must clear stale to-be-closed metadata past the explicit argument",
                             "existing VM closure growth must reset newly exposed entry locals past the explicit argument");

    ZrCore_GlobalState_Free(state->global);
}

static void test_resolved_vm_precall_preserves_multiple_explicit_arguments_across_repeated_growth_with_dirty_allocator(void) {
    TestDirtyAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_dirty_allocator(&allocatorContext);
    SZrFunction *function;
    const TZrUInt32 localSlots[] = {2u, 3u, 4u};
    const TZrInt64 argumentValues[2][2] = {{770, 880}, {771, 881}};
    const TZrUInt32 argumentOffsets[2][2] = {{55u, 66u}, {56u, 67u}};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 5;
    function->parameterCount = 2;
    assign_entry_local_metadata(state, function, localSlots, ZR_ARRAY_COUNT(localSlots));

    for (TZrSize pass = 0; pass < 2; pass++) {
        TZrStackValuePointer callBase = state->stackTail.valuePointer - 3;
        SZrTypeValue *preparedCallable;
        SZrCallInfo *callInfo;
        TZrUInt32 previousMoveCount = allocatorContext.moveCount;
        SZrRawObject *expectedCallableObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);

        TEST_ASSERT_TRUE(callBase >= state->stackBase.valuePointer);
        init_function_callable_value(state, callBase, function);
        write_int_argument_slot(state, callBase, 0, argumentValues[pass][0], argumentOffsets[pass][0]);
        write_int_argument_slot(state, callBase, 1, argumentValues[pass][1], argumentOffsets[pass][1]);

        state->callInfoList = &state->baseCallInfo;
        state->stackTop.valuePointer = callBase + 3;
        callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 2, 1, ZR_NULL);

        TEST_ASSERT_NOT_NULL(callInfo);
        TEST_ASSERT_GREATER_THAN_UINT32(previousMoveCount, allocatorContext.moveCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->vmEntryClearStackSizePlusOne);
        preparedCallable = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
        TEST_ASSERT_NOT_NULL(preparedCallable);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, preparedCallable->type);
        TEST_ASSERT_FALSE(preparedCallable->isNative);
        TEST_ASSERT_EQUAL_PTR(expectedCallableObject, preparedCallable->value.object);
        assert_int_argument_slot(callInfo->functionBase.valuePointer,
                                 0,
                                 argumentValues[pass][0],
                                 argumentOffsets[pass][0],
                                 "resolved vm growth must preserve the first explicit argument value",
                                 "resolved vm growth must preserve the first explicit argument metadata");
        assert_int_argument_slot(callInfo->functionBase.valuePointer,
                                 1,
                                 argumentValues[pass][1],
                                 argumentOffsets[pass][1],
                                 "resolved vm growth must preserve the second explicit argument value",
                                 "resolved vm growth must preserve the second explicit argument metadata");
        assert_reset_frame_slots(callInfo->functionBase.valuePointer,
                                 2,
                                 function->stackSize,
                                 "resolved vm growth must clear stale to-be-closed metadata past explicit arguments",
                                 "resolved vm growth must reset newly exposed entry locals past explicit arguments");
    }

    ZrCore_GlobalState_Free(state->global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_precall_clears_reused_frame_slot_metadata);
    RUN_TEST(test_resolved_vm_precall_clears_reused_frame_slot_metadata_with_explicit_argument_count);
    RUN_TEST(test_resolved_vm_precall_keeps_transient_temp_slots_intact_when_no_entry_locals_need_null_reset);
    RUN_TEST(test_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info);
    RUN_TEST(test_prepared_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info);
    RUN_TEST(test_prepared_resolved_vm_precall_try_exact_args_steady_state_hits_on_cached_path);
    RUN_TEST(test_precall_growth_clears_newly_exposed_entry_local_slots_with_dirty_allocator);
    RUN_TEST(test_precall_growth_reuses_cached_zero_capture_closure_across_repeated_growths_with_dirty_allocator);
    RUN_TEST(test_precall_growth_with_existing_vm_closure_clears_newly_exposed_entry_local_slots_with_dirty_allocator);
    RUN_TEST(test_resolved_vm_precall_preserves_multiple_explicit_arguments_across_repeated_growth_with_dirty_allocator);

    return UNITY_END();
}
