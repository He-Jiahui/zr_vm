#include "unity.h"

#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_push_to_stack_refreshes_forwarded_function_before_materializing_closure(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    TZrStackValuePointer closureSlot;
    SZrTypeValue *closureValue;
    SZrClosure *closure;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);

    originalFunction->closureValueLength = 0;
    forwardedFunction->closureValueLength = 0;
    forwardedFunction->stackSize = 7;
    forwardedFunction->parameterCount = 1;
    originalFunction->super.garbageCollectMark.forwardingAddress = &forwardedFunction->super;

    closureSlot = state->stackTop.valuePointer;
    state->stackTop.valuePointer = closureSlot + 1;
    ZrCore_Closure_PushToStack(state, originalFunction, ZR_NULL, closureSlot, closureSlot);

    closureValue = ZrCore_Stack_GetValue(closureSlot);
    TEST_ASSERT_NOT_NULL(closureValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, closureValue->type);
    TEST_ASSERT_FALSE(closureValue->isNative);

    closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_EQUAL_PTR(forwardedFunction, closure->function);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_push_to_stack_uses_forwarded_function_capture_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    SZrFunctionClosureVariable forwardedCapture;
    TZrStackValuePointer captureBase;
    TZrStackValuePointer closureSlot;
    SZrTypeValue *capturedStackValue;
    SZrTypeValue *closureValue;
    SZrClosure *closure;
    SZrClosureValue *captureValue;
    SZrTypeValue *capturedValue;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);

    memset(&forwardedCapture, 0, sizeof(forwardedCapture));
    forwardedCapture.inStack = ZR_TRUE;
    forwardedCapture.index = 0;

    originalFunction->closureValueLength = 0;
    originalFunction->closureValueList = ZR_NULL;

    forwardedFunction->closureValueLength = 1;
    forwardedFunction->closureValueList = &forwardedCapture;
    originalFunction->super.garbageCollectMark.forwardingAddress = &forwardedFunction->super;

    captureBase = state->stackTop.valuePointer;
    closureSlot = captureBase + 1;
    state->stackTop.valuePointer = closureSlot + 1;

    capturedStackValue = ZrCore_Stack_GetValue(captureBase);
    TEST_ASSERT_NOT_NULL(capturedStackValue);
    ZrCore_Value_InitAsInt(state, capturedStackValue, 1234);

    ZrCore_Closure_PushToStack(state, originalFunction, ZR_NULL, captureBase, closureSlot);

    closureValue = ZrCore_Stack_GetValue(closureSlot);
    TEST_ASSERT_NOT_NULL(closureValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, closureValue->type);
    TEST_ASSERT_FALSE(closureValue->isNative);

    closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)closure->closureValueCount);

    captureValue = closure->closureValuesExtend[0];
    TEST_ASSERT_NOT_NULL(captureValue);
    capturedValue = ZrCore_ClosureValue_GetValue(captureValue);
    TEST_ASSERT_NOT_NULL(capturedValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, capturedValue->type);
    TEST_ASSERT_EQUAL_INT64(1234, capturedValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_get_metadata_function_refreshes_forwarded_vm_closure(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    SZrClosure *originalClosure;
    SZrClosure *forwardedClosure;
    SZrTypeValue closureValue;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = ZrCore_Function_New(state);
    originalClosure = ZrCore_Closure_New(state, 0);
    forwardedClosure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);
    TEST_ASSERT_NOT_NULL(originalClosure);
    TEST_ASSERT_NOT_NULL(forwardedClosure);

    originalClosure->function = originalFunction;
    forwardedClosure->function = forwardedFunction;
    originalClosure->super.garbageCollectMark.forwardingAddress = &forwardedClosure->super;

    ZrCore_Value_ResetAsNull(&closureValue);
    ZrCore_Value_InitAsRawObject(state, &closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalClosure));
    closureValue.type = ZR_VALUE_TYPE_CLOSURE;
    closureValue.isGarbageCollectable = ZR_TRUE;
    closureValue.isNative = ZR_FALSE;

    TEST_ASSERT_EQUAL_PTR(forwardedFunction,
                          ZrCore_Closure_GetMetadataFunctionFromValue(state, &closureValue));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_precall_known_value_uses_forwarded_vm_closure_metadata_function(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *originalFunction;
    SZrFunction *forwardedFunction;
    SZrClosure *originalClosure;
    SZrClosure *forwardedClosure;
    TZrStackValuePointer callBase;
    SZrTypeValue *callableValue;
    SZrCallInfo *callInfo;

    TEST_ASSERT_NOT_NULL(state);

    originalFunction = ZrCore_Function_New(state);
    forwardedFunction = ZrCore_Function_New(state);
    originalClosure = ZrCore_Closure_New(state, 0);
    forwardedClosure = ZrCore_Closure_New(state, 0);
    TEST_ASSERT_NOT_NULL(originalFunction);
    TEST_ASSERT_NOT_NULL(forwardedFunction);
    TEST_ASSERT_NOT_NULL(originalClosure);
    TEST_ASSERT_NOT_NULL(forwardedClosure);

    originalFunction->stackSize = 1;
    originalFunction->parameterCount = 0;
    originalFunction->closureValueLength = 0;
    forwardedFunction->stackSize = 4;
    forwardedFunction->parameterCount = 0;
    forwardedFunction->closureValueLength = 0;

    originalClosure->function = originalFunction;
    forwardedClosure->function = forwardedFunction;
    originalClosure->super.garbageCollectMark.forwardingAddress = &forwardedClosure->super;

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, 1 + forwardedFunction->stackSize, callBase);
    callableValue = ZrCore_Stack_GetValue(callBase);
    TEST_ASSERT_NOT_NULL(callableValue);

    ZrCore_Value_InitAsRawObject(state, callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(originalClosure));
    callableValue->type = ZR_VALUE_TYPE_CLOSURE;
    callableValue->isGarbageCollectable = ZR_TRUE;
    callableValue->isNative = ZR_FALSE;

    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = callBase + 1;
    callInfo = ZrCore_Function_PreCallKnownValue(state, callBase, callableValue, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + forwardedFunction->stackSize, callInfo->functionTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

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

static void test_resolved_vm_precall_reuses_existing_vm_closure_slot_without_rewriting_callable(void) {
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
    callInfo = ZrCore_Function_PreCallResolvedVmFunction(state, callBase, function, 0, 1, ZR_NULL);

    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_PTR(callBase, callInfo->functionBase.valuePointer);
    TEST_ASSERT_EQUAL_PTR(callBase + 1 + function->stackSize, callInfo->functionTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(callBase)->value.object));
    TEST_ASSERT_EQUAL_PTR(closure, ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer)->value.object));
    TEST_ASSERT_NULL_MESSAGE(function->cachedStatelessClosure,
                             "resolved precall on an existing VM closure must not backfill the stateless function cache");

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_push_to_stack_refreshes_forwarded_function_before_materializing_closure);
    RUN_TEST(test_push_to_stack_uses_forwarded_function_capture_layout);
    RUN_TEST(test_get_metadata_function_refreshes_forwarded_vm_closure);
    RUN_TEST(test_precall_known_value_uses_forwarded_vm_closure_metadata_function);
    RUN_TEST(test_precall_reuses_existing_vm_closure_slot_without_rewriting_callable);
    RUN_TEST(test_resolved_vm_precall_reuses_existing_vm_closure_slot_without_rewriting_callable);

    return UNITY_END();
}
