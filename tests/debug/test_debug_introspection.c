#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
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
    TZrBool sawEvaluationContext;
    TZrBool sawCanonicalActiveBinding;
    TZrBool sawInactiveCallerBindingExcluded;
    TZrBool sawReusedFrameGenerationRejected;
    TZrBool sawFreeFrameWithoutReceiver;
    TZrBool sawCanonicalReceiver;
    TZrBool sawRuntimeRoot;
    TZrBool sawMismatchedRuntimeRootRejected;
    TZrBool sawClosureCapture;
    TZrBool sawClosureCaptureTokenRejected;
    TZrBool sawClosureCaptureOutOfRangeRejected;
    SZrDebugEvaluationContext capturedEvaluationContext;
    SZrDebugRuntimeRootBinding capturedRuntimeRoot;
    SZrDebugEvaluationContext capturedClosureContext;
    SZrDebugClosureCaptureBinding capturedClosureCapture;
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

static const SZrFunctionLocalVariable *find_local_variable_by_name(const SZrFunction *function,
                                                                     const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        TZrNativeString localName = local->name != ZR_NULL ? ZrCore_String_GetNativeString(local->name) : ZR_NULL;
        if (localName != ZR_NULL && strcmp(localName, name) == 0) {
            return local;
        }
    }

    return ZR_NULL;
}

static SZrFunctionTypedLocalBinding *find_typed_local_binding_by_slot(SZrFunction *function,
                                                                       TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool evaluation_context_contains_stack_slot(SZrState *state,
                                                       const SZrDebugEvaluationContext *context,
                                                       TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (state == ZR_NULL || context == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < context->activeBindingCount; index++) {
        SZrDebugFrameBinding binding;
        if (ZrCore_Debug_EvaluationContext_GetBinding(state, context, index, &binding) !=
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return ZR_FALSE;
        }
        if (binding.stackSlot == stackSlot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
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
    SZrDebugEvaluationContext evaluationContext;
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
        info.name == ZR_NULL) {
        return;
    }

    if (strcmp(info.name, "target") != 0) {
        return;
    }

    g_debugIntrospectionCapture.sawTargetFrame = ZR_TRUE;
    g_debugIntrospectionCapture.sawNameWhat =
            (TZrBool)(info.nameWhat == ZR_DEBUG_NAMEWHAT_UNKNOWN);

    memset(&evaluationContext, 0, sizeof(evaluationContext));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &evaluationContext) ==
        ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        const SZrFunctionLocalVariable *inputLocal = find_local_variable_by_name(activation.function, "input");
        const SZrFunctionTypedLocalBinding *inputBinding =
                inputLocal != ZR_NULL ? find_typed_local_binding_by_slot(activation.function, inputLocal->stackSlot)
                                      : ZR_NULL;

        if (inputBinding != ZR_NULL &&
            inputBinding->symbolId != 0u &&
            inputBinding->typeId != 0u &&
            inputBinding->placeId != 0u &&
            evaluationContext.frameGeneration != 0u &&
            evaluation_context_contains_stack_slot(state, &evaluationContext, inputBinding->stackSlot)) {
            g_debugIntrospectionCapture.sawCanonicalActiveBinding = ZR_TRUE;
        }
        if (!g_debugIntrospectionCapture.sawEvaluationContext) {
            g_debugIntrospectionCapture.capturedEvaluationContext = evaluationContext;
            g_debugIntrospectionCapture.sawEvaluationContext = ZR_TRUE;
        } else if (evaluationContext.frameGeneration !=
                   g_debugIntrospectionCapture.capturedEvaluationContext.frameGeneration) {
            SZrDebugFrameBinding staleBinding;

            if (ZrCore_Debug_EvaluationContext_GetBinding(
                        state,
                        &g_debugIntrospectionCapture.capturedEvaluationContext,
                        0u,
                        &staleBinding) == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME) {
                g_debugIntrospectionCapture.sawReusedFrameGenerationRejected = ZR_TRUE;
            }
        }
        {
            SZrDebugRuntimeRootBinding runtimeRoot;
            SZrDebugRuntimeRootBinding mismatchedRoot;
            SZrTypeValue runtimeRootValue;

            memset(&runtimeRoot, 0, sizeof(runtimeRoot));
            ZrCore_Value_ResetAsNull(&runtimeRootValue);
            if (ZrCore_Debug_EvaluationContext_GetRuntimeRoot(
                        state,
                        &evaluationContext,
                        ZR_DEBUG_RUNTIME_ROOT_ZR,
                        &runtimeRoot) == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
                runtimeRoot.token != 0u &&
                ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                        state,
                        &evaluationContext,
                        &runtimeRoot,
                        &runtimeRootValue) == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
                runtimeRootValue.type == state->global->zrObject.type &&
                runtimeRootValue.value.object == state->global->zrObject.value.object) {
                g_debugIntrospectionCapture.sawRuntimeRoot = ZR_TRUE;
                g_debugIntrospectionCapture.capturedRuntimeRoot = runtimeRoot;
            }

            mismatchedRoot = runtimeRoot;
            mismatchedRoot.token++;
            ZrCore_Value_ResetAsNull(&runtimeRootValue);
            if (ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                        state,
                        &evaluationContext,
                        &mismatchedRoot,
                        &runtimeRootValue) == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME) {
                g_debugIntrospectionCapture.sawMismatchedRuntimeRootRejected = ZR_TRUE;
            }
        }
        {
            SZrDebugFrameBinding receiverBinding;
            SZrTypeValue receiverValue;
            const SZrFunctionLocalVariable *receiverInputLocal;
            SZrFunctionTypedLocalBinding *receiverInputBinding;

            ZrCore_Value_ResetAsNull(&receiverValue);
            if (ZrCore_Debug_EvaluationContext_GetReceiver(
                        state, &evaluationContext, &receiverBinding, &receiverValue) ==
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_NO_RECEIVER) {
                g_debugIntrospectionCapture.sawFreeFrameWithoutReceiver = ZR_TRUE;
            }

            receiverInputLocal = find_local_variable_by_name(activation.function, "input");
            receiverInputBinding = receiverInputLocal != ZR_NULL
                    ? find_typed_local_binding_by_slot(activation.function, receiverInputLocal->stackSlot)
                    : ZR_NULL;
            if (receiverInputBinding != ZR_NULL) {
                receiverInputBinding->roleFlags |= ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER;
                ZrCore_Value_ResetAsNull(&receiverValue);
                if (ZrCore_Debug_EvaluationContext_GetReceiver(
                            state, &evaluationContext, &receiverBinding, &receiverValue) ==
                            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
                    (receiverBinding.roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u &&
                    receiverBinding.symbolId != 0u && receiverBinding.typeId != 0u &&
                    receiverBinding.placeId != 0u && value_is_int64(&receiverValue, 4)) {
                    g_debugIntrospectionCapture.sawCanonicalReceiver = ZR_TRUE;
                }
            }
        }
    }

    {
        SZrDebugEvaluationContext callerContext;
        SZrDebugActivation callerActivation;
        const SZrFunctionLocalVariable *afterLocal;

        memset(&callerContext, 0, sizeof(callerContext));
        memset(&callerActivation, 0, sizeof(callerActivation));
        if (ZrCore_Debug_GetEvaluationContext(state, 1u, &callerContext) ==
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
            ZrCore_Debug_GetStack(state, 1u, &callerActivation)) {
            afterLocal = find_local_variable_by_name(callerActivation.function, "after");
            if (afterLocal != ZR_NULL &&
                !evaluation_context_contains_stack_slot(state, &callerContext, afterLocal->stackSlot)) {
                g_debugIntrospectionCapture.sawInactiveCallerBindingExcluded = ZR_TRUE;
            }
        }
    }

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

static void debug_closure_capture_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugEvaluationContext context;
    SZrDebugClosureCaptureBinding capture;
    SZrDebugClosureCaptureBinding invalidCapture;
    SZrDebugClosureCaptureBinding mismatchedCapture;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(debugInfo);
    if (state == ZR_NULL || g_debugIntrospectionCapture.sawClosureCapture) {
        return;
    }

    memset(&context, 0, sizeof(context));
    memset(&capture, 0, sizeof(capture));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &context) !=
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        context.activation.function == ZR_NULL || context.activation.function->closureValueLength != 1u) {
        return;
    }

    if (ZrCore_Debug_EvaluationContext_GetClosureCapture(
                state, &context, 0u, &capture) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        capture.type == ZR_NULL || capture.symbolId == 0u || capture.typeId == 0u ||
        capture.token == 0u) {
        return;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
                state, &context, &capture, &value) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK ||
        !value_is_int64(&value, 4)) {
        return;
    }

    mismatchedCapture = capture;
    mismatchedCapture.token++;
    ZrCore_Value_ResetAsNull(&value);
    if (ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
                state, &context, &mismatchedCapture, &value) ==
        ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME) {
        g_debugIntrospectionCapture.sawClosureCaptureTokenRejected = ZR_TRUE;
    }

    memset(&invalidCapture, 0, sizeof(invalidCapture));
    if (ZrCore_Debug_EvaluationContext_GetClosureCapture(
                state, &context, 1u, &invalidCapture) ==
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE &&
        invalidCapture.type == ZR_NULL && invalidCapture.symbolId == 0u &&
        invalidCapture.typeId == 0u && invalidCapture.token == 0u) {
        g_debugIntrospectionCapture.sawClosureCaptureOutOfRangeRejected = ZR_TRUE;
    }

    g_debugIntrospectionCapture.capturedClosureContext = context;
    g_debugIntrospectionCapture.capturedClosureCapture = capture;
    g_debugIntrospectionCapture.sawClosureCapture = ZR_TRUE;
}

static void test_getlocal_and_setlocal_walk_active_locals_by_index(void) {
    const char *source =
            "fn target(input: int): int {\n"
            "    var mutable = input + 1;\n"
            "    var result = mutable + 1;\n"
            "    return result;\n"
            "}\n"
            "fn outer(seed: int): int {\n"
            "    var base = seed;\n"
            "    var first = target(base);\n"
            "    var after = target(first);\n"
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
    TEST_ASSERT_EQUAL_INT64(43, result);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawTargetFrame);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawCallerFrame);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawInputLocal);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawMutableLocal);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.changedMutableLocal);
    TEST_ASSERT_FALSE(g_debugIntrospectionCapture.sawLocalOutOfScope);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawNameWhat);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawEvaluationContext);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawCanonicalActiveBinding);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawInactiveCallerBindingExcluded);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawReusedFrameGenerationRejected);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawFreeFrameWithoutReceiver);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawCanonicalReceiver);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawRuntimeRoot);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawMismatchedRuntimeRootRejected);

    {
        SZrDebugFrameBinding binding;
        SZrTypeValue runtimeRootValue;

        TEST_ASSERT_EQUAL_INT(
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME,
                ZrCore_Debug_EvaluationContext_GetBinding(state,
                                                           &g_debugIntrospectionCapture.capturedEvaluationContext,
                                                           0u,
                                                           &binding));
        ZrCore_Value_ResetAsNull(&runtimeRootValue);
        TEST_ASSERT_EQUAL_INT(
                ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME,
                ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                        state,
                        &g_debugIntrospectionCapture.capturedEvaluationContext,
                        &g_debugIntrospectionCapture.capturedRuntimeRoot,
                        &runtimeRootValue));
    }

    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_evaluation_context_resolves_generation_checked_closure_capture(void) {
    const char *source =
            "fn outer(): int {\n"
            "    var seed: int = 4;\n"
            "    var reader = fn(): int {\n"
            "        return seed + 1;\n"
            "    };\n"
            "    return reader();\n"
            "}\n"
            "return outer();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValue staleValue;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_introspection_source(state, source, "debug_introspection_closure_capture.zr");
    TEST_ASSERT_NOT_NULL(function);

    debug_introspection_capture_reset();
    ZrCore_Debug_SetHook(state, debug_closure_capture_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawClosureCapture);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawClosureCaptureTokenRejected);
    TEST_ASSERT_TRUE(g_debugIntrospectionCapture.sawClosureCaptureOutOfRangeRejected);

    ZrCore_Value_ResetAsNull(&staleValue);
    TEST_ASSERT_EQUAL_INT(
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME,
            ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
                    state,
                    &g_debugIntrospectionCapture.capturedClosureContext,
                    &g_debugIntrospectionCapture.capturedClosureCapture,
                    &staleValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, staleValue.type);

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
    RUN_TEST(test_evaluation_context_resolves_generation_checked_closure_capture);
    RUN_TEST(test_getupvalue_setupvalue_and_upvalue_id_use_closure_cells);
    return UNITY_END();
}
