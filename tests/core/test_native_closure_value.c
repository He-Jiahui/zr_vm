#include <time.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
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

static void test_native_closure_metadata_uses_aot_shim_function(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrFunction *shimFunction;
    TZrStackValuePointer functionBase;
    SZrTypeValue *functionBaseValue;
    SZrFunction *resolvedFunction;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);

    shimFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(shimFunction);
    nativeClosure->aotShimFunction = shimFunction;

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 1, functionBase);
    functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(functionBaseValue);
    ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, functionBaseValue->type);
    TEST_ASSERT_TRUE(functionBaseValue->isNative);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 1;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    resolvedFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, state->callInfoList);

    TEST_ASSERT_EQUAL_PTR(shimFunction, resolvedFunction);

    ZrTests_Runtime_State_Destroy(state);
}

typedef struct TestAotLoaderContext {
    TZrUInt32 aotCalls;
    TZrUInt32 nativeCalls;
    SZrObjectModule *module;
} TestAotLoaderContext;

static SZrObjectModule *test_aot_loader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
    TestAotLoaderContext *context = (TestAotLoaderContext *)userData;

    if (state == ZR_NULL || moduleName == ZR_NULL || context == ZR_NULL) {
        return ZR_NULL;
    }

    context->aotCalls++;
    if (context->module == ZR_NULL) {
        context->module = ZrCore_Module_Create(state);
    }
    return context->module;
}

static SZrObjectModule *test_native_loader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
    TestAotLoaderContext *context = (TestAotLoaderContext *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(moduleName);

    if (context != ZR_NULL) {
        context->nativeCalls++;
    }
    return ZR_NULL;
}

static void test_aot_module_loader_runs_before_native_loader_and_populates_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *modulePath;
    TestAotLoaderContext context;
    SZrObjectModule *firstImport;
    SZrObjectModule *cachedImport;

    TEST_ASSERT_NOT_NULL(state);

    memset(&context, 0, sizeof(context));
    ZrCore_GlobalState_SetAotModuleLoader(state->global, test_aot_loader, &context);
    ZrCore_GlobalState_SetNativeModuleLoader(state->global, test_native_loader, &context);

    modulePath = ZrCore_String_Create(state, "fixtures/aot_entry", strlen("fixtures/aot_entry"));
    TEST_ASSERT_NOT_NULL(modulePath);

    firstImport = ZrCore_Module_ImportByPath(state, modulePath);
    cachedImport = ZrCore_Module_ImportByPath(state, modulePath);

    TEST_ASSERT_NOT_NULL(firstImport);
    TEST_ASSERT_EQUAL_PTR(firstImport, cachedImport);
    TEST_ASSERT_EQUAL_UINT32(1u, context.aotCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, context.nativeCalls);
    TEST_ASSERT_EQUAL_PTR(modulePath, firstImport->moduleName);
    TEST_ASSERT_EQUAL_PTR(modulePath, firstImport->fullPath);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_native_closure_stack_offset_value_accepts_native_closure_type);
    RUN_TEST(test_native_closure_metadata_uses_aot_shim_function);
    RUN_TEST(test_aot_module_loader_runs_before_native_loader_and_populates_cache);

    return UNITY_END();
}
