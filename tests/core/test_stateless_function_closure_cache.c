#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_precall_reuses_zero_capture_function_closure_across_distinct_call_sites(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer stackBase;
    TZrStackValuePointer firstCallBase;
    TZrStackValuePointer secondCallBase;
    SZrTypeValue *firstCallableValue;
    SZrTypeValue *secondCallableValue;
    SZrCallInfo *firstCallInfo;
    SZrCallInfo *secondCallInfo;
    SZrTypeValue *firstPreparedCallable;
    SZrTypeValue *secondPreparedCallable;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, function->closureValueLength);

    stackBase = state->stackTop.valuePointer;
    stackBase = ZrCore_Function_CheckStackAndGc(state, 4, stackBase);
    firstCallBase = stackBase;
    secondCallBase = stackBase + 2;

    firstCallableValue = ZrCore_Stack_GetValue(firstCallBase);
    secondCallableValue = ZrCore_Stack_GetValue(secondCallBase);
    TEST_ASSERT_NOT_NULL(firstCallableValue);
    TEST_ASSERT_NOT_NULL(secondCallableValue);

    ZrCore_Value_InitAsRawObject(state, firstCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, firstCallableValue->type);
    TEST_ASSERT_FALSE(firstCallableValue->isNative);

    ZrCore_Value_InitAsRawObject(state, secondCallableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, secondCallableValue->type);
    TEST_ASSERT_FALSE(secondCallableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = firstCallBase + 1;
    firstCallInfo = ZrCore_Function_PreCallKnownValue(state, firstCallBase, firstCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(firstCallInfo);

    firstPreparedCallable = ZrCore_Stack_GetValue(firstCallInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(firstPreparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, firstPreparedCallable->type);
    TEST_ASSERT_FALSE(firstPreparedCallable->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = secondCallBase + 1;
    secondCallInfo = ZrCore_Function_PreCallKnownValue(state, secondCallBase, secondCallableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(secondCallInfo);

    secondPreparedCallable = ZrCore_Stack_GetValue(secondCallInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(secondPreparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, secondPreparedCallable->type);
    TEST_ASSERT_FALSE(secondPreparedCallable->isNative);

    TEST_ASSERT_EQUAL_PTR(firstPreparedCallable->value.object, secondPreparedCallable->value.object);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_known_vm_precall_keeps_zero_capture_function_value_materialized_as_function(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer stackBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    SZrTypeValue *preparedCallable;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, function->closureValueLength);
    TEST_ASSERT_NULL(function->cachedStatelessClosure);

    stackBase = state->stackTop.valuePointer;
    stackBase = ZrCore_Function_CheckStackAndGc(state, 2, stackBase);
    callableValue = ZrCore_Stack_GetValue(stackBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);
    TEST_ASSERT_FALSE(callableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = stackBase + 1;
    callInfo = ZrCore_Function_PreCallKnownVmValue(state, stackBase, callableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    preparedCallable = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(preparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, preparedCallable->type);
    TEST_ASSERT_FALSE(preparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(function), preparedCallable->value.object);
    TEST_ASSERT_NULL(function->cachedStatelessClosure);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_known_vm_precall_refreshes_forwarded_zero_capture_function_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    TZrStackValuePointer stackBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    SZrTypeValue *preparedCallable;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);
    TEST_ASSERT_EQUAL_UINT32(0u, originalFunction->closureValueLength);
    TEST_ASSERT_EQUAL_UINT32(0u, forwardedFunction->closureValueLength);
    TEST_ASSERT_NULL(originalFunction->cachedStatelessClosure);
    TEST_ASSERT_NULL(forwardedFunction->cachedStatelessClosure);

    originalFunction->super.garbageCollectMark.forwardingAddress = &forwardedFunction->super;

    stackBase = state->stackTop.valuePointer;
    stackBase = ZrCore_Function_CheckStackAndGc(state, 2, stackBase);
    callableValue = ZrCore_Stack_GetValue(stackBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalFunction));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);
    TEST_ASSERT_FALSE(callableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = stackBase + 1;
    callInfo = ZrCore_Function_PreCallKnownVmValue(state, stackBase, callableValue, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    preparedCallable = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(preparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, preparedCallable->type);
    TEST_ASSERT_FALSE(preparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedFunction), preparedCallable->value.object);
    TEST_ASSERT_NULL(originalFunction->cachedStatelessClosure);
    TEST_ASSERT_NULL(forwardedFunction->cachedStatelessClosure);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_resolved_vm_precall_keeps_zero_capture_function_value_materialized_as_function(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrStackValuePointer stackBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;
    SZrTypeValue *preparedCallable;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, function->closureValueLength);
    TEST_ASSERT_NULL(function->cachedStatelessClosure);

    stackBase = state->stackTop.valuePointer;
    stackBase = ZrCore_Function_CheckStackAndGc(state, 2, stackBase);
    callableValue = ZrCore_Stack_GetValue(stackBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, callableValue->type);
    TEST_ASSERT_FALSE(callableValue->isNative);

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = stackBase + 1;
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, stackBase, function, 0, 1, ZR_NULL);
    TEST_ASSERT_NOT_NULL(callInfo);

    preparedCallable = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(preparedCallable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_FUNCTION, preparedCallable->type);
    TEST_ASSERT_FALSE(preparedCallable->isNative);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(function), preparedCallable->value.object);
    TEST_ASSERT_NULL(function->cachedStatelessClosure);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_precall_reuses_zero_capture_function_closure_across_distinct_call_sites);
    RUN_TEST(test_known_vm_precall_keeps_zero_capture_function_value_materialized_as_function);
    RUN_TEST(test_known_vm_precall_refreshes_forwarded_zero_capture_function_value);
    RUN_TEST(test_resolved_vm_precall_keeps_zero_capture_function_value_materialized_as_function);

    return UNITY_END();
}
