#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_debug/module.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser.h"

void setUp(void) {}

void tearDown(void) {}

static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
    }

    return state;
}

static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const TZrChar *value_as_cstring(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

static SZrObject *value_as_object(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static const SZrTypeValue *object_field(SZrState *state, SZrObject *object, const TZrChar *name) {
    TEST_ASSERT_NOT_NULL(object);
    return ZrLib_Object_GetFieldCString(state, object, name);
}

static void assert_string_field(SZrState *state,
                                SZrObject *object,
                                const TZrChar *fieldName,
                                const TZrChar *expected) {
    const SZrTypeValue *field = object_field(state, object, fieldName);
    const TZrChar *actual;

    TEST_ASSERT_NOT_NULL(field);
    actual = value_as_cstring(state, field);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

static void assert_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 expected) {
    const SZrTypeValue *field = object_field(state, object, fieldName);

    ZR_UNUSED_PARAMETER(state);

    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(field->type));
    TEST_ASSERT_EQUAL_INT64(expected, field->value.nativeObject.nativeInt64);
}

static TZrInt64 get_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *field = object_field(state, object, fieldName);

    ZR_UNUSED_PARAMETER(state);

    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(field->type));
    return field->value.nativeObject.nativeInt64;
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

    messageValue = ZrLib_Object_GetFieldCString(state, errorObject, "message");
    return value_as_cstring(state, messageValue);
}

static void destroy_compiled_state(SZrState *state, SZrFunction *function) {
    if (function != ZR_NULL) {
        ZrCore_Function_Free(state, function);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_module_is_not_loaded_until_host_registers_it(void) {
    SZrState *state = create_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(ZrLib_Module_GetLoaded(state, "debug"));
    TEST_ASSERT_NULL(ZrLib_Module_GetExport(state, "debug", "traceback"));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_registered_debug_module_exports_lua_aligned_api_surface(void) {
    static const TZrChar *kExpectedExports[] = {
            "traceback",
            "getinfo",
            "getlocal",
            "setlocal",
            "getupvalue",
            "setupvalue",
            "upvalueid",
            "sethook",
            "gethook",
    };
    SZrState *state = create_test_state();
    TZrSize index;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));

    for (index = 0; index < sizeof(kExpectedExports) / sizeof(kExpectedExports[0]); index++) {
        TEST_ASSERT_NOT_NULL_MESSAGE(ZrLib_Module_GetExport(state, "debug", kExpectedExports[index]),
                                     kExpectedExports[index]);
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_traceback_returns_known_script_call_chain(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "func leaf(value: int): string {\n"
            "    var next = value + 1;\n"
            "    return debug.traceback(\"phase4-marker\");\n"
            "}\n"
            "func middle(seed: int): string {\n"
            "    var text = leaf(seed + 1);\n"
            "    var keep = seed + 2;\n"
            "    if (keep > 0) {\n"
            "        return text;\n"
            "    }\n"
            "    return text;\n"
            "}\n"
            "return middle(2);\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrTypeValue result;
    const TZrChar *tracebackText;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));
    function = compile_source(state, source, "debug_library_traceback.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_Execute(state, function, &result),
                             current_exception_message(state));
    tracebackText = value_as_cstring(state, &result);
    TEST_ASSERT_NOT_NULL(tracebackText);
    TEST_ASSERT_NOT_NULL(strstr(tracebackText, "phase4-marker"));
    TEST_ASSERT_NOT_NULL(strstr(tracebackText, "leaf"));
    TEST_ASSERT_NOT_NULL(strstr(tracebackText, "middle"));
    TEST_ASSERT_NOT_NULL(strstr(tracebackText, "debug_library_traceback.zr"));

    destroy_compiled_state(state, function);
}

static void test_getinfo_reports_name_source_line_and_parameter_count(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "func inspect(value: int) {\n"
            "    var info = debug.getinfo(1, \"nSlu\");\n"
            "    return info;\n"
            "}\n"
            "return inspect(7);\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrTypeValue result;
    SZrObject *infoObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));
    function = compile_source(state, source, "debug_library_getinfo.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_Execute(state, function, &result),
                             current_exception_message(state));
    infoObject = value_as_object(state, &result);
    TEST_ASSERT_NOT_NULL(infoObject);
    assert_string_field(state, infoObject, "name", "inspect");
    assert_string_field(state, infoObject, "source", "debug_library_getinfo.zr");
    assert_int_field(state, infoObject, "nparams", 1);
    TEST_ASSERT_TRUE(get_int_field(state, infoObject, "currentline") > 0);
    TEST_ASSERT_TRUE(get_int_field(state, infoObject, "linedefined") > 0);
    TEST_ASSERT_TRUE(get_int_field(state, infoObject, "lastlinedefined") >=
                     get_int_field(state, infoObject, "linedefined"));

    destroy_compiled_state(state, function);
}

static void test_getlocal_and_setlocal_read_and_change_active_script_locals(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "func mutate(input: int): int {\n"
            "    var target = input + 1;\n"
            "    var before = debug.getlocal(1, 2);\n"
            "    if (before.value != 5) {\n"
            "        return -10;\n"
            "    }\n"
            "    debug.setlocal(1, 2, 40);\n"
            "    return target;\n"
            "}\n"
            "return mutate(4);\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));
    function = compile_source(state, source, "debug_library_locals.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
                             current_exception_message(state));
    TEST_ASSERT_EQUAL_INT64(40, result);

    destroy_compiled_state(state, function);
}

static void test_upvalue_helpers_read_write_and_identify_closure_cells(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "func makeRunner(seed: int) {\n"
            "    var captured = seed;\n"
            "    return () => {\n"
            "        return captured;\n"
            "    };\n"
            "}\n"
            "var runner = makeRunner(7);\n"
            "var before = debug.getupvalue(runner, 1);\n"
            "var firstId = debug.upvalueid(runner, 1);\n"
            "debug.setupvalue(runner, 1, 19);\n"
            "var secondId = debug.upvalueid(runner, 1);\n"
            "if (before.value == 7 && runner() == 19 && firstId != null && secondId != null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));
    function = compile_source(state, source, "debug_library_upvalues.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
                             current_exception_message(state));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_compiled_state(state, function);
}

static void test_sethook_invokes_script_hook_and_gethook_reports_state(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "var events = 0;\n"
            "var hook = (event, line) => {\n"
            "    events = events + 1;\n"
            "    return 0;\n"
            "};\n"
            "func work(): int {\n"
            "    var first = 1 + 2;\n"
            "    var second = first + 3;\n"
            "    return second;\n"
            "}\n"
            "debug.sethook(hook, \"l\", 3);\n"
            "var info = debug.gethook();\n"
            "var second = work();\n"
            "debug.sethook(null, \"\", 0);\n"
            "return {\n"
            "    events: events,\n"
            "    mask: info.mask,\n"
            "    count: info.count,\n"
            "    hook: info.hook\n"
            "};\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrTypeValue result;
    SZrObject *resultObject;
    const SZrTypeValue *hookValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_Register(state->global));
    function = compile_source(state, source, "debug_library_hooks.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_Execute(state, function, &result),
                             current_exception_message(state));
    resultObject = value_as_object(state, &result);
    TEST_ASSERT_NOT_NULL(resultObject);
    TEST_ASSERT_TRUE(get_int_field(state, resultObject, "events") > 0);
    assert_string_field(state, resultObject, "mask", "l");
    assert_int_field(state, resultObject, "count", 3);
    hookValue = object_field(state, resultObject, "hook");
    TEST_ASSERT_NOT_NULL(hookValue);
    TEST_ASSERT_TRUE(hookValue->type == ZR_VALUE_TYPE_FUNCTION || hookValue->type == ZR_VALUE_TYPE_CLOSURE);

    destroy_compiled_state(state, function);
}

static void test_sandboxed_debug_module_rejects_write_apis(void) {
    const TZrChar *source =
            "var debug = %import(\"debug\");\n"
            "func mutate(input: int): int {\n"
            "    var target = input + 1;\n"
            "    debug.setlocal(1, 2, 40);\n"
            "    return target;\n"
            "}\n"
            "return mutate(4);\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrTypeValue result;
    const TZrChar *message;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibDebug_RegisterSandboxed(state->global));
    function = compile_source(state, source, "debug_library_sandbox.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "debug write API is disabled"));

    destroy_compiled_state(state, function);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_debug_module_is_not_loaded_until_host_registers_it);
    RUN_TEST(test_registered_debug_module_exports_lua_aligned_api_surface);
    RUN_TEST(test_traceback_returns_known_script_call_chain);
    RUN_TEST(test_getinfo_reports_name_source_line_and_parameter_count);
    RUN_TEST(test_getlocal_and_setlocal_read_and_change_active_script_locals);
    RUN_TEST(test_upvalue_helpers_read_write_and_identify_closure_cells);
    RUN_TEST(test_sethook_invokes_script_hook_and_gethook_reports_state);
    RUN_TEST(test_sandboxed_debug_module_rejects_write_apis);

    return UNITY_END();
}
