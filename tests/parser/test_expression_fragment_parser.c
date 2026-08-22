#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static SZrState *g_state;

typedef struct SExpressionFragmentDiagnosticCapture {
    TZrUInt32 structuredErrorCount;
    TZrUInt32 legacyErrorCount;
    char firstLegacyMessage[192];
} SExpressionFragmentDiagnosticCapture;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void capture_structured_error(TZrPtr userData,
                                     const SZrStructuredDiagnostic *diagnostic,
                                     EZrToken token) {
    SExpressionFragmentDiagnosticCapture *capture =
            (SExpressionFragmentDiagnosticCapture *)userData;

    ZR_UNUSED_PARAMETER(token);
    if (capture != ZR_NULL && diagnostic != ZR_NULL &&
        diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        capture->structuredErrorCount++;
    }
}

static void capture_legacy_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SExpressionFragmentDiagnosticCapture *capture =
            (SExpressionFragmentDiagnosticCapture *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }

    if (capture->legacyErrorCount == 0u && message != ZR_NULL) {
        snprintf(capture->firstLegacyMessage,
                 sizeof(capture->firstLegacyMessage),
                 "%s",
                 message);
    }
    capture->legacyErrorCount++;
}

static SZrAstNode *parse_fragment(const TZrChar *source,
                                  SExpressionFragmentDiagnosticCapture *capture,
                                  SZrParserState *outParserState) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(outParserState);
    memset(capture, 0, sizeof(*capture));

    sourceName = ZrCore_String_CreateFromNative(g_state, "debug_expression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(outParserState,
                        g_state,
                        source,
                        strlen(source),
                        sourceName);
    outParserState->structuredErrorCallback = capture_structured_error;
    outParserState->errorCallback = capture_legacy_error;
    outParserState->errorUserData = capture;
    outParserState->suppressErrorOutput = ZR_TRUE;

    return ZrParser_ParseExpressionWithState(outParserState);
}

static void test_expression_fragment_parses_the_full_formal_expression(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment(
            "true ? 1 : 2",
            &capture,
            &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONDITIONAL_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_UINT32(0u, capture.structuredErrorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, capture.legacyErrorCount);

    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_reports_structured_parser_errors(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("value +", &capture, &parserState);

    TEST_ASSERT_NULL(expression);
    TEST_ASSERT_TRUE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.structuredErrorCount);

    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_rejects_trailing_tokens(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("one two", &capture, &parserState);

    TEST_ASSERT_NULL(expression);
    TEST_ASSERT_TRUE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.legacyErrorCount);
    TEST_ASSERT_NOT_NULL(strstr(capture.firstLegacyMessage,
                                "Unexpected token after expression"));

    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_preserves_external_canonical_binding_identity(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrFileRange declarationRange;
    const SZrSemanticReferenceFact *reference;
    SZrAstNode *expression = parse_fragment("paused", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, g_state);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);

    declarationRange = expression->location;
    declarationRange.start.offset = 400u;
    declarationRange.end.offset = 406u;
    declarationRange.start.line = 21u;
    declarationRange.end.line = 21u;
    declarationRange.start.column = 5u;
    declarationRange.end.column = 11u;

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
            g_state,
            compilerState.typeEnv,
            expression->data.identifier.name,
            &inferredType,
            7001u,
            7002u,
            7003u,
            declarationRange));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            &compilerState,
            expression,
            &inferredType));

    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compilerState.semanticContext,
            expression,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(7001u, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(7002u, reference->typeId);
    TEST_ASSERT_EQUAL_UINT32(7003u, reference->placeId);
    TEST_ASSERT_EQUAL_UINT32(400u, (TZrUInt32)reference->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(406u, (TZrUInt32)reference->declarationRange.end.offset);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_marks_ordinary_binding_place_unavailable(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrFileRange declarationRange;
    const SZrSemanticReferenceFact *reference;
    const SZrTypeBinding *binding;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    SZrAstNode *expression = parse_fragment("ordinary", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, g_state);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);
    declarationRange = expression->location;

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            compilerState.typeEnv,
            expression->data.identifier.name,
            &inferredType,
            expression,
            declarationRange));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compilerState.typeEnv,
            expression->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    symbolId = binding->symbolId;
    typeId = binding->typeId;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
            g_state,
            compilerState.typeEnv,
            expression->data.identifier.name,
            &inferredType,
            symbolId,
            typeId,
            8003u,
            declarationRange));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            compilerState.typeEnv,
            expression->data.identifier.name,
            &inferredType,
            expression,
            declarationRange));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            &compilerState,
            expression,
            &inferredType));

    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compilerState.semanticContext,
            expression,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(0u, reference->placeId);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_preserves_runtime_root_origin(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    const SZrTypeBinding *binding;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticReferenceFact *alternateReference;
    SZrAstNode *rootIdentifier;
    SZrAstNode *alternateIdentifier;
    SZrAstNode *expression = parse_fragment("zr != null ? zr : zr", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONDITIONAL_EXPRESSION, expression->type);
    rootIdentifier = expression->data.conditionalExpression.consequent;
    alternateIdentifier = expression->data.conditionalExpression.alternate;
    TEST_ASSERT_NOT_NULL(rootIdentifier);
    TEST_ASSERT_NOT_NULL(alternateIdentifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, rootIdentifier->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, alternateIdentifier->type);
    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, g_state);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            g_state,
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_NONE,
            9001u));
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            g_state,
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_ZR,
            0u));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            g_state,
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_ZR,
            9001u));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT, binding->originKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_ZR, binding->runtimeRootKind);
    TEST_ASSERT_EQUAL_UINT64(9001u, binding->originToken);
    TEST_ASSERT_EQUAL_UINT32(0u, binding->placeId);
    TEST_ASSERT_FALSE(binding->hasDeclarationRange);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            g_state,
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_ZR,
            9002u));

    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            &compilerState,
            expression,
            &inferredType));
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compilerState.semanticContext,
            rootIdentifier,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(binding->typeId, reference->typeId);
    TEST_ASSERT_EQUAL_UINT32(0u, reference->placeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT, reference->originKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_ZR, reference->runtimeRootKind);
    TEST_ASSERT_EQUAL_UINT64(9001u, reference->originToken);
    TEST_ASSERT_NULL(reference->declarationRange.source);
    TEST_ASSERT_FALSE(reference->hasDefinitionRange);
    alternateReference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compilerState.semanticContext,
            alternateIdentifier,
            ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(alternateReference);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_REFERENCE_ORIGIN_RUNTIME_ROOT,
            alternateReference->originKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_ZR, alternateReference->runtimeRootKind);
    TEST_ASSERT_EQUAL_UINT64(9001u, alternateReference->originToken);
    TEST_ASSERT_EQUAL_UINT32(0u, alternateReference->placeId);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state,
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name,
            &inferredType,
            rootIdentifier,
            rootIdentifier->location));
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compilerState.typeEnv,
            rootIdentifier->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION, binding->originKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RUNTIME_ROOT_NONE, binding->runtimeRootKind);
    TEST_ASSERT_EQUAL_UINT64(0u, binding->originToken);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_import_expression_releases_normalized_module_path(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("import(\"native.math\")", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.importExpression.modulePath);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL,
                          expression->data.importExpression.modulePath->type);
    TEST_ASSERT_FALSE(parserState.hasError);

    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_type_cast_releases_member_call_subtree(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("<int> receiver.read(1)", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_CAST_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.typeCastExpression.targetType);
    TEST_ASSERT_NOT_NULL(expression->data.typeCastExpression.expression);
    TEST_ASSERT_FALSE(parserState.hasError);

    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_releases_logical_operands(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("true || false", &capture, &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.logicalExpression.left);
    TEST_ASSERT_NOT_NULL(expression->data.logicalExpression.right);

    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_script_releases_module_declaration_subtree(void) {
    static const TZrChar source[] =
            "module lifecycle.cleanup;\n"
            "var value: int = 1;\n";
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *script;

    sourceName = ZrCore_String_CreateFromNative(g_state, "module_cleanup.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState,
                        g_state,
                        source,
                        strlen(source),
                        sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.moduleName);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MODULE_DECLARATION,
                          script->data.script.moduleName->type);
    TEST_ASSERT_NOT_NULL(script->data.script.moduleName->data.moduleDeclaration.name);

    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static void test_script_releases_decorator_lookahead_subtree(void) {
    static const TZrChar source[] =
            "#zr.testing.test# fn decorated(): void { }\n";
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *script;

    sourceName = ZrCore_String_CreateFromNative(g_state, "decorator_cleanup.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState,
                        g_state,
                        source,
                        strlen(source),
                        sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION,
                          script->data.script.statements->nodes[0]->type);

    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static void test_script_releases_destructuring_and_partial_declarations(void) {
    static const TZrChar validSource[] =
            "module lifecycle.cleanup;\n"
            "var { key } = source;\n";
    static const TZrChar invalidSource[] = "var missing: int = ;\n";
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *script;

    sourceName = ZrCore_String_CreateFromNative(g_state, "destructuring_cleanup.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState,
                        g_state,
                        validSource,
                        strlen(validSource),
                        sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DESTRUCTURING_OBJECT,
                          script->data.script.statements->nodes[0]
                                  ->data.variableDeclaration.pattern->type);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);

    sourceName = ZrCore_String_CreateFromNative(g_state, "partial_declaration_cleanup.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState,
                        g_state,
                        invalidSource,
                        strlen(invalidSource),
                        sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_TRUE(parserState.hasError);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static void test_compiler_state_releases_child_function_name_map(void) {
    SZrCompilerState compilerState;

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, g_state);
    TEST_ASSERT_TRUE(compilerState.childFunctionNameMap.isValid);

    ZrParser_CompilerState_Free(&compilerState);
    TEST_ASSERT_FALSE(compilerState.childFunctionNameMap.isValid);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_expression_fragment_parses_the_full_formal_expression);
    RUN_TEST(test_expression_fragment_reports_structured_parser_errors);
    RUN_TEST(test_expression_fragment_rejects_trailing_tokens);
    RUN_TEST(test_expression_fragment_preserves_external_canonical_binding_identity);
    RUN_TEST(test_expression_fragment_marks_ordinary_binding_place_unavailable);
    RUN_TEST(test_expression_fragment_preserves_runtime_root_origin);
    RUN_TEST(test_import_expression_releases_normalized_module_path);
    RUN_TEST(test_type_cast_releases_member_call_subtree);
    RUN_TEST(test_expression_fragment_releases_logical_operands);
    RUN_TEST(test_script_releases_module_declaration_subtree);
    RUN_TEST(test_script_releases_decorator_lookahead_subtree);
    RUN_TEST(test_script_releases_destructuring_and_partial_declarations);
    RUN_TEST(test_compiler_state_releases_child_function_name_map);
    return UNITY_END();
}
