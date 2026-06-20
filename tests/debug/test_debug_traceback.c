#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

typedef struct SZrDebugTracebackCapture {
    TZrBool captured;
    TZrSize written;
    TZrSize smallWritten;
    char buffer[1024];
    char smallBuffer[16];
} SZrDebugTracebackCapture;

static SZrDebugTracebackCapture g_tracebackCapture;

typedef struct SZrDebugTracebackFoldCapture {
    TZrBool captured;
    TZrSize written;
    char buffer[1024];
} SZrDebugTracebackFoldCapture;

static SZrDebugTracebackFoldCapture g_foldCapture;

static void debug_traceback_capture_reset(void) {
    memset(&g_tracebackCapture, 0, sizeof(g_tracebackCapture));
    memset(g_tracebackCapture.smallBuffer, 'x', sizeof(g_tracebackCapture.smallBuffer));
}

static void debug_traceback_fold_capture_reset(void) {
    memset(&g_foldCapture, 0, sizeof(g_foldCapture));
}

static SZrFunction *compile_traceback_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const TZrChar *debug_traceback_get_string_field(SZrState *state,
                                                       const SZrTypeValue *objectValue,
                                                       const char *fieldName) {
    SZrObject *object;
    SZrString *fieldString;
    SZrTypeValue key;
    const SZrTypeValue *fieldValue;

    if (state == ZR_NULL || objectValue == ZR_NULL || objectValue->value.object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, objectValue->value.object);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    fieldValue = ZrCore_Object_GetValue(state, object, &key);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_STRING || fieldValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, fieldValue->value.object));
}

static TZrInt64 debug_traceback_native_noop(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 0;
}

static SZrFunction *debug_traceback_new_metadata_function(SZrState *state,
                                                          const char *functionName,
                                                          const char *sourceName) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(functionName);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->functionName = ZrCore_String_CreateFromNative(state, (TZrNativeString)functionName);
    TEST_ASSERT_NOT_NULL(function->functionName);
    if (sourceName != ZR_NULL) {
        function->sourceCodeList = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourceName);
        TEST_ASSERT_NOT_NULL(function->sourceCodeList);
    }
    function->lineInSourceStart = 1u;
    function->lineInSourceEnd = 1u;
    return function;
}

static TZrStackValuePointer debug_traceback_push_native_callable(SZrState *state, SZrFunction *metadataFunction) {
    SZrClosureNative *closure;
    TZrStackValuePointer slot;
    SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(metadataFunction);

    ZrCore_Function_CheckStackAndGc(state, 1, state->stackTop.valuePointer);
    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = debug_traceback_native_noop;
    closure->aotShimFunction = metadataFunction;

    slot = state->stackTop.valuePointer++;
    value = ZrCore_Stack_GetValue(slot);
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isNative = ZR_TRUE;
    value->isGarbageCollectable = ZR_TRUE;
    return slot;
}

static TZrStackValuePointer debug_traceback_push_script_callable(SZrState *state, SZrFunction *function) {
    SZrClosure *closure;
    TZrStackValuePointer slot;
    SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_CheckStackAndGc(state, 1, state->stackTop.valuePointer);
    closure = ZrCore_Closure_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    slot = state->stackTop.valuePointer++;
    value = ZrCore_Stack_GetValue(slot);
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isNative = ZR_FALSE;
    value->isGarbageCollectable = ZR_TRUE;
    return slot;
}

static void debug_traceback_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugActivation activation;
    SZrDebugInfo info;

    if (state == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE ||
        g_tracebackCapture.captured) {
        return;
    }

    memset(&activation, 0, sizeof(activation));
    memset(&info, 0, sizeof(info));
    if (!ZrCore_Debug_GetStack(state, 0u, &activation) ||
        !ZrCore_Debug_GetInfo(state,
                              &activation,
                              (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME | ZR_DEBUG_INFO_LINE_NUMBER),
                              &info) ||
        info.name == ZR_NULL ||
        strcmp(info.name, "leaf") != 0) {
        return;
    }

    g_tracebackCapture.written = ZrCore_Debug_Traceback(state,
                                                        "traceback:",
                                                        0u,
                                                        0u,
                                                        g_tracebackCapture.buffer,
                                                        sizeof(g_tracebackCapture.buffer));
    g_tracebackCapture.smallWritten = ZrCore_Debug_Traceback(state,
                                                             "traceback:",
                                                             0u,
                                                             0u,
                                                             g_tracebackCapture.smallBuffer,
                                                             sizeof(g_tracebackCapture.smallBuffer));
    g_tracebackCapture.captured = ZR_TRUE;
}

static void debug_traceback_fold_hook(struct SZrState *state, SZrDebugInfo *debugInfo) {
    if (state == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE ||
        g_foldCapture.captured || debugInfo->currentLine != 3u) {
        return;
    }

    g_foldCapture.written = ZrCore_Debug_Traceback(state,
                                                   "traceback:",
                                                   0u,
                                                   7u,
                                                   g_foldCapture.buffer,
                                                   sizeof(g_foldCapture.buffer));
    g_foldCapture.captured = ZR_TRUE;
}

static void test_traceback_formats_active_script_frames_and_truncates_safely(void) {
    const char *source =
            "func leaf(value: int): int {\n"
            "    var local = value + 1;\n"
            "    return local;\n"
            "}\n"
            "func middle(seed: int): int {\n"
            "    var next = seed + 1;\n"
            "    var answer = leaf(next);\n"
            "    return answer + 1;\n"
            "}\n"
            "func root(): int {\n"
            "    var answer = middle(3);\n"
            "    return answer - 1;\n"
            "}\n"
            "return root();";
    const char *sourcePath = "debug_traceback_nested.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_traceback_source(state, source, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    debug_traceback_capture_reset();
    ZrCore_Debug_SetHook(state, debug_traceback_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    TEST_ASSERT_TRUE(g_tracebackCapture.captured);
    TEST_ASSERT_TRUE(g_tracebackCapture.written > 0u);
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, "traceback:"));
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, "  at "));
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, "leaf"));
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, "middle"));
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, "root"));
    TEST_ASSERT_NOT_NULL(strstr(g_tracebackCapture.buffer, sourcePath));
    TEST_ASSERT_EQUAL_CHAR('\0', g_tracebackCapture.smallBuffer[sizeof(g_tracebackCapture.smallBuffer) - 1u]);
    TEST_ASSERT_TRUE(g_tracebackCapture.smallWritten < sizeof(g_tracebackCapture.smallBuffer));

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_traceback_formats_mixed_native_and_script_frames(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *nativeOuterFunction;
    SZrFunction *scriptFunction;
    SZrFunction *nativeInnerFunction;
    TZrStackValuePointer nativeOuterSlot;
    TZrStackValuePointer scriptSlot;
    TZrStackValuePointer nativeInnerSlot;
    SZrCallInfo nativeOuterCall;
    SZrCallInfo scriptCall;
    SZrCallInfo nativeInnerCall;
    SZrCallInfo *previousCallInfo;
    char buffer[1024];

    TEST_ASSERT_NOT_NULL(state);
    nativeOuterFunction = debug_traceback_new_metadata_function(state, "native_outer", "native_outer.c");
    scriptFunction = debug_traceback_new_metadata_function(state, "script_middle", "debug_traceback_mixed.zr");
    nativeInnerFunction = debug_traceback_new_metadata_function(state, "native_inner", "native_inner.c");
    nativeOuterSlot = debug_traceback_push_native_callable(state, nativeOuterFunction);
    scriptSlot = debug_traceback_push_script_callable(state, scriptFunction);
    nativeInnerSlot = debug_traceback_push_native_callable(state, nativeInnerFunction);

    memset(&nativeOuterCall, 0, sizeof(nativeOuterCall));
    memset(&scriptCall, 0, sizeof(scriptCall));
    memset(&nativeInnerCall, 0, sizeof(nativeInnerCall));
    nativeOuterCall.functionBase.valuePointer = nativeOuterSlot;
    nativeOuterCall.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    scriptCall.functionBase.valuePointer = scriptSlot;
    scriptCall.previous = &nativeOuterCall;
    nativeInnerCall.functionBase.valuePointer = nativeInnerSlot;
    nativeInnerCall.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    nativeInnerCall.previous = &scriptCall;

    previousCallInfo = state->callInfoList;
    state->callInfoList = &nativeInnerCall;
    TEST_ASSERT_TRUE(ZrCore_Debug_Traceback(state, "traceback:", 0u, 0u, buffer, sizeof(buffer)) > 0u);
    state->callInfoList = previousCallInfo;

    TEST_ASSERT_NOT_NULL(strstr(buffer, "traceback:"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "native_inner [native]"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "script_middle (debug_traceback_mixed.zr"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "native_outer [native]"));

    ZrCore_Function_Free(state, nativeOuterFunction);
    ZrCore_Function_Free(state, scriptFunction);
    ZrCore_Function_Free(state, nativeInnerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_traceback_folds_deep_stacks_with_skip_marker(void) {
    const char *source =
            "func recur(depth: int): int {\n"
            "    if (depth == 0) {\n"
            "        var here = depth + 1;\n"
            "        return here;\n"
            "    }\n"
            "    var next = recur(depth - 1);\n"
            "    return next + 1;\n"
            "}\n"
            "return recur(24);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_traceback_source(state, source, "debug_traceback_deep.zr");
    TEST_ASSERT_NOT_NULL(function);

    debug_traceback_fold_capture_reset();
    ZrCore_Debug_SetHook(state, debug_traceback_fold_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(25, result);
    TEST_ASSERT_TRUE(g_foldCapture.captured);
    TEST_ASSERT_TRUE(g_foldCapture.written > 0u);
    TEST_ASSERT_NOT_NULL(strstr(g_foldCapture.buffer, "traceback:"));
    TEST_ASSERT_NOT_NULL(strstr(g_foldCapture.buffer, "recur"));
    TEST_ASSERT_NOT_NULL(strstr(g_foldCapture.buffer, "skipping"));

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_throw_normalizes_exception_with_text_traceback(void) {
    const char *source =
            "func leaf(): int {\n"
            "    throw \"boom\";\n"
            "    return 0;\n"
            "}\n"
            "func middle(): int {\n"
            "    var value = leaf();\n"
            "    return value;\n"
            "}\n"
            "func root(): int {\n"
            "    var value = middle();\n"
            "    return value;\n"
            "}\n"
            "return root();";
    const TZrChar *stackText;
    FILE *unhandledStream;
    char unhandledText[1024];
    size_t unhandledLength;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_traceback_source(state, source, "debug_traceback_throw.zr");
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
    TEST_ASSERT_TRUE(state->hasCurrentException);
    stackText = debug_traceback_get_string_field(state, &state->currentException, "stack");
    TEST_ASSERT_NOT_NULL(stackText);
    TEST_ASSERT_NOT_NULL(strstr(stackText, "  at "));
    TEST_ASSERT_NOT_NULL(strstr(stackText, "leaf"));
    TEST_ASSERT_NOT_NULL(strstr(stackText, "middle"));
    TEST_ASSERT_NOT_NULL(strstr(stackText, "root"));
    TEST_ASSERT_NOT_NULL(strstr(stackText, "debug_traceback_throw.zr"));

    unhandledStream = tmpfile();
    TEST_ASSERT_NOT_NULL(unhandledStream);
    ZrCore_Exception_PrintUnhandled(state, &state->currentException, unhandledStream);
    TEST_ASSERT_EQUAL_INT(0, fseek(unhandledStream, 0, SEEK_SET));
    memset(unhandledText, 0, sizeof(unhandledText));
    unhandledLength = fread(unhandledText, 1u, sizeof(unhandledText) - 1u, unhandledStream);
    TEST_ASSERT_TRUE(unhandledLength > 0u);
    TEST_ASSERT_NOT_NULL(strstr(unhandledText, "  at "));
    TEST_ASSERT_NOT_NULL(strstr(unhandledText, "leaf"));
    TEST_ASSERT_NOT_NULL(strstr(unhandledText, "debug_traceback_throw.zr"));
    TEST_ASSERT_NULL(strstr(unhandledText, "ip="));
    fclose(unhandledStream);

    ZrCore_State_ResetThread(state, state->currentExceptionStatus);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_traceback_formats_active_script_frames_and_truncates_safely);
    RUN_TEST(test_traceback_formats_mixed_native_and_script_frames);
    RUN_TEST(test_traceback_folds_deep_stacks_with_skip_marker);
    RUN_TEST(test_throw_normalizes_exception_with_text_traceback);
    return UNITY_END();
}
