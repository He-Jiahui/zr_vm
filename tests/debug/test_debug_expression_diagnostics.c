#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "debug_internal.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

static void assert_text_contains(const TZrChar *text, const TZrChar *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), text);
}

static void assert_text_not_contains(const TZrChar *text, const TZrChar *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL_MESSAGE(strstr(text, needle), text);
}

static SZrFunction *compile_debug_source(SZrState *state, const char *sourceLabel, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

typedef struct SZrDebugCanonicalBindingCapture {
    ZrDebugAgent *agent;
    TZrBool sawPausedBinding;
    TZrBool sawReferenceFact;
    TZrUInt32 expectedSymbolId;
    TZrUInt32 expectedTypeId;
    TZrUInt32 expectedStartLine;
    TZrUInt32 expectedStartColumn;
    TZrUInt32 actualSymbolId;
    TZrUInt32 actualTypeId;
    TZrUInt32 actualStartLine;
    TZrUInt32 actualStartColumn;
} SZrDebugCanonicalBindingCapture;

static SZrDebugCanonicalBindingCapture g_debugCanonicalBindingCapture;

static const TZrChar *debug_canonical_binding_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(value)
                   : ZrCore_String_GetNativeString(value);
}

static const SZrFunctionTypedLocalBinding *debug_canonical_binding_find_typed_local(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static void debug_canonical_binding_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    SZrDebugEvaluationContext context;
    SZrDebugFrameBinding frameBinding;
    const SZrFunctionTypedLocalBinding *typedBinding = ZR_NULL;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    SZrAstNode *expression = ZR_NULL;
    const SZrSemanticReferenceFact *reference;
    TZrUInt32 index;

    ZR_UNUSED_PARAMETER(debugInfo);
    memset(&frameBinding, 0, sizeof(frameBinding));
    if (state == ZR_NULL || g_debugCanonicalBindingCapture.agent == ZR_NULL ||
        g_debugCanonicalBindingCapture.sawReferenceFact) {
        return;
    }

    memset(&context, 0, sizeof(context));
    if (ZrCore_Debug_GetEvaluationContext(state, 0u, &context) != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return;
    }

    for (index = 0u; index < context.activeBindingCount; index++) {
        memset(&frameBinding, 0, sizeof(frameBinding));
        if (ZrCore_Debug_EvaluationContext_GetBinding(state, &context, index, &frameBinding) !=
            ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return;
        }
        typedBinding = debug_canonical_binding_find_typed_local(context.activation.function, frameBinding.stackSlot);
        if (typedBinding != ZR_NULL &&
            strcmp(debug_canonical_binding_string_text(typedBinding->name), "paused") == 0) {
            break;
        }
        typedBinding = ZR_NULL;
    }

    if (typedBinding == ZR_NULL) {
        return;
    }

    g_debugCanonicalBindingCapture.sawPausedBinding = ZR_TRUE;
    g_debugCanonicalBindingCapture.expectedSymbolId = frameBinding.symbolId;
    g_debugCanonicalBindingCapture.expectedTypeId = frameBinding.typeId;
    g_debugCanonicalBindingCapture.expectedStartLine = frameBinding.declarationStartLine;
    g_debugCanonicalBindingCapture.expectedStartColumn = frameBinding.declarationStartColumn;

    sourceName = ZrCore_String_CreateFromNative(state, "<debug:e2b1-binding>");
    if (sourceName == ZR_NULL) {
        return;
    }
    ZrParser_State_Init(&parserState, state, "paused", strlen("paused"), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    expression = ZrParser_ParseExpressionWithState(&parserState);
    if (parserState.hasError || expression == ZR_NULL) {
        ZrParser_State_Free(&parserState);
        return;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerState.currentAst = expression;
    compilerState.scriptAst = expression;
    compilerState.suppressErrorOutput = ZR_TRUE;
    zr_debug_semantic_register_bindings(g_debugCanonicalBindingCapture.agent, 1u, &compilerState);
    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(&compilerState, expression, &inferredType)) {
        reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                compilerState.semanticContext,
                expression,
                ZR_SEMANTIC_REFERENCE_READ);
        if (reference != ZR_NULL) {
            g_debugCanonicalBindingCapture.sawReferenceFact = ZR_TRUE;
            g_debugCanonicalBindingCapture.actualSymbolId = reference->symbolId;
            g_debugCanonicalBindingCapture.actualTypeId = reference->typeId;
            g_debugCanonicalBindingCapture.actualStartLine = (TZrUInt32)reference->declarationRange.start.line;
            g_debugCanonicalBindingCapture.actualStartColumn = (TZrUInt32)reference->declarationRange.start.column;
        }
    }
    ZrParser_InferredType_Free(state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_debug_semantic_binding_preserves_paused_frame_canonical_identity(void) {
    const char *source =
            "func target(paused: int): int {\n"
            "    var observed = paused + 1;\n"
            "    return observed;\n"
            "}\n"
            "return target(4);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_e2b1_paused_binding.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    memset(&g_debugCanonicalBindingCapture, 0, sizeof(g_debugCanonicalBindingCapture));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    g_debugCanonicalBindingCapture.agent = &agent;

    ZrCore_Debug_SetHook(state, debug_canonical_binding_hook, ZR_DEBUG_HOOK_MASK_LINE, 0u);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
    ZrCore_Debug_SetHook(state, ZR_NULL, 0u, 0u);

    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawPausedBinding);
    TEST_ASSERT_TRUE(g_debugCanonicalBindingCapture.sawReferenceFact);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedSymbolId,
            g_debugCanonicalBindingCapture.actualSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedTypeId,
            g_debugCanonicalBindingCapture.actualTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedStartLine,
            g_debugCanonicalBindingCapture.actualStartLine);
    TEST_ASSERT_EQUAL_UINT32(
            g_debugCanonicalBindingCapture.expectedStartColumn,
            g_debugCanonicalBindingCapture.actualStartColumn);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 +", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '+'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "right-hand expression");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_expression_reports_missing_logical_operand_with_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true &&", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '&&'");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_short_circuits_or_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "true || missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_short_circuits_and_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "false && missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_composed_comparison_logical_expression_returns_bool(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) && (3 < 4)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "!(1 < 2)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_semantic_summary_escapes_string_constants(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "\"a\\\"b\\\\c\\n\\t\"", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("string", result.type_name);
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "expression literal exact");
    assert_text_contains(result.semantic_summary, "constant \"a\\\"b\\\\c\\n\\t\"");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_semantic_summary_reports_unsigned_numeric_range(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "1 + 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("3", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "type int");
    assert_text_contains(result.semantic_summary, "range 3..3");
    assert_text_contains(result.semantic_summary, "unsigned range 3..3");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_semantic_summary_reports_parser_member_reference_fact(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "zr[1]", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "reference member access");
    assert_text_contains(result.reference_summary, "global zr");
    assert_text_contains(result.reference_summary, "index access");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_compiled_function_call_reference_fact(void) {
    const char *source =
            "func pick(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY * 2u];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_function_call_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->topLevelCallableBindingLength > 0);
    TEST_ASSERT_TRUE(function->topLevelCallableBindings[0].callableChildIndex < function->childFunctionLength);
    TEST_ASSERT_TRUE(function->childFunctionList[function->topLevelCallableBindings[0].callableChildIndex]
                             .hasCallableReturnType);
    TEST_ASSERT_TRUE(function->childFunctionList[function->topLevelCallableBindings[0].callableChildIndex]
                             .parameterMetadataCount > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "pick(1 + 2)", summary, sizeof(summary));

    assert_text_contains(summary, "call pick args=1");
    assert_text_contains(summary, "reference call pick");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_compiled_top_level_variable_reference_fact(void) {
    const char *source =
            "var globalSeed: int = 2;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_global_variable_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "globalSeed + 1", summary, sizeof(summary));

    assert_text_contains(summary, "reference read globalSeed");
    assert_text_contains(summary, "expression binary");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_compiled_ownership_fact(void) {
    const char *source =
            "var owner: %unique int;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_ownership_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "%borrow(owner)", summary, sizeof(summary));

    assert_text_contains(summary, "reference read owner");
    assert_text_contains(summary, "expression ownership exact");
    assert_text_contains(summary, "ownership borrow %borrowed");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_walks_type_query_operand_reference(void) {
    const char *source =
            "var owner: %unique int;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_type_query_operand_reference.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "%type(owner)", summary, sizeof(summary));

    assert_text_contains(summary, "reference read owner");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_member_expression_payload_fact(void) {
    const char *source =
            "var seed: int = 2;\n"
            "var index: int = 1;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_member_payload_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength >= 2);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "seed[index]", summary, sizeof(summary));

    assert_text_contains(summary, "member index");
    assert_text_contains(summary, "reference member access index");
    assert_text_contains(summary, "reference read index");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_assignment_write_reference_facts(void) {
    const char *source =
            "var globalSeed: int = 2;\n"
            "var seed: int = 0;\n"
            "var index: int = 1;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY * 2u];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_assignment_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength >= 3);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "globalSeed = 3", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "reference write globalSeed");

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "seed.value = 3", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "member value");
    assert_text_contains(summary, "reference member write value");

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "seed[index] = 4", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "member index");
    assert_text_contains(summary, "reference member write index");
    assert_text_contains(summary, "reference read index");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_walks_member_receiver_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "{a: 1 + 2}.a", summary, sizeof(summary));

    assert_text_contains(summary, "expression member exact");
    assert_text_contains(summary, "member a");
    assert_text_contains(summary, "reference member access a");
    assert_text_contains(summary, "expression object exact");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");
    assert_text_not_contains(summary, "constant 1");
    assert_text_not_contains(summary, "constant 2");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_replays_conditional_branch_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "true ? 1 : 2", summary, sizeof(summary));

    assert_text_contains(summary, "expression conditional exact");
    assert_text_contains(summary, "range 1..1");
    assert_text_contains(summary, "logical true");
    assert_text_contains(summary, "unreachable because a constant branch skips evaluation");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_semantic_summary_replays_boolean_runtime_condition_fact(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_Value_InitAsBool(state, &state->global->zrObject, ZR_TRUE);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "zr ? 1 : 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("1", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "reference read zr");
    assert_text_contains(result.semantic_summary, "logical true");
    assert_text_contains(result.semantic_summary, "unreachable because a constant branch skips evaluation");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_semantic_summary_walks_lambda_local_initializer_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent,
                                              1,
                                              "() => { var folded = 1 + 2; return folded; }",
                                              summary,
                                              sizeof(summary));

    assert_text_contains(summary, "expression lambda exact");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");
    assert_text_contains(summary, "reference read folded");
    assert_text_not_contains(summary, "constant 1");
    assert_text_not_contains(summary, "constant 2");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_comparison_short_circuits_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) || missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(2 < 1) && missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "true ? 1 : missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("1", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "false ? missingLocal : 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("2", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reference_summary_tracks_selected_branch_reads(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "zr ? loadedModules : missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.reference_summary, "global zr");
    assert_text_contains(result.reference_summary, "global loadedModules");
    assert_text_not_contains(result.reference_summary, "missingLocal");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_ternary_consequent_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true ? : 2", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing consequent expression in conditional expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_ternary_alternate_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true ? 1 :", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing alternate expression in conditional expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"text\" + 1", &result, error, sizeof(error)));
    assert_text_contains(error, "numeric operands");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "Use numeric operands");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 / 0", &result, error, sizeof(error)));
    assert_text_contains(error, "division by zero");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "guard the divisor");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_invalid_numeric_literal_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1..2", &result, error, sizeof(error)));
    assert_text_contains(error, "Invalid numeric literal");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "debug evaluate");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "single decimal point");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true.", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing member name after '.'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "member access");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_index_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "false || true[0", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ']' in index access");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_group_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "(true || false", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ')' in grouped expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"open", &result, error, sizeof(error)));
    assert_text_contains(error, "Unterminated string literal");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "debug evaluate");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "closing quote");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"bad\\q\"", &result, error, sizeof(error)));
    assert_text_contains(error, "Unsupported string escape");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "\\q");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "supported escape");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_rejects_function_call_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "sideEffect()", &result, error, sizeof(error)));
    assert_text_contains(error, "Function calls are not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "safe debug evaluate");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_rejects_assignment_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true = false", &result, error, sizeof(error)));
    assert_text_contains(error, "Assignment is not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "read-only");
    assert_text_contains(error, "Suggestion:");

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "local = 1", &result, error, sizeof(error)));
    assert_text_contains(error, "Assignment is not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "read-only");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_expression_reports_missing_logical_operand_with_suggestion);
    RUN_TEST(test_debug_condition_short_circuits_or_without_resolving_missing_rhs);
    RUN_TEST(test_debug_condition_short_circuits_and_without_resolving_missing_rhs);
    RUN_TEST(test_debug_evaluate_composed_comparison_logical_expression_returns_bool);
    RUN_TEST(test_debug_evaluate_semantic_summary_escapes_string_constants);
    RUN_TEST(test_debug_evaluate_semantic_summary_reports_unsigned_numeric_range);
    RUN_TEST(test_debug_evaluate_semantic_summary_reports_parser_member_reference_fact);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_function_call_reference_fact);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_top_level_variable_reference_fact);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_ownership_fact);
    RUN_TEST(test_debug_semantic_summary_walks_type_query_operand_reference);
    RUN_TEST(test_debug_semantic_summary_replays_member_expression_payload_fact);
    RUN_TEST(test_debug_semantic_summary_replays_assignment_write_reference_facts);
    RUN_TEST(test_debug_semantic_summary_walks_member_receiver_facts);
    RUN_TEST(test_debug_semantic_summary_replays_conditional_branch_facts);
    RUN_TEST(test_debug_evaluate_semantic_summary_replays_boolean_runtime_condition_fact);
    RUN_TEST(test_debug_semantic_summary_walks_lambda_local_initializer_facts);
    RUN_TEST(test_debug_condition_comparison_short_circuits_without_resolving_missing_rhs);
    RUN_TEST(test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch);
    RUN_TEST(test_debug_condition_reference_summary_tracks_selected_branch_reads);
    RUN_TEST(test_debug_condition_reports_missing_ternary_consequent_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_ternary_alternate_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_invalid_numeric_literal_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_index_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_group_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_rejects_function_call_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_rejects_assignment_with_cause_and_suggestion);
    RUN_TEST(test_debug_semantic_binding_preserves_paused_frame_canonical_identity);
    return UNITY_END();
}
