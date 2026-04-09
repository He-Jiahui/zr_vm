#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

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

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tail_reuse_reinitializes_reused_callinfo_state);
    RUN_TEST(test_tail_reuse_with_existing_vm_closure_keeps_callable_object_and_cache_state);

    return UNITY_END();
}
