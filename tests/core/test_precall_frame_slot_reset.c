#include <stdlib.h>
#include <stddef.h>
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

static TZrInstruction *assign_owned_instructions(SZrState *state,
                                                 SZrFunction *function,
                                                 TZrUInt32 instructionCount) {
    TZrInstruction *instructions;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, instructionCount);

    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * instructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(instructions);
    ZrCore_Memory_RawSet(instructions, 0, sizeof(TZrInstruction) * instructionCount);
    function->instructionsList = instructions;
    function->instructionsLength = instructionCount;
    return instructions;
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

static TZrSize test_frame_storage_slots_for_bytes(TZrUInt32 byteSize) {
    TZrSize slotByteSize = sizeof(SZrTypeValueOnStack);
    return (byteSize + slotByteSize - 1u) / slotByteSize;
}

static void test_call_info_frame_storage_cache_uses_legacy_padding(void) {
    TZrSize legacySize;
    TZrSize returnFlagEnd =
            offsetof(SZrCallInfo, hasReturnDestination) +
            sizeof(((SZrCallInfo *)0)->hasReturnDestination);
    TZrSize cacheOffset = offsetof(SZrCallInfo, frameStorageSlotCountPlusOne);
    TZrSize cacheEnd =
            cacheOffset + sizeof(((SZrCallInfo *)0)->frameStorageSlotCountPlusOne);
    TZrSize nextLegacyFieldOffset =
            offsetof(SZrCallInfo, argumentSourceFrameBase);
    TZrSize debugGenerationEnd =
            offsetof(SZrCallInfo, debugFrameGeneration) +
            sizeof(((SZrCallInfo *)0)->debugFrameGeneration);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(returnFlagEnd, cacheOffset);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(nextLegacyFieldOffset, cacheEnd);
    legacySize = (debugGenerationEnd + ZR_ALIGN_SIZE - 1u) /
                 ZR_ALIGN_SIZE * ZR_ALIGN_SIZE;
    TEST_ASSERT_EQUAL_UINT32(legacySize, sizeof(SZrCallInfo));
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

static void test_resolved_vm_precall_clears_logical_temps_when_typed_frame_layout_is_present(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *layouts;
    TZrStackValuePointer callBase;
    SZrCallInfo *callInfo;
    TZrSize frameStorageSlotCount;
    TZrUInt32 layoutOffset;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 4u;
    function->parameterCount = 1u;
    function->vmEntryClearStackSizePlusOne = 2u;
    function->frameSlotLayoutLength = function->stackSize;
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*layouts) * function->frameSlotLayoutLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    ZrCore_Memory_RawSet(layouts, 0, sizeof(*layouts) * function->frameSlotLayoutLength);
    function->frameSlotLayouts = layouts;
    layoutOffset = (TZrUInt32)(sizeof(SZrTypeValueOnStack) * function->stackSize);
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset = layoutOffset;
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = ZR_ALIGN_SIZE;
        layouts[index].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        layouts[index].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layoutOffset += (TZrUInt32)sizeof(SZrTypeValue);
    }
    function->frameByteSize = layoutOffset;
    function->frameByteAlign = ZR_ALIGN_SIZE;

    frameStorageSlotCount = ZrCore_Function_GetFrameStorageSlotCount(function);
    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + frameStorageSlotCount, callBase);
    init_function_callable_value(state, callBase, function);
    for (TZrUInt32 index = 0u; index < function->stackSize; index++) {
        ZrCore_Value_InitAsInt(state, &callBase[1u + index].value, (TZrInt64)(70u + index));
        callBase[1u + index].toBeClosedValueOffset = 10u + index;
    }

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 2u;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 1u, 1u, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_UINT32(function->stackSize + 1u, function->vmEntryClearStackSizePlusOne);
    TEST_ASSERT_EQUAL_INT64(70, ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1u)
                                          ->value.nativeObject.nativeInt64);
    for (TZrUInt32 index = 1u; index < function->stackSize; index++) {
        SZrTypeValueOnStack *slot = callInfo->functionBase.valuePointer + 1u + index;
        TEST_ASSERT_EQUAL_UINT32(0u, slot->toBeClosedValueOffset);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(slot->value.type));
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction *instructions;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    instructions = assign_owned_instructions(state, function, 1u);
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
    TZrInstruction *instructions;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    instructions = assign_owned_instructions(state, function, 1u);
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
    TZrInstruction *instructions;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 1;
    instructions = assign_owned_instructions(state, function, 1u);
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

static void test_resolved_vm_precall_reserves_byte_frame_storage_beyond_logical_slots(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    TZrSize expectedStorageSlots;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 1;
    function->parameterCount = 0;
    assign_owned_instructions(state, function, 1u);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;
    function->vmEntryClearStackSizePlusOne = 1u;
    function->frameByteSize = (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 4u - 1u);
    function->frameByteAlign = ZR_ALIGN_SIZE;

    expectedStorageSlots = test_frame_storage_slots_for_bytes(function->frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, expectedStorageSlots);
    TEST_ASSERT_GREATER_THAN_UINT32(function->stackSize, expectedStorageSlots);

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + expectedStorageSlots, callBase);
    init_function_callable_value(state, callBase, function);
    for (TZrSize index = function->stackSize; index < expectedStorageSlots; index++) {
        ZrCore_Value_InitAsInt(state, &callBase[1 + index].value, (TZrInt64)(900 + index));
        callBase[1 + index].toBeClosedValueOffset = (TZrUInt32)(30u + index);
    }

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 0, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + expectedStorageSlots, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, state->stackTop.valuePointer);
    for (TZrSize index = function->stackSize; index < expectedStorageSlots; index++) {
        TEST_ASSERT_EQUAL_UINT32(0u, callBase[1 + index].toBeClosedValueOffset);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(callBase[1 + index].value.type));
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_reserves_generated_temp_slots_beyond_declared_stack_size(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction *instructions;
    TZrStackValuePointer callBase;
    TZrSize expectedStorageSlots;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 0;
    instructions = assign_owned_instructions(state, function, 4u);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;
    function->vmEntryClearStackSizePlusOne = 1u;

    instructions[0].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_GLOBAL);
    instructions[0].instruction.operandExtra = 32u;
    instructions[1].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_MEMBER);
    instructions[1].instruction.operandExtra = 3u;
    instructions[1].instruction.operand.operand1[0] = 32u;
    instructions[2].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(RESET_STACK_NULL);
    instructions[2].instruction.operandExtra = 31u;
    instructions[3].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instructions[3].instruction.operandExtra = 1u;
    instructions[3].instruction.operand.operand1[0] = 31u;

    expectedStorageSlots = 33u;
    TEST_ASSERT_EQUAL_UINT32(expectedStorageSlots, ZrCore_Function_GetGeneratedFrameSlotCount(function));
    TEST_ASSERT_EQUAL_UINT32(expectedStorageSlots, ZrCore_Function_GetFrameStorageSlotCount(function));

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + expectedStorageSlots, callBase);
    init_function_callable_value(state, callBase, function);
    for (TZrSize index = function->stackSize; index < expectedStorageSlots; index++) {
        ZrCore_Value_InitAsInt(state, &callBase[1 + index].value, (TZrInt64)(700 + index));
        callBase[1 + index].toBeClosedValueOffset = (TZrUInt32)(90u + index);
    }

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 0, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + expectedStorageSlots, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(expectedStorageSlots + 1u,
                             ZrCore_CallInfo_GetFrameStorageSlotCountPlusOne(callInfo));
    function->instructionsLength = 0u;
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + expectedStorageSlots,
                          ZrCore_Function_GetCallInfoFrameStorageTop(
                                  state, callInfo));
    function->instructionsLength = 4u;
    TEST_ASSERT_EQUAL_PTR(callBase + 1, state->stackTop.valuePointer);
    for (TZrSize index = function->stackSize; index < expectedStorageSlots; index++) {
        TEST_ASSERT_EQUAL_UINT32(0u, callBase[1 + index].toBeClosedValueOffset);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(callBase[1 + index].value.type));
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_generated_frame_slot_count_summary_is_finalize_only_and_refreshable(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInstruction *instructions;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2u;
    instructions = assign_owned_instructions(state, function, 1u);
    instructions[0].instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_GLOBAL);
    instructions[0].instruction.operandExtra = 32u;

    TEST_ASSERT_EQUAL_UINT32(
            33u, ZrCore_Function_GetGeneratedFrameSlotCount(function));
    instructions[0].instruction.operandExtra = 9u;
    TEST_ASSERT_EQUAL_UINT32(
            10u, ZrCore_Function_GetGeneratedFrameSlotCount(function));

    instructions[0].instruction.operandExtra = 32u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);
    function->instructionsLength = 0u;
    TEST_ASSERT_EQUAL_UINT32(
            33u, ZrCore_Function_GetGeneratedFrameSlotCount(function));

    function->instructionsLength = 1u;
    instructions[0].instruction.operandExtra = 7u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);
    TEST_ASSERT_EQUAL_UINT32(
            8u, ZrCore_Function_GetGeneratedFrameSlotCount(function));

    ZrTests_Runtime_State_Destroy(state);
}

typedef struct TestFrameGcVisitContext {
    TZrSize count;
    SZrTypeValue *lastValue;
} TestFrameGcVisitContext;

static void test_frame_gc_visit_value_slot(SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    TestFrameGcVisitContext *context = (TestFrameGcVisitContext *)userData;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(context);

    context->count++;
    context->lastValue = value;
}

static void test_frame_gc_visitor_includes_value_layout_slots(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *layout;
    TZrStackValuePointer callBase;
    TZrStackValuePointer frameBase;
    SZrStackFramePlace place;
    TestFrameGcVisitContext context = {0u, ZR_NULL};

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 0;
    function->frameByteSize = (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 4u);
    function->frameByteAlign = ZR_ALIGN_SIZE;
    function->frameSlotLayoutLength = 1u;
    layout = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*layout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layout);
    memset(layout, 0, sizeof(*layout));
    layout->stackSlot = 1u;
    layout->byteOffset = (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 3u);
    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = ZR_ALIGN_SIZE;
    layout->slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function->frameSlotLayouts = layout;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + ZrCore_Function_GetFrameStorageSlotCount(function), callBase);
    TEST_ASSERT_NOT_NULL(callBase);
    frameBase = callBase + 1;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, layout->stackSlot, &place));

    ZrCore_Value_InitAsInt(state, (SZrTypeValue *)place.address, 4242);
    ZrCore_Value_InitAsInt(state, &frameBase[layout->stackSlot].value, 7);

    TEST_ASSERT_TRUE(ZrCore_Function_VisitFrameGcValues(state,
                                                        function,
                                                        frameBase,
                                                        ZR_NULL,
                                                        ZR_NULL,
                                                        test_frame_gc_visit_value_slot,
                                                        &context));
    TEST_ASSERT_EQUAL_UINT64(1u, context.count);
    TEST_ASSERT_EQUAL_PTR(place.address, context.lastValue);
    TEST_ASSERT_EQUAL_INT64(4242, context.lastValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_prepared_resolved_vm_precall_reserves_byte_frame_storage_after_fast_probe_fallback(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    TZrSize expectedStorageSlots;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 1;
    function->parameterCount = 0;
    assign_owned_instructions(state, function, 1u);
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;
    function->vmEntryClearStackSizePlusOne = 1u;
    function->frameByteSize = (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 3u + 1u);
    function->frameByteAlign = ZR_ALIGN_SIZE;

    expectedStorageSlots = test_frame_storage_slots_for_bytes(function->frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(4u, expectedStorageSlots);

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + expectedStorageSlots, callBase);
    init_function_callable_value(state, callBase, function);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    TEST_ASSERT_NULL(ZrCore_Function_TryPreCallPreparedResolvedVmFunctionExactArgsSteadyState(
            state,
            callBase,
            function,
            0,
            1,
            ZR_NULL));
    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(state, callBase, function, 0, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + expectedStorageSlots, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_packed_direct_prepared_precall_clears_and_initializes_complete_frame(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionFrameSlotLayout *layouts;
    TZrStackValuePointer callBase;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            2u * sizeof(*layouts),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    memset(layouts, 0, 2u * sizeof(*layouts));

    function->stackSize = 2u;
    function->parameterCount = 1u;
    function->frameByteSize =
            4u * (TZrUInt32)sizeof(SZrTypeValueOnStack);
    function->frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValueOnStack);
    function->frameSlotLayouts = layouts;
    function->frameSlotLayoutLength = 2u;
    for (TZrUInt32 index = 0u; index < 2u; index++) {
        layouts[index].stackSlot = index;
        layouts[index].byteOffset =
                (2u + index) * (TZrUInt32)sizeof(SZrTypeValueOnStack);
        layouts[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[index].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[index].slotKind =
                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        layouts[index].isParameter = index == 0u;
    }
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);
    TEST_ASSERT_TRUE(ZrCore_Function_HasDirectValueFrameSlotSummary(function));

    callBase = ZrCore_Function_CheckStackAndGc(
            state, 5u, state->stackTop.valuePointer);
    init_function_callable_value(state, callBase, function);
    write_int_argument_slot(state, callBase, 0u, 73, 0u);
    ZrCore_Value_InitAsInt(state, &callBase[2].value, 91);
    ZrCore_Value_InitAsInt(state, &callBase[3].value, 92);
    ZrCore_Value_InitAsInt(state, &callBase[4].value, 93);
    callBase[2].toBeClosedValueOffset = 12u;
    callBase[3].toBeClosedValueOffset = 13u;
    callBase[4].toBeClosedValueOffset = 14u;
    state->callInfoList = &state->baseCallInfo;
    TEST_ASSERT_NOT_NULL(ZrCore_CallInfo_Extend(state));
    state->stackTop.valuePointer = callBase + 2u;

    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunction(
            state, callBase, function, 1u, 1u, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase + 5u, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(function, callInfo->metadataFunction);
    TEST_ASSERT_EQUAL_PTR(callInfo, state->callInfoList);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(callBase[2].value.type));
    TEST_ASSERT_EQUAL_INT64(73, callBase[3].value.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(callBase[4].value.type));
    TEST_ASSERT_EQUAL_UINT32(0u, callBase[2].toBeClosedValueOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, callBase[3].toBeClosedValueOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, callBase[4].toBeClosedValueOffset);

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

    RUN_TEST(test_call_info_frame_storage_cache_uses_legacy_padding);
    RUN_TEST(test_precall_clears_reused_frame_slot_metadata);
    RUN_TEST(test_resolved_vm_precall_clears_reused_frame_slot_metadata_with_explicit_argument_count);
    RUN_TEST(test_resolved_vm_precall_keeps_transient_temp_slots_intact_when_no_entry_locals_need_null_reset);
    RUN_TEST(test_resolved_vm_precall_clears_logical_temps_when_typed_frame_layout_is_present);
    RUN_TEST(test_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info);
    RUN_TEST(test_prepared_resolved_vm_precall_exact_args_cached_path_reinitializes_dirty_reused_call_info);
    RUN_TEST(test_prepared_resolved_vm_precall_try_exact_args_steady_state_hits_on_cached_path);
    RUN_TEST(test_resolved_vm_precall_reserves_byte_frame_storage_beyond_logical_slots);
    RUN_TEST(test_resolved_vm_precall_reserves_generated_temp_slots_beyond_declared_stack_size);
    RUN_TEST(test_generated_frame_slot_count_summary_is_finalize_only_and_refreshable);
    RUN_TEST(test_frame_gc_visitor_includes_value_layout_slots);
    RUN_TEST(test_prepared_resolved_vm_precall_reserves_byte_frame_storage_after_fast_probe_fallback);
    RUN_TEST(test_packed_direct_prepared_precall_clears_and_initializes_complete_frame);
    RUN_TEST(test_precall_growth_clears_newly_exposed_entry_local_slots_with_dirty_allocator);
    RUN_TEST(test_precall_growth_reuses_cached_zero_capture_closure_across_repeated_growths_with_dirty_allocator);
    RUN_TEST(test_precall_growth_with_existing_vm_closure_clears_newly_exposed_entry_local_slots_with_dirty_allocator);
    RUN_TEST(test_resolved_vm_precall_preserves_multiple_explicit_arguments_across_repeated_growth_with_dirty_allocator);

    return UNITY_END();
}
