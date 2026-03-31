#include <time.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_native_closure_stack_offset_value_accepts_native_closure_type(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer functionBase;
    SZrTypeValue *functionBaseValue;
    SZrClosureNative *nativeClosure;
    SZrTypeValue capturedValue;
    SZrTypeValue *resolvedValue;

    TEST_ASSERT_NOT_NULL(state);

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 1, functionBase);
    functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(functionBaseValue);

    nativeClosure = ZrCore_ClosureNative_New(state, 1);
    TEST_ASSERT_NOT_NULL(nativeClosure);

    ZrCore_Value_InitAsInt(state, &capturedValue, 42);
    nativeClosure->closureValuesExtend[0] = &capturedValue;

    ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, functionBaseValue->type);
    TEST_ASSERT_TRUE(functionBaseValue->isNative);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 1;
    state->baseCallInfo.previous = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    resolvedValue = ZrCore_Value_GetStackOffsetValue(state, ZR_VM_STACK_GLOBAL_MODULE_REGISTRY - 1);

    TEST_ASSERT_NOT_NULL(resolvedValue);
    TEST_ASSERT_EQUAL_PTR(&capturedValue, resolvedValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resolvedValue->type));
    TEST_ASSERT_EQUAL_INT64(42, resolvedValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_native_closure_stack_offset_value_accepts_native_closure_type);

    return UNITY_END();
}
