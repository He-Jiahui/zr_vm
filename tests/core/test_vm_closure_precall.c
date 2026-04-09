#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_precall_reuses_existing_vm_closure_slot_without_rewriting_callable(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 2;
    function->parameterCount = 0;
    function->closureValueLength = 0;

    closure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + function->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callableValue->type = ZR_VALUE_TYPE_CLOSURE;
    callableValue->isGarbageCollectable = ZR_TRUE;
    callableValue->isNative = ZR_FALSE;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, callableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + function->stackSize, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(callBase)->value.object));
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer)->value.object));
    TEST_ASSERT_NULL_MESSAGE(function->cachedStatelessClosure,
                             "precall on an existing VM closure must not backfill the stateless function cache");

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_precall_reuses_existing_vm_closure_slot_without_rewriting_callable);

    return UNITY_END();
}
