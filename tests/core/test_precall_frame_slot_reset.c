#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_precall_clears_reused_frame_slot_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 3;
    function->parameterCount = 0;

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

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_precall_clears_reused_frame_slot_metadata);

    return UNITY_END();
}
