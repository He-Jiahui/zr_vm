#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/compiler.h"

void setUp(void) {}
void tearDown(void) {}

static void close_meta_exception_panic(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

static SZrState *close_meta_exception_new_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(close_meta_exception_panic);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibSystem_Register(state->global);
    }
    return state;
}

static SZrFunction *close_meta_exception_compile(SZrState *state,
                                                 const TZrChar *source,
                                                 const TZrChar *sourceName) {
    SZrString *name = ZrCore_String_Create(
            state, (TZrNativeString)sourceName, strlen(sourceName));
    return name != ZR_NULL
                   ? ZrParser_Source_Compile(state, source, strlen(source), name)
                   : ZR_NULL;
}

static TZrBool close_meta_exception_execute(SZrState *state,
                                            const TZrChar *source,
                                            const TZrChar *sourceName,
                                            TZrInt64 *outResult) {
    SZrFunction *function = close_meta_exception_compile(state, source, sourceName);
    TZrBool executed;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    executed = ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, outResult);
    ZrCore_Function_Free(state, function);
    return executed;
}

static void test_plain_throw_catch_message_control(void) {
    static const TZrChar source[] =
            "try { throw \"original\"; }\n"
            "catch (e) { if (e.message == \"original\") { return 1; } }\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_plain_catch_control.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_repeated_close_meta_keeps_reusable_callinfo_chain_reachable(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "try { using (new CloseProbe()) { throw \"original\"; } }\n"
            "catch (e) { if (e.message == \"original\") { return 1; } }\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = close_meta_exception_compile(state, source, "close_meta_repeated.zr");
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 attempt = 0u; attempt < 3u; ++attempt) {
        TZrInt64 result = 0;
        TZrUInt32 reachableCallInfos = 0u;
        SZrCallInfo *cursor;

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
                state, function, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);
        for (cursor = state->baseCallInfo.next; cursor != ZR_NULL;
             cursor = cursor->next) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(state->callInfoListLength,
                                             reachableCallInfos + 1u);
            ++reachableCallInfos;
        }
        TEST_ASSERT_EQUAL_UINT32(state->callInfoListLength, reachableCallInfos);
        TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
    }
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_script_close_meta_preserves_outer_exception_through_nested_calls(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "try {\n"
            "    using (new CloseProbe()) { throw \"original\"; }\n"
            "} catch (e) {\n"
            "    if (e.message == \"original\") {\n"
            "        if (CloseProbe.calls == 1) { return 1; }\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_preserve_outer_exception.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_script_close_meta_new_exception_replaces_outer_exception(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { throw \"replacement\"; }\n"
            "}\n"
            "try {\n"
            "    using (new CloseProbe()) { throw \"original\"; }\n"
            "} catch (e) {\n"
            "    if (e.message == \"replacement\") { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_replace_outer_exception.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_plain_throw_catch_message_control);
    RUN_TEST(test_repeated_close_meta_keeps_reusable_callinfo_chain_reachable);
    RUN_TEST(test_script_close_meta_preserves_outer_exception_through_nested_calls);
    RUN_TEST(test_script_close_meta_new_exception_replaces_outer_exception);
    return UNITY_END();
}
