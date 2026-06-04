#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static TZrUInt32 test_tail_reuse_write_compiled_prototype_data(TZrByte *buffer,
                                                               TZrUInt32 bufferSize,
                                                               const SZrCompiledPrototypeInfo *prototype,
                                                               const SZrCompiledMemberInfo *members,
                                                               TZrUInt32 memberCount) {
    TZrUInt32 prototypeCount = 1u;
    TZrUInt32 cursor = 0u;
    TZrUInt32 memberBytes = sizeof(SZrCompiledMemberInfo) * memberCount;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(bufferSize >= sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + memberBytes);

    memcpy(buffer + cursor, &prototypeCount, sizeof(prototypeCount));
    cursor += (TZrUInt32)sizeof(prototypeCount);
    memcpy(buffer + cursor, prototype, sizeof(*prototype));
    cursor += (TZrUInt32)sizeof(*prototype);
    if (memberBytes > 0u) {
        TEST_ASSERT_NOT_NULL(members);
        memcpy(buffer + cursor, members, memberBytes);
        cursor += memberBytes;
    }

    return cursor;
}

static void test_tail_reuse_install_managed_inline_frame_metadata(SZrState *state,
                                                                  SZrFunction *function,
                                                                  TZrUInt32 stackSlot,
                                                                  TZrUInt32 byteOffset) {
    SZrFunctionFrameSlotLayout *layouts;
    SZrCompiledPrototypeInfo prototype;
    SZrCompiledMemberInfo member;
    TZrUInt32 prototypeDataLength =
            (TZrUInt32)(sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledMemberInfo));
    TZrByte *prototypeData;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);

    layouts[0].stackSlot = stackSlot;
    layouts[0].byteOffset = byteOffset;
    layouts[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layouts[0].byteAlign = ZR_ALIGN_SIZE;
    layouts[0].typeLayoutId = 0u;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[0].isParameter = ZR_FALSE;
    layouts[0].reserved0 = 0u;

    memset(&prototype, 0, sizeof(prototype));
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype.membersCount = 1u;
    prototype.layoutByteSize = (TZrUInt32)sizeof(SZrTypeValue);
    prototype.layoutByteAlign = ZR_ALIGN_SIZE;

    memset(&member, 0, sizeof(member));
    member.memberType = ZR_AST_CONSTANT_STRUCT_FIELD;
    member.fieldOffset = 0u;
    member.fieldSize = (TZrUInt32)sizeof(SZrTypeValue);
    member.isUsingManaged = 1u;
    member.ownershipQualifier = 1u;

    prototypeData = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            state->global,
            prototypeDataLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(prototypeData);

    function->frameSlotLayouts = layouts;
    function->frameSlotLayoutLength = 1u;
    function->frameByteSize = byteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function->frameByteAlign = ZR_ALIGN_SIZE;
    function->prototypeData = prototypeData;
    function->prototypeDataLength = test_tail_reuse_write_compiled_prototype_data(
            prototypeData,
            prototypeDataLength,
            &prototype,
            &member,
            1u);
    function->prototypeCount = 1u;
}

static void test_tail_reuse_reinitializes_reused_callinfo_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *currentFunction;
    SZrFunction *nextFunction;
    TZrInstruction currentInstructions[1] = {0};
    TZrInstruction nextInstructions[1] = {0};
    TZrStackValuePointer callBase;
    TZrStackValuePointer tailCallableSlot;
    SZrTypeValue *currentCallableValue;
    SZrTypeValue *tailCallableValue;
    SZrTypeValue *preparedCallableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    currentFunction = ZrCore_Function_New(state);
    nextFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(currentFunction);
    TEST_ASSERT_NOT_NULL(nextFunction);

    currentFunction->instructionsList = currentInstructions;
    currentFunction->instructionsLength = 1;
    currentFunction->stackSize = 4;
    currentFunction->parameterCount = 0;

    nextFunction->instructionsList = nextInstructions;
    nextFunction->instructionsLength = 1;
    nextFunction->stackSize = 2;
    nextFunction->parameterCount = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + currentFunction->stackSize, callBase);
    currentCallableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(currentCallableValue);

    ZrCore_Value_InitAsRawObject(state, currentCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, currentCallableValue->type);
    TEST_ASSERT_FALSE(currentCallableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, currentCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);

    tailCallableSlot = callInfo->functionBase.valuePointer + 1;
    tailCallableValue = ZrCore_Stack_GetValue(tailCallableSlot);
    TEST_ASSERT_NOT_NULL(tailCallableValue);

    ZrCore_Value_InitAsRawObject(state, tailCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nextFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, tailCallableValue->type);
    TEST_ASSERT_FALSE(tailCallableValue->isNative);

    state->stackTop.valuePointer = tailCallableSlot + 1;
    callInfo->callStatus = ZR_CALL_STATUS_TAIL_CALL;
    callInfo->context.context.trap = ZR_DEBUG_HOOK_MASK_LINE;
    callInfo->yieldContext.transferStart = 7u;
    callInfo->yieldContext.transferCount = 3u;
    state->debugHookSignal = ZR_DEBUG_SIGNAL_NONE;

    TEST_ASSERT_TRUE(ZrCore_Function_TryReuseTailVmCall(state, callInfo, tailCallableSlot));

    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + nextFunction->stackSize, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(nextInstructions, callInfo->context.context.programCounter);
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_SIGNAL_NONE, callInfo->context.context.trap);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)callInfo->context.context.variableArgumentCount);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     callInfo->yieldContext.transferStart,
                                     "tail reuse must clear stale transfer start");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     callInfo->yieldContext.transferCount,
                                     "tail reuse must clear stale transfer count");
    TEST_ASSERT_TRUE_MESSAGE((callInfo->callStatus & ZR_CALL_STATUS_TAIL_CALL) != 0,
                             "tail reuse must preserve tail-call status");
    TEST_ASSERT_TRUE(callInfo->hasReturnDestination);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->returnDestination);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, state->stackTop.valuePointer);

    preparedCallableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(preparedCallableValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, preparedCallableValue->type);
    TEST_ASSERT_FALSE(preparedCallableValue->isNative);
    TEST_ASSERT_NOT_NULL(nextFunction->cachedStatelessClosure);
    TEST_ASSERT_EQUAL_PTR(nextFunction->cachedStatelessClosure,
                          ZR_CAST_VM_CLOSURE(state, preparedCallableValue->value.object));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_tail_reuse_drops_inline_frame_values_before_reusing_storage(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *currentFunction;
    SZrFunction *nextFunction;
    SZrString *text;
    TZrInstruction currentInstructions[1] = {0};
    TZrInstruction nextInstructions[1] = {0};
    TZrStackValuePointer callBase;
    TZrStackValuePointer frameBase;
    TZrStackValuePointer tailCallableSlot;
    SZrStackFramePlace inlinePlace;
    SZrTypeValue *currentCallableValue;
    SZrTypeValue *tailCallableValue;
    SZrTypeValue *inlineValue;
    SZrCallInfo *callInfo;
    TZrUInt32 inlineByteOffset =
            3u * (TZrUInt32)sizeof(SZrTypeValueOnStack) - ZR_ALIGN_SIZE;

    TEST_ASSERT_NOT_NULL(state);

    currentFunction = ZrCore_Function_New(state);
    nextFunction = ZrCore_Function_New(state);
    text = ZrCore_String_CreateFromNative(state, "tail-reuse-inline-drop");
    TEST_ASSERT_NOT_NULL(currentFunction);
    TEST_ASSERT_NOT_NULL(nextFunction);
    TEST_ASSERT_NOT_NULL(text);

    currentFunction->instructionsList = currentInstructions;
    currentFunction->instructionsLength = 1;
    currentFunction->stackSize = 4;
    currentFunction->parameterCount = 0;
    test_tail_reuse_install_managed_inline_frame_metadata(state, currentFunction, 2u, inlineByteOffset);

    nextFunction->instructionsList = nextInstructions;
    nextFunction->instructionsLength = 1;
    nextFunction->stackSize = 1;
    nextFunction->parameterCount = 0;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(
            state,
            1u + ZrCore_Function_GetFrameStorageSlotCount(currentFunction),
            callBase);
    currentCallableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(currentCallableValue);
    ZrCore_Value_InitAsRawObject(state, currentCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, currentCallableValue->type);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, currentCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    frameBase = callInfo->functionBase.valuePointer + 1;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, currentFunction, frameBase, 2u, &inlinePlace));
    TEST_ASSERT_TRUE(ZrCore_Function_FrameStackSlotIntersectsInlineStruct(currentFunction, frameBase, frameBase + 2));
    TEST_ASSERT_TRUE(ZrCore_Function_FrameStackSlotIntersectsInlineStruct(currentFunction, frameBase, frameBase + 3));

    memset(frameBase + 2, 0xA5, 2u * sizeof(SZrTypeValueOnStack));
    inlineValue = (SZrTypeValue *)inlinePlace.address;
    ZrCore_Value_InitAsRawObject(state, inlineValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    inlineValue->type = ZR_VALUE_TYPE_STRING;

    tailCallableSlot = callInfo->functionBase.valuePointer + 1;
    tailCallableValue = ZrCore_Stack_GetValue(tailCallableSlot);
    TEST_ASSERT_NOT_NULL(tailCallableValue);
    ZrCore_Value_InitAsRawObject(state, tailCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nextFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, tailCallableValue->type);

    state->stackTop.valuePointer = tailCallableSlot + 1;
    callInfo->callStatus = ZR_CALL_STATUS_TAIL_CALL;
    state->debugHookSignal = ZR_DEBUG_SIGNAL_NONE;

    TEST_ASSERT_TRUE(ZrCore_Function_TryReuseTailVmCall(state, callInfo, tailCallableSlot));

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, inlineValue->type);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_tail_reuse_with_existing_vm_closure_keeps_callable_object_and_cache_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *currentFunction;
    SZrFunction *nextFunction;
    SZrClosure *closure;
    TZrInstruction currentInstructions[1] = {0};
    TZrInstruction nextInstructions[1] = {0};
    TZrStackValuePointer callBase;
    TZrStackValuePointer tailCallableSlot;
    SZrTypeValue *currentCallableValue;
    SZrTypeValue *tailCallableValue;
    SZrTypeValue *preparedCallableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    currentFunction = ZrCore_Function_New(state);
    nextFunction = ZrCore_Function_New(state);
    closure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(currentFunction);
    TEST_ASSERT_NOT_NULL(nextFunction);
    TEST_ASSERT_NOT_NULL(closure);

    currentFunction->instructionsList = currentInstructions;
    currentFunction->instructionsLength = 1;
    currentFunction->stackSize = 4;
    currentFunction->parameterCount = 0;

    nextFunction->instructionsList = nextInstructions;
    nextFunction->instructionsLength = 1;
    nextFunction->stackSize = 2;
    nextFunction->parameterCount = 0;
    nextFunction->closureValueLength = 0;
    closure->function = nextFunction;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + currentFunction->stackSize, callBase);
    currentCallableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(currentCallableValue);

    ZrCore_Value_InitAsRawObject(state, currentCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, currentCallableValue->type);
    TEST_ASSERT_FALSE(currentCallableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, currentCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    tailCallableSlot = callInfo->functionBase.valuePointer + 1;
    tailCallableValue = ZrCore_Stack_GetValue(tailCallableSlot);
    TEST_ASSERT_NOT_NULL(tailCallableValue);

    ZrCore_Value_InitAsRawObject(state, tailCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    tailCallableValue->type = ZR_VALUE_TYPE_CLOSURE;
    tailCallableValue->isGarbageCollectable = ZR_TRUE;
    tailCallableValue->isNative = ZR_FALSE;

    state->stackTop.valuePointer = tailCallableSlot + 1;
    callInfo->callStatus = ZR_CALL_STATUS_TAIL_CALL;

    TEST_ASSERT_TRUE(ZrCore_Function_TryReuseTailVmCall(state, callInfo, tailCallableSlot));

    preparedCallableValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(preparedCallableValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, preparedCallableValue->type);
    TEST_ASSERT_FALSE(preparedCallableValue->isNative);
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, preparedCallableValue->value.object));
    TEST_ASSERT_NULL_MESSAGE(nextFunction->cachedStatelessClosure,
                             "tail reuse on an existing VM closure must not backfill the stateless function cache");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_tail_reuse_declines_inline_parameter_callee_until_layout_move_is_available(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *currentFunction;
    SZrFunction *nextFunction;
    SZrFunctionFrameSlotLayout *nextLayouts;
    TZrInstruction currentInstructions[1] = {0};
    TZrInstruction nextInstructions[1] = {0};
    TZrStackValuePointer callBase;
    TZrStackValuePointer tailCallableSlot;
    SZrTypeValue *currentCallableValue;
    SZrTypeValue *tailCallableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    currentFunction = ZrCore_Function_New(state);
    nextFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(currentFunction);
    TEST_ASSERT_NOT_NULL(nextFunction);

    nextLayouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(nextLayouts);
    memset(nextLayouts, 0, sizeof(SZrFunctionFrameSlotLayout));

    currentFunction->instructionsList = currentInstructions;
    currentFunction->instructionsLength = 1;
    currentFunction->stackSize = 4;
    currentFunction->parameterCount = 0;

    nextLayouts[0].stackSlot = 0u;
    nextLayouts[0].byteOffset = 16u;
    nextLayouts[0].byteSize = 12u;
    nextLayouts[0].byteAlign = 4u;
    nextLayouts[0].typeLayoutId = 0u;
    nextLayouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    nextLayouts[0].isParameter = ZR_TRUE;

    nextFunction->instructionsList = nextInstructions;
    nextFunction->instructionsLength = 1;
    nextFunction->stackSize = 1;
    nextFunction->parameterCount = 1;
    nextFunction->frameSlotLayouts = nextLayouts;
    nextFunction->frameSlotLayoutLength = 1u;
    nextFunction->frameByteSize = 28u;
    nextFunction->frameByteAlign = 4u;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + currentFunction->stackSize, callBase);
    currentCallableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(currentCallableValue);
    ZrCore_Value_InitAsRawObject(state, currentCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, currentCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    tailCallableSlot = callInfo->functionBase.valuePointer + 1;
    tailCallableValue = ZrCore_Stack_GetValue(tailCallableSlot);
    TEST_ASSERT_NOT_NULL(tailCallableValue);
    ZrCore_Value_InitAsRawObject(state, tailCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nextFunction));

    state->stackTop.valuePointer = tailCallableSlot + 2;
    callInfo->callStatus = ZR_CALL_STATUS_TAIL_CALL;
    state->debugHookSignal = ZR_DEBUG_SIGNAL_NONE;

    TEST_ASSERT_FALSE(ZrCore_Function_TryReuseTailVmCall(state, callInfo, tailCallableSlot));
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(tailCallableSlot + 2, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(currentInstructions, callInfo->context.context.programCounter);
    TEST_ASSERT_TRUE((callInfo->callStatus & ZR_CALL_STATUS_TAIL_CALL) != 0u);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tail_reuse_reinitializes_reused_callinfo_state);
    RUN_TEST(test_tail_reuse_drops_inline_frame_values_before_reusing_storage);
    RUN_TEST(test_tail_reuse_with_existing_vm_closure_keeps_callable_object_and_cache_state);
    RUN_TEST(test_tail_reuse_declines_inline_parameter_callee_until_layout_move_is_available);

    return UNITY_END();
}
