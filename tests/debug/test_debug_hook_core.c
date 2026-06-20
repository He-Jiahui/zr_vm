#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

typedef struct SZrCoreHookCapture {
    TZrUInt32 countEvents;
    TZrUInt32 lineEvents;
    TZrUInt32 duplicateLineEvents;
    TZrUInt32 callEvents;
    TZrUInt32 returnEvents;
    TZrUInt32 lastLine;
    TZrBool sawInnerFrame;
    TZrBool sawOuterFrame;
    TZrBool lineOnlyRespectedTypeMask;
    char innerName[64];
    char outerName[64];
    char sourceName[128];
} SZrCoreHookCapture;

static SZrCoreHookCapture g_hookCapture;

static void hook_capture_reset(void) {
    memset(&g_hookCapture, 0, sizeof(g_hookCapture));
}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void count_capture_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    ZR_UNUSED_PARAMETER(state);

    if (debugInfo == ZR_NULL) {
        return;
    }

    if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_COUNT) {
        g_hookCapture.countEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_LINE) {
        if (debugInfo->currentLine != 0 && debugInfo->currentLine == g_hookCapture.lastLine) {
            g_hookCapture.duplicateLineEvents++;
        }
        g_hookCapture.lastLine = (TZrUInt32)debugInfo->currentLine;
        g_hookCapture.lineEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_CALL) {
        g_hookCapture.callEvents++;
    } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_RETURN) {
        g_hookCapture.returnEvents++;
    }
}

static void stack_capture_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugActivation innerActivation;
    SZrDebugActivation outerActivation;
    SZrDebugInfo innerInfo;
    SZrDebugInfo outerInfo;
    SZrDebugInfo lineOnlyInfo;

    if (state == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }

    memset(&innerActivation, 0, sizeof(innerActivation));
    memset(&outerActivation, 0, sizeof(outerActivation));
    if (!ZrCore_Debug_GetStack(state, 0, &innerActivation) ||
        !ZrCore_Debug_GetStack(state, 1, &outerActivation)) {
        return;
    }

    memset(&innerInfo, 0, sizeof(innerInfo));
    if (ZrCore_Debug_GetInfo(state,
                             &innerActivation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER |
                                                ZR_DEBUG_INFO_CLOSURE),
                             &innerInfo) &&
        innerInfo.name != ZR_NULL &&
        strcmp(innerInfo.name, "inner") == 0) {
        g_hookCapture.sawInnerFrame = ZR_TRUE;
        snprintf(g_hookCapture.innerName, sizeof(g_hookCapture.innerName), "%s", innerInfo.name);
        snprintf(g_hookCapture.sourceName,
                 sizeof(g_hookCapture.sourceName),
                 "%s",
                 innerInfo.source != ZR_NULL ? innerInfo.source : "");
        TEST_ASSERT_TRUE(innerInfo.currentLine > 0);
        TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)innerInfo.parametersCount);

        memset(&outerInfo, 0, sizeof(outerInfo));
        if (ZrCore_Debug_GetInfo(state,
                                 &outerActivation,
                                 (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                    ZR_DEBUG_INFO_LINE_NUMBER),
                                 &outerInfo) &&
            outerInfo.name != ZR_NULL &&
            strcmp(outerInfo.name, "outer") == 0 &&
            outerInfo.currentLine > 0) {
            g_hookCapture.sawOuterFrame = ZR_TRUE;
            snprintf(g_hookCapture.outerName, sizeof(g_hookCapture.outerName), "%s", outerInfo.name);
        }

        memset(&lineOnlyInfo, 0, sizeof(lineOnlyInfo));
        if (ZrCore_Debug_GetInfo(state,
                                 &innerActivation,
                                 ZR_DEBUG_INFO_LINE_NUMBER,
                                 &lineOnlyInfo) &&
            lineOnlyInfo.currentLine > 0 &&
            lineOnlyInfo.name == ZR_NULL &&
            lineOnlyInfo.source == ZR_NULL &&
            lineOnlyInfo.parametersCount == 0) {
            g_hookCapture.lineOnlyRespectedTypeMask = ZR_TRUE;
        }
    }
}

static void test_count_hook_fires_for_each_instruction_period_and_can_be_disabled(void) {
    const char *source =
            "var value = 1;\n"
            "value = value + 2;\n"
            "value = value + 3;\n"
            "return value;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_count.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, count_capture_hook, ZR_DEBUG_HOOK_MASK_COUNT, 1u);
    TEST_ASSERT_EQUAL_PTR(count_capture_hook, ZrCore_Debug_GetHook(state));
    TEST_ASSERT_EQUAL_UINT32(ZR_DEBUG_HOOK_MASK_COUNT, ZrCore_Debug_GetHookMask(state));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Debug_GetHookCount(state));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)function->instructionsLength, g_hookCapture.countEvents);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    TEST_ASSERT_NULL(ZrCore_Debug_GetHook(state));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookMask(state));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookCount(state));
    TEST_ASSERT_EQUAL_UINT32(0u, state->debugHookSignal);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);
    TEST_ASSERT_EQUAL_UINT32(0u, g_hookCapture.countEvents);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_line_and_count_hooks_are_independent_and_line_events_are_deduplicated(void) {
    const char *source =
            "var first = 1 + 2 + 3;\n"
            "var second = first + 4;\n"
            "return second;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_line_count.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state,
                         count_capture_hook,
                         (TZrUInt32)(ZR_DEBUG_HOOK_MASK_LINE | ZR_DEBUG_HOOK_MASK_COUNT),
                         1u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);
    TEST_ASSERT_TRUE(g_hookCapture.countEvents > 0);
    TEST_ASSERT_TRUE(g_hookCapture.lineEvents >= 2u);
    TEST_ASSERT_EQUAL_UINT32(0u, g_hookCapture.duplicateLineEvents);

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_getstack_and_getinfo_resolve_nested_frames_and_respect_type_mask(void) {
    const char *source =
            "func inner(value: int): int {\n"
            "    var local = value + 1;\n"
            "    return local;\n"
            "}\n"
            "func outer(value: int): int {\n"
            "    var result = inner(value);\n"
            "    return result;\n"
            "}\n"
            "return outer(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "debug_hook_stack.zr");
    TEST_ASSERT_NOT_NULL(function);

    hook_capture_reset();
    ZrCore_Debug_SetHook(state, stack_capture_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    TEST_ASSERT_TRUE(g_hookCapture.sawInnerFrame);
    TEST_ASSERT_TRUE(g_hookCapture.sawOuterFrame);
    TEST_ASSERT_TRUE(g_hookCapture.lineOnlyRespectedTypeMask);
    TEST_ASSERT_EQUAL_STRING("inner", g_hookCapture.innerName);
    TEST_ASSERT_EQUAL_STRING("outer", g_hookCapture.outerName);
    TEST_ASSERT_EQUAL_STRING("debug_hook_stack.zr", g_hookCapture.sourceName);

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_count_hook_fires_for_each_instruction_period_and_can_be_disabled);
    RUN_TEST(test_line_and_count_hooks_are_independent_and_line_events_are_deduplicated);
    RUN_TEST(test_getstack_and_getinfo_resolve_nested_frames_and_respect_type_mask);
    return UNITY_END();
}
