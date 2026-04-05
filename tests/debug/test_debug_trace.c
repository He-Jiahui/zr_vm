#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

typedef struct SZrDebugTraceCapture {
    TZrUInt32 lineCount;
    TZrUInt32 lines[16];
    TZrUInt32 callCount;
    TZrUInt32 returnCount;
    TZrBool sawNamedFunctionInfo;
    TZrUInt32 infoLine;
    TZrUInt32 definedLineStart;
    TZrUInt32 definedLineEnd;
    TZrUInt32 parametersCount;
    char functionName[64];
    char sourceFile[256];
} SZrDebugTraceCapture;

static SZrDebugTraceCapture g_debugTraceCapture;

static void debug_trace_capture_reset(void) {
    memset(&g_debugTraceCapture, 0, sizeof(g_debugTraceCapture));
}

static void debug_trace_capture(struct SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugInfo resolvedInfo;

    if (debugInfo == ZR_NULL) {
        return;
    }

    if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_CALL) {
        g_debugTraceCapture.callCount++;
        return;
    }

    if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_RETURN) {
        g_debugTraceCapture.returnCount++;
        return;
    }

    if (debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }

    if (g_debugTraceCapture.lineCount < (TZrUInt32)(sizeof(g_debugTraceCapture.lines) / sizeof(g_debugTraceCapture.lines[0]))) {
        g_debugTraceCapture.lines[g_debugTraceCapture.lineCount] = (TZrUInt32)debugInfo->currentLine;
    }
    g_debugTraceCapture.lineCount++;

    memset(&resolvedInfo, 0, sizeof(resolvedInfo));
    if (!ZrCore_DebugInfo_Get(state,
                              (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                 ZR_DEBUG_INFO_SOURCE_FILE |
                                                 ZR_DEBUG_INFO_LINE_NUMBER),
                              &resolvedInfo)) {
        return;
    }

    if (resolvedInfo.name != ZR_NULL && strcmp(resolvedInfo.name, "addOne") == 0) {
        g_debugTraceCapture.sawNamedFunctionInfo = ZR_TRUE;
        g_debugTraceCapture.infoLine = (TZrUInt32)resolvedInfo.currentLine;
        g_debugTraceCapture.definedLineStart = (TZrUInt32)resolvedInfo.definedLineStart;
        g_debugTraceCapture.definedLineEnd = (TZrUInt32)resolvedInfo.definedLineEnd;
        g_debugTraceCapture.parametersCount = (TZrUInt32)resolvedInfo.parametersCount;
        snprintf(g_debugTraceCapture.functionName,
                 sizeof(g_debugTraceCapture.functionName),
                 "%s",
                 resolvedInfo.name != ZR_NULL ? resolvedInfo.name : "");
        snprintf(g_debugTraceCapture.sourceFile,
                 sizeof(g_debugTraceCapture.sourceFile),
                 "%s",
                 resolvedInfo.source != ZR_NULL ? resolvedInfo.source : "");
    }
}

static SZrFunction *compile_debug_trace_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "func addOne(value: int): int {\n"
            "    var base = value + 1;\n"
            "    return base;\n"
            "}\n"
            "var first = addOne(4);\n"
            "var second = first + 2;\n"
            "return second;";
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_interp_debug_trace_publishes_line_events_and_resolves_debug_info(void) {
    const char *sourcePath = "debug_trace_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_debug_trace_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    debug_trace_capture_reset();
    state->debugHook = debug_trace_capture;
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_CALL | ZR_DEBUG_HOOK_MASK_RETURN | ZR_DEBUG_HOOK_MASK_LINE;

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    TEST_ASSERT_TRUE(g_debugTraceCapture.callCount > 0);
    TEST_ASSERT_TRUE(g_debugTraceCapture.returnCount > 0);
    TEST_ASSERT_TRUE(g_debugTraceCapture.lineCount >= 3);
    TEST_ASSERT_TRUE(g_debugTraceCapture.sawNamedFunctionInfo);
    TEST_ASSERT_EQUAL_STRING("addOne", g_debugTraceCapture.functionName);
    TEST_ASSERT_EQUAL_STRING(sourcePath, g_debugTraceCapture.sourceFile);
    TEST_ASSERT_TRUE(g_debugTraceCapture.infoLine >= 2);
    TEST_ASSERT_TRUE(g_debugTraceCapture.definedLineStart >= 1);
    TEST_ASSERT_TRUE(g_debugTraceCapture.definedLineEnd >= g_debugTraceCapture.definedLineStart);
    TEST_ASSERT_EQUAL_UINT32(1u, g_debugTraceCapture.parametersCount);

    state->debugHook = ZR_NULL;
    state->debugHookSignal = 0;
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_interp_debug_trace_publishes_line_events_and_resolves_debug_info);
    return UNITY_END();
}
