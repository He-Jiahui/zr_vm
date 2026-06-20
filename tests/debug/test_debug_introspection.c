#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

typedef struct SZrDebugIntrospectionCapture {
    TZrBool sawTargetFrame;
    TZrBool sawCallerFrame;
    TZrBool sawInputLocal;
    TZrBool sawMutableLocal;
    TZrBool changedMutableLocal;
    TZrBool sawLocalOutOfScope;
    TZrBool sawNameWhat;
} SZrDebugIntrospectionCapture;

static SZrDebugIntrospectionCapture g_debugIntrospectionCapture;

static void debug_introspection_capture_reset(void) {
    memset(&g_debugIntrospectionCapture, 0, sizeof(g_debugIntrospectionCapture));
}

static SZrFunction *compile_introspection_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrBool value_is_int64(const SZrTypeValue *value, TZrInt64 expected) {
    return (TZrBool)(value != ZR_NULL &&
                     ZR_VALUE_IS_TYPE_INT(value->type) &&
                     value->value.nativeObject.nativeInt64 == expected);
}

static void inspect_caller_frame(SZrState *state) {
    SZrDebugActivation callerActivation;
    SZrDebugInfo callerInfo;
    TZrInt32 localIndex;

    memset(&callerActivation, 0, sizeof(callerActivation));
    memset(&callerInfo, 0, sizeof(callerInfo));
    if (!ZrCore_Debug_GetStack(state, 1u, &callerActivation) ||
        !ZrCore_Debug_GetInfo(state, &callerActivation, ZR_DEBUG_INFO_FUNCTION_NAME, &callerInfo) ||
        callerInfo.name == ZR_NULL ||
        strcmp(callerInfo.name, "outer") != 0) {
        return;
    }

    for (localIndex = 1; localIndex <= 8; localIndex++) {
        SZrTypeValue value;
        TZrNativeString name;

        ZrCore_Value_ResetAsNull(&value);
        name = ZrCore_Debug_GetLocal(state, &callerActivation, localIndex, &value);
        if (name != ZR_NULL && strcmp(name, "base") == 0 && value_is_int64(&value, 4)) {
            g_debugIntrospectionCapture.sawCallerFrame = ZR_TRUE;
        }
    }
}

static void debug_introspection_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugActivation activation;
    SZrDebugInfo info;
    TZrInt32 localIndex;

    if (state == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
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
        strcmp(info.name, "target") != 0) {
        return;
    }

    g_debugIntrospectionCapture.sawTargetFrame = ZR_TRUE;
    g_debugIntrospectionCapture.sawNameWhat =
            (TZrBool)(info.nameWhat == ZR_DEBUG_NAMEWHAT_UNKNOWN);

    TEST_ASSERT_NULL(ZrCore_Debug_GetLocal(state, &activation, 0, ZR_NULL));
    inspect_caller_frame(state);

    for (localIndex = 1; localIndex <= 8; localIndex++) {
        SZrTypeValue value;
        TZrNativeString name;

        ZrCore_Value_ResetAsNull(&value);
        name = ZrCore_Debug_GetLocal(state, &activation, localIndex, &value);
        if (name == ZR_NULL) {
            continue;
        }

        if (strcmp(name, "input") == 0 && value_is_int64(&value, 4)) {
            g_debugIntrospectionCapture.sawInputLocal = ZR_TRUE;
        } else if (strcmp(name, "mutable") == 0) {
            g_debugIntrospectionCapture.sawMutableLocal = ZR_TRUE;
            if (!g_debugIntrospectionCapture.changedMutableLocal && value_is_int64(&value, 5)) {
                SZrTypeValue replacement;
                TZrNativeString changedName;

                ZrCore_Value_InitAsInt(state, &replacement, 40);
                changedName = ZrCore_Debug_SetLocal(state, &activation, localIndex, &replacement);
                TEST_ASSERT_NOT_NULL(changedName);
                TEST_ASSERT_EQUAL_STRING("mutable", changedName);
                g_debugIntrospectionCapture.changedMutableLocal = ZR_TRUE;
            }
        } else if (strcmp(name, "after") == 0) {
            g_debugIntrospectionCapture.sawLocalOutOfScope = ZR_TRUE;
        }
    }
}

static void test_getlocal_and_setlocal_walk_active_locals_by_index(void) {
    const char *source =
            "func target(input: int): int {\n"
            "    var mutable = input + 1;\n"
            "    var result = mutable + 1;\n"
            "    return result;\n"
            "}\n"
            "func outer(seed: int): int {\n"
            "    var base = seed;\n"
            "    var after = target(base);\n"
            "    return after;\n"
            "}\n"
            "return outer(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_introspection_source(state, source, "debug_introspection_locals.zr");
    TEST_ASSERT_NOT_NULL(function);

    debug_introspection_capture_reset();
    ZrCore_Debug_SetHook(state, debug_introspection_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(41, result);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawTargetFrame);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawCallerFrame);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawInputLocal);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawMutableLocal);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.changedMutableLocal);
    TEST_ASSERT_FALSE(g_debugIntrospectionCapture.sawLocalOutOfScope);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawNameWhat);

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_getupvalue_setupvalue_and_upvalue_id_use_closure_cells(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrClosure *closure;
    SZrClosureValue *captureCell;
    SZrTypeValue value;
    SZrTypeValue replacement;
    TZrNativeString name;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->closureValueLength = 1u;
    function->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionClosureVariable),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->closureValueList);
    memset(function->closureValueList, 0, sizeof(SZrFunctionClosureVariable));
    function->closureValueList[0].name = ZrCore_String_CreateFromNative(state, "captured");
    TEST_ASSERT_NOT_NULL(function->closureValueList[0].name);

    closure = ZrCore_Closure_New(state, 1u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    captureCell = closure->closureValuesExtend[0];
    TEST_ASSERT_NOT_NULL(captureCell);
    ZrCore_Value_InitAsInt(state, ZrCore_ClosureValue_GetValue(captureCell), 7);

    ZrCore_Value_ResetAsNull(&value);
    name = ZrCore_Debug_GetUpvalue(state, closure, 1, &value);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("captured", name);
    TEST_ASSERT_TRUE(value_is_int64(&value, 7));
    TEST_ASSERT_EQUAL_PTR(captureCell, ZrCore_Debug_GetUpvalueId(state, closure, 1));

    ZrCore_Value_InitAsInt(state, &replacement, 19);
    name = ZrCore_Debug_SetUpvalue(state, closure, 1, &replacement);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("captured", name);
    TEST_ASSERT_TRUE(value_is_int64(ZrCore_ClosureValue_GetValue(captureCell), 19));

    TEST_ASSERT_NULL(ZrCore_Debug_GetUpvalue(state, closure, 0, &value));
    TEST_ASSERT_NULL(ZrCore_Debug_SetUpvalue(state, closure, 2, &replacement));
    TEST_ASSERT_NULL(ZrCore_Debug_GetUpvalueId(state, closure, 2));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_getlocal_and_setlocal_walk_active_locals_by_index);
    RUN_TEST(test_getupvalue_setupvalue_and_upvalue_id_use_closure_cells);
    return UNITY_END();
}
