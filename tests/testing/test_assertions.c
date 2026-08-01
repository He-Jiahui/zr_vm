#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_testing/module.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compiler.h"

#include <string.h>

static SZrState *create_testing_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibTesting_Register(state->global));
    return state;
}

static void reset_exception(SZrState *state) {
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
}

static const ZrLibFunctionDescriptor *find_function(
        const ZrLibModuleDescriptor *descriptor,
        const TZrChar *name) {
    for (TZrSize index = 0U; descriptor != ZR_NULL && index < descriptor->functionCount; index++) {
        if (strcmp(descriptor->functions[index].name, name) == 0) {
            return &descriptor->functions[index];
        }
    }
    return ZR_NULL;
}

static const ZrLibAttributeRoleDescriptor *find_role(
        const ZrLibModuleDescriptor *descriptor,
        TZrUInt32 role) {
    for (TZrSize index = 0U; descriptor != ZR_NULL && index < descriptor->attributeRoleCount; index++) {
        if (descriptor->attributeRoles[index].role == role) {
            return &descriptor->attributeRoles[index];
        }
    }
    return ZR_NULL;
}

static void test_descriptor_is_the_test_phase_contract_source(void) {
    const ZrLibModuleDescriptor *descriptor = ZrVmLibTesting_GetModuleDescriptor();
    const TZrUInt32 roles[] = {
            ZR_PARSER_ATTRIBUTE_ROLE_TEST,
            ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE,
            ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP,
    };

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING("zr.testing", descriptor->moduleName);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_TEST, descriptor->providerPhase);
    TEST_ASSERT_EQUAL_UINT32(3U, descriptor->functionCount);
    TEST_ASSERT_EQUAL_UINT32(4U, descriptor->typeCount);
    TEST_ASSERT_EQUAL_UINT32(3U, descriptor->attributeRoleCount);
    TEST_ASSERT_EQUAL_STRING(
            "zr.testing:v1:test-case-skip:assert-equal-throws",
            descriptor->publicContractHash);
    TEST_ASSERT_NOT_NULL(find_function(descriptor, "assert"));
    TEST_ASSERT_NOT_NULL(find_function(descriptor, "equal"));
    TEST_ASSERT_NOT_NULL(find_function(descriptor, "throws"));

    for (TZrSize index = 0U; index < ZR_ARRAY_COUNT(roles); index++) {
        const ZrLibAttributeRoleDescriptor *nativeRole = find_role(descriptor, roles[index]);
        const SZrParserAttributeSchema *parserRole =
                ZrParser_AttributeContract_FindBuiltinByRole((EZrParserAttributeRole)roles[index]);

        TEST_ASSERT_NOT_NULL(nativeRole);
        TEST_ASSERT_NOT_NULL(parserRole);
        TEST_ASSERT_EQUAL_STRING(parserRole->qualifiedName, nativeRole->qualifiedName);
        TEST_ASSERT_EQUAL_UINT32(parserRole->attributeId, nativeRole->attributeId);
        TEST_ASSERT_EQUAL_UINT32(parserRole->usage.targets, nativeRole->targetFlags);
        TEST_ASSERT_EQUAL_UINT32(parserRole->usage.retention, nativeRole->retention);
        TEST_ASSERT_EQUAL(parserRole->usage.repeatable, nativeRole->repeatable);
        TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_TEST, parserRole->providerPhase);
    }
}

static void test_runtime_phase_rejects_testing_and_test_phase_materializes_it(void) {
    SZrState *state = create_testing_state();

    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
                      ZrLibrary_State_GetProviderPhase(state));
    TEST_ASSERT_NULL(ZrLib_Module_GetExport(state, "zr.testing", "assert"));
    TEST_ASSERT_EQUAL(
            ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH,
            ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    TEST_ASSERT_NOT_NULL(strstr(
            ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global),
            "provider phase mismatch"));

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL);
    TEST_ASSERT_NULL(ZrLib_Module_GetExport(state, "zr.testing", "assert"));
    TEST_ASSERT_EQUAL(
            ZR_LIB_NATIVE_REGISTRY_ERROR_PHASE_MISMATCH,
            ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    TEST_ASSERT_NOT_NULL(ZrLib_Module_GetExport(state, "zr.testing", "assert"));
    TEST_ASSERT_NOT_NULL(ZrLib_Type_FindPrototype(state, "AssertionFailure"));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_assert_success_and_structured_failure(void) {
    SZrState *state = create_testing_state();
    SZrTypeValue arguments[2];
    SZrTypeValue result;
    SZrTestingAssertionFailure failure;
    SZrObject *exceptionObject;

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    ZrLib_Value_SetBool(state, &arguments[0], ZR_TRUE);
    ZrLib_Value_SetString(state, &arguments[1], "ok");
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(
            state, "zr.testing", "assert", arguments, 2U, &result));
    TEST_ASSERT_FALSE(ZrVmLibTesting_GetLastFailure(&failure));

    ZrLib_Value_SetBool(state, &arguments[0], ZR_FALSE);
    ZrLib_Value_SetString(state, &arguments[1], "bounded assertion");
    TEST_ASSERT_FALSE(ZrLib_CallModuleExport(
            state, "zr.testing", "assert", arguments, 2U, &result));
    TEST_ASSERT_TRUE(state->hasCurrentException);
    TEST_ASSERT_TRUE(ZrVmLibTesting_GetLastFailure(&failure));
    TEST_ASSERT_EQUAL(ZR_TESTING_ASSERTION_KIND_ASSERT, failure.assertionKind);
    TEST_ASSERT_EQUAL_STRING("bounded assertion", failure.message);
    TEST_ASSERT_EQUAL(ZR_VALUE_TYPE_OBJECT, state->currentException.type);
    exceptionObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    TEST_ASSERT_NOT_NULL(exceptionObject);
    TEST_ASSERT_NOT_NULL(exceptionObject->prototype);
    TEST_ASSERT_EQUAL_STRING(
            "AssertionFailure",
            ZrCore_String_GetNativeString(exceptionObject->prototype->name));

    reset_exception(state);
    ZrVmLibTesting_ClearLastFailure();
    ZrTests_Runtime_State_Destroy(state);
}

static void test_equal_uses_canonical_equality_and_bounded_snapshots(void) {
    SZrState *state = create_testing_state();
    SZrTypeValue arguments[2];
    SZrTypeValue result;
    SZrTestingAssertionFailure failure;
    TZrChar longText[ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY + 80U];

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    ZrLib_Value_SetInt(state, &arguments[0], 7);
    ZrLib_Value_SetInt(state, &arguments[1], 7);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(
            state, "zr.testing", "equal", arguments, 2U, &result));

    memset(longText, 'x', sizeof(longText) - 1U);
    longText[sizeof(longText) - 1U] = '\0';
    ZrLib_Value_SetString(state, &arguments[0], longText);
    ZrLib_Value_SetString(state, &arguments[1], "expected");
    TEST_ASSERT_FALSE(ZrLib_CallModuleExport(
            state, "zr.testing", "equal", arguments, 2U, &result));
    TEST_ASSERT_TRUE(ZrVmLibTesting_GetLastFailure(&failure));
    TEST_ASSERT_EQUAL(ZR_TESTING_ASSERTION_KIND_EQUAL, failure.assertionKind);
    TEST_ASSERT_TRUE(failure.actual.truncated);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
            ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY,
            strlen(failure.actual.text));
    TEST_ASSERT_EQUAL_STRING("expected", failure.expected.text);

    reset_exception(state);
    ZrVmLibTesting_ClearLastFailure();
    ZrTests_Runtime_State_Destroy(state);
}

static TZrInt64 test_action_noop(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase =
            callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;

    TEST_ASSERT_NOT_NULL(functionBase);
    ZrLib_Value_SetNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 test_action_throws(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase =
            callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;

    TEST_ASSERT_NOT_NULL(functionBase);
    ZrLib_Value_SetNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    ZrCore_Debug_RunError(state, "expected action failure");
    return 1;
}

static TZrInt64 test_formatter_throws(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase =
            callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;

    TEST_ASSERT_NOT_NULL(functionBase);
    ZrLib_Value_SetNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    ZrCore_Debug_RunError(state, "formatter fault probe");
    return 1;
}

static void init_native_callable(SZrState *state,
                                 FZrNativeFunction callback,
                                 SZrTypeValue *value) {
    SZrClosureNative *closure = ZrCore_ClosureNative_New(state, 0U);

    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = callback;
    ZrCore_Value_InitAsRawObject(
            state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isNative = ZR_TRUE;
    value->isGarbageCollectable = ZR_TRUE;
}

static void test_throws_returns_exception_and_rejects_non_throwing_action(void) {
    SZrState *state = create_testing_state();
    SZrTypeValue action;
    SZrTypeValue result;
    SZrTestingAssertionFailure failure;

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    init_native_callable(state, test_action_throws, &action);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(
            state, "zr.testing", "throws", &action, 1U, &result));
    TEST_ASSERT_EQUAL(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_FALSE(state->hasCurrentException);

    init_native_callable(state, test_action_noop, &action);
    TEST_ASSERT_FALSE(ZrLib_CallModuleExport(
            state, "zr.testing", "throws", &action, 1U, &result));
    TEST_ASSERT_TRUE(ZrVmLibTesting_GetLastFailure(&failure));
    TEST_ASSERT_EQUAL(ZR_TESTING_ASSERTION_KIND_THROWS, failure.assertionKind);
    TEST_ASSERT_NOT_NULL(strstr(failure.message, "without throwing"));

    reset_exception(state);
    ZrVmLibTesting_ClearLastFailure();
    ZrTests_Runtime_State_Destroy(state);
}

static void test_equal_isolates_formatter_faults(void) {
    SZrState *state = create_testing_state();
    SZrTypeValue arguments[2];
    SZrTypeValue result;
    SZrTestingAssertionFailure failure;
    SZrClosureNative *formatterClosure;
    SZrMeta throwingMeta;
    SZrMeta *previousMeta;
    SZrObjectPrototype *boolPrototype;

    ZrLibrary_State_SetProviderPhase(state, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    boolPrototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_BOOL];
    TEST_ASSERT_NOT_NULL(boolPrototype);
    formatterClosure = ZrCore_ClosureNative_New(state, 0U);
    TEST_ASSERT_NOT_NULL(formatterClosure);
    formatterClosure->nativeFunction = test_formatter_throws;
    ZrCore_RawObject_MarkAsPermanent(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(formatterClosure));
    throwingMeta.metaType = ZR_META_TO_STRING;
    throwingMeta.function = ZR_CAST(
            SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(formatterClosure));
    previousMeta = boolPrototype->metaTable.metas[ZR_META_TO_STRING];
    boolPrototype->metaTable.metas[ZR_META_TO_STRING] = &throwingMeta;

    ZrLib_Value_SetBool(state, &arguments[0], ZR_FALSE);
    ZrLib_Value_SetBool(state, &arguments[1], ZR_TRUE);
    TEST_ASSERT_FALSE(ZrLib_CallModuleExport(
            state, "zr.testing", "equal", arguments, 2U, &result));
    boolPrototype->metaTable.metas[ZR_META_TO_STRING] = previousMeta;

    TEST_ASSERT_TRUE(ZrVmLibTesting_GetLastFailure(&failure));
    TEST_ASSERT_TRUE(failure.expected.formatterFaulted);
    TEST_ASSERT_TRUE(failure.actual.formatterFaulted);
    TEST_ASSERT_EQUAL_STRING("<format-error>", failure.expected.text);
    TEST_ASSERT_EQUAL_STRING("<format-error>", failure.actual.text);

    reset_exception(state);
    ZrVmLibTesting_ClearLastFailure();
    ZrTests_Runtime_State_Destroy(state);
}

static void test_compiler_enforces_testing_provider_phase(void) {
    static const TZrChar *source =
            "let testing = import(\"zr.testing\");\n"
            "testing.assert(true);\n";
    SZrState *state = create_testing_state();
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "testing_phase.zr");
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL(function);

    function = ZrParser_Source_CompileTest(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(state, function);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_descriptor_is_the_test_phase_contract_source);
    RUN_TEST(test_runtime_phase_rejects_testing_and_test_phase_materializes_it);
    RUN_TEST(test_assert_success_and_structured_failure);
    RUN_TEST(test_equal_uses_canonical_equality_and_bounded_snapshots);
    RUN_TEST(test_throws_returns_exception_and_rejects_non_throwing_action);
    RUN_TEST(test_equal_isolates_formatter_faults);
    RUN_TEST(test_compiler_enforces_testing_provider_phase);
    return UNITY_END();
}
