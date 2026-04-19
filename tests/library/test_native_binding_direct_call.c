#include "unity.h"

#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser.h"

typedef ZrTestsFixtureSource ZrTestsModuleFixtureSource;

#define MODULE_FIXTURE_SOURCE_TEXT(pathValue, sourceValue) ZR_TESTS_FIXTURE_SOURCE_TEXT(pathValue, sourceValue)
#define get_object_field_value ZrTests_Fixture_GetObjectFieldValue

void setUp(void) {}

void tearDown(void) {}

static const ZrTestsModuleFixtureSource *g_module_fixture_sources = ZR_NULL;
static TZrSize g_module_fixture_source_count = 0;

static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
    }

    return state;
}

static TZrBool module_fixture_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(state,
                                                 sourcePath,
                                                 md5,
                                                 io,
                                                 g_module_fixture_sources,
                                                 g_module_fixture_source_count);
}

static const TZrChar *current_exception_message(SZrState *state) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || !state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    errorObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    if (errorObject == ZR_NULL) {
        return ZR_NULL;
    }

    messageValue = get_object_field_value(state, errorObject, "message");
    if (messageValue == ZR_NULL || messageValue->type != ZR_VALUE_TYPE_STRING || messageValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object));
}

static TZrBool host_demo_bump_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 left = 0;
    TZrInt64 right = 0;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_CheckArity(context, 2, 2) || !ZrLib_CallContext_ReadInt(context, 0, &left) ||
        !ZrLib_CallContext_ReadInt(context, 1, &right)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, result, left + right);
    return ZR_TRUE;
}

static const ZrLibParameterDescriptor kHostDemoBumpParameters[] = {
        {"left", "int", "left operand"},
        {"right", "int", "right operand"},
};

static const ZrLibFunctionDescriptor kHostDemoFunctions[] = {
        {
                .name = "bump",
                .minArgumentCount = 2,
                .maxArgumentCount = 2,
                .callback = host_demo_bump_callback,
                .returnTypeName = "int",
                .documentation = "Adds two values.",
                .parameters = kHostDemoBumpParameters,
                .parameterCount = sizeof(kHostDemoBumpParameters) / sizeof(kHostDemoBumpParameters[0]),
        },
};

static const ZrLibConstantDescriptor kHostDemoConstants[] = {
        {
                .name = "answer",
                .kind = ZR_LIB_CONSTANT_KIND_INT,
                .intValue = 100,
                .documentation = "Answer constant.",
                .typeName = "int",
        },
};

static const ZrLibModuleDescriptor kHostDemoModuleDescriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "host_demo",
        .constants = kHostDemoConstants,
        .constantCount = sizeof(kHostDemoConstants) / sizeof(kHostDemoConstants[0]),
        .functions = kHostDemoFunctions,
        .functionCount = sizeof(kHostDemoFunctions) / sizeof(kHostDemoFunctions[0]),
        .types = ZR_NULL,
        .typeCount = 0,
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Host demo module.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = ZR_NULL,
};

static void test_direct_module_export_succeeds_for_source_module_function(void) {
    static const ZrTestsModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "simple_direct_export",
                    "pub replay(): int {\n"
                    "    return 42;\n"
                    "}\n"
                    "\n"
                    "return 0;\n")
    };
    SZrState *state = create_test_state();
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    g_module_fixture_sources = kFixtures;
    g_module_fixture_source_count = sizeof(kFixtures) / sizeof(kFixtures[0]);
    state->global->sourceLoader = module_fixture_source_loader;

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state, "simple_direct_export", "replay", ZR_NULL, 0, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = ZR_NULL;
    g_module_fixture_source_count = 0;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_module_export_runtime_error_returns_false_instead_of_aborting(void) {
    static const ZrTestsModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "broken_direct_export",
                    "pub broken(): int {\n"
                    "    var value = 1;\n"
                    "    return value.answer;\n"
                    "}\n"
                    "\n"
                    "return 0;\n")
    };
    SZrState *state = create_test_state();
    SZrTypeValue result;
    const TZrChar *message;

    TEST_ASSERT_NOT_NULL(state);
    g_module_fixture_sources = kFixtures;
    g_module_fixture_source_count = sizeof(kFixtures) / sizeof(kFixtures[0]);
    state->global->sourceLoader = module_fixture_source_loader;

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrLib_CallModuleExport(state, "broken_direct_export", "broken", ZR_NULL, 0, &result));
    TEST_ASSERT_NOT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_TRUE(state->hasCurrentException);
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "receiver must be an object"));

    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = ZR_NULL;
    g_module_fixture_source_count = 0;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_module_export_preserves_scalar_captures_after_tail_called_entry(void) {
    static const ZrTestsModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "captured_scalar_tail_entry",
                    "var captured = 100;\n"
                    "\n"
                    "pub replay(): int {\n"
                    "    return captured + 5;\n"
                    "}\n"
                    "\n"
                    "return replay();\n")
    };
    SZrState *state = create_test_state();
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    g_module_fixture_sources = kFixtures;
    g_module_fixture_source_count = sizeof(kFixtures) / sizeof(kFixtures[0]);
    state->global->sourceLoader = module_fixture_source_loader;

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLib_CallModuleExport(state, "captured_scalar_tail_entry", "replay", ZR_NULL, 0, &result),
                             current_exception_message(state));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = ZR_NULL;
    g_module_fixture_source_count = 0;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_module_export_preserves_imported_native_module_captures(void) {
    static const ZrTestsModuleFixtureSource kFixtures[] = {
            MODULE_FIXTURE_SOURCE_TEXT(
                    "captured_native_import",
                    "var host = %import(\"host_demo\");\n"
                    "\n"
                    "pub replay(): int {\n"
                    "    return host.answer + host.bump(2, 3);\n"
                    "}\n"
                    "\n"
                    "return replay();\n")
    };
    SZrState *state = create_test_state();
    SZrString *moduleName = ZR_NULL;
    SZrObjectModule *module = ZR_NULL;
    const SZrTypeValue *exportValue = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &kHostDemoModuleDescriptor));
    g_module_fixture_sources = kFixtures;
    g_module_fixture_source_count = sizeof(kFixtures) / sizeof(kFixtures[0]);
    state->global->sourceLoader = module_fixture_source_loader;

    moduleName = ZrCore_String_Create(state, "captured_native_import", strlen("captured_native_import"));
    TEST_ASSERT_NOT_NULL(moduleName);
    module = ZrCore_Module_ImportByPath(state, moduleName);
    TEST_ASSERT_NOT_NULL(module);
    exportValue = ZrLib_Module_GetExport(state, "captured_native_import", "replay");
    TEST_ASSERT_NOT_NULL(exportValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, exportValue->type);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLib_CallModuleExport(state, "captured_native_import", "replay", ZR_NULL, 0, &result),
                             current_exception_message(state));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(105, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    state->global->sourceLoader = ZR_NULL;
    g_module_fixture_sources = ZR_NULL;
    g_module_fixture_source_count = 0;
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_direct_module_export_succeeds_for_source_module_function);
    RUN_TEST(test_direct_module_export_runtime_error_returns_false_instead_of_aborting);
    RUN_TEST(test_direct_module_export_preserves_scalar_captures_after_tail_called_entry);
    RUN_TEST(test_direct_module_export_preserves_imported_native_module_captures);

    return UNITY_END();
}
