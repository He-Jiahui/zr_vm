#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/compiler_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;
static SZrSyntaxContextTable *g_contextTable;
static SZrSyntaxContext g_defSite;
static SZrAstNode *g_ast;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrInferredType g_result;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion;
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_result, ZR_VALUE_TYPE_STRING);
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    memset(&expansion, 0, sizeof(expansion));
    expansion.stableIdentity = 0xA115U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
}

void tearDown(void) {
    ZrParser_InferredType_Free(g_state, &g_result);
    if (g_compilerInitialized) {
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_contextTable != ZR_NULL) {
        ZrParser_SyntaxContextTable_Release(g_contextTable);
        g_contextTable = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNodeArray *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "callable_return_inference.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    g_compiler.scriptAst = g_ast;
    g_compiler.currentFunctionNode = g_ast;
    return g_ast->data.script.statements;
}

static SZrAstNode *parse_function(const TZrChar *source) {
    SZrAstNodeArray *statements = parse_source(source);

    TEST_ASSERT_GREATER_THAN_UINT(0U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[0]->type);
    return statements->nodes[0];
}

static SZrAstNode *first_return_expression(SZrAstNode *functionNode) {
    SZrAstNode *body = functionNode->data.functionDeclaration.body;
    SZrAstNode *returnNode;

    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_GREATER_THAN_UINT(0U, body->data.block.body->count);
    returnNode = body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    TEST_ASSERT_NOT_NULL(returnNode->data.returnStatement.expr);
    return returnNode->data.returnStatement.expr;
}

static void register_outer_value(SZrString *name, EZrValueType valueType) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, valueType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state, g_compiler.typeEnv, name, &type));
    ZrParser_InferredType_Free(g_state, &type);
}

static void assert_query_int(SZrAstNode *declaration) {
    SZrTypeEnvironment *savedEnv = g_compiler.typeEnv;
    SZrAstNode *savedFunction = g_compiler.currentFunctionNode;
    TZrSize savedVariables = savedEnv->variableTypes.length;
    TZrSize savedStackDepth = g_compiler.typeEnvStack.length;
    TZrBool inferred = ZrParser_Compiler_InferCallableReturnType(
            &g_compiler, declaration, &g_result);

    TEST_ASSERT_EQUAL_PTR(savedEnv, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_PTR(savedFunction, g_compiler.currentFunctionNode);
    TEST_ASSERT_EQUAL_UINT(savedVariables, savedEnv->variableTypes.length);
    TEST_ASSERT_EQUAL_UINT(savedStackDepth, g_compiler.typeEnvStack.length);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(inferred);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_result.baseType);
}

static void test_literal_return_query_prerequisite(void) {
    assert_query_int(parse_function("fn sample() { return 7; }"));
}

static void test_registered_parameter_expression_prerequisite(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value: int) { return value; }");
    SZrAstNode *parameter = functionNode->data.functionDeclaration.params->nodes[0];
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, g_compiler.typeEnv, parameter->data.parameter.name->name,
            &type, parameter, parameter->location));
    ZrParser_InferredType_Free(g_state, &type);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            &g_compiler, first_return_expression(functionNode), &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_result.baseType);
}

static void test_query_binds_typed_function_parameters(void) {
    assert_query_int(parse_function(
            "fn sample(left: int, right: int) { return left + right; }"));
}

static void test_parameter_type_shadows_outer_same_spelling(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value: int) { return value; }");
    SZrString *name = functionNode->data.functionDeclaration.params->nodes[0]
                             ->data.parameter.name->name;

    register_outer_value(name, ZR_VALUE_TYPE_STRING);
    assert_query_int(functionNode);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupVariable(
            g_state, g_compiler.typeEnv, name, &g_result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, g_result.baseType);
}

static void test_query_binds_definition_site_parameter(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value: int) { return value; }");
    SZrAstNode *parameter = functionNode->data.functionDeclaration.params->nodes[0];
    SZrAstNode *expression = first_return_expression(functionNode);

    parameter->data.parameter.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expression->type);
    expression->data.identifier.syntaxContext = g_defSite;
    register_outer_value(parameter->data.parameter.name->name, ZR_VALUE_TYPE_STRING);
    assert_query_int(functionNode);
}

static void test_untyped_parameter_does_not_capture_outer_exact_type(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value) { return value; }");
    SZrString *name = functionNode->data.functionDeclaration.params->nodes[0]
                             ->data.parameter.name->name;
    SZrTypeEnvironment *savedEnv = g_compiler.typeEnv;

    register_outer_value(name, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_FALSE(ZrParser_Compiler_InferCallableReturnType(
            &g_compiler, functionNode, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(savedEnv, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.currentFunctionNode);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, g_result.baseType);
}

static void test_query_binds_lambda_parameters(void) {
    SZrAstNodeArray *statements = parse_source(
            "let sample = fn(value: int) => value;");
    SZrAstNode *lambda = statements->nodes[0]->data.variableDeclaration.value;

    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    assert_query_int(lambda);
}

static void test_conflicting_returns_restore_query_state(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value: int) { if (true) { return 7; } return \"text\"; }");
    SZrTypeEnvironment *savedEnv = g_compiler.typeEnv;
    TZrSize savedStackDepth = g_compiler.typeEnvStack.length;

    TEST_ASSERT_FALSE(ZrParser_Compiler_InferCallableReturnType(
            &g_compiler, functionNode, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_TRUE(g_compiler.hasStructuredError);
    TEST_ASSERT_EQUAL_PTR(savedEnv, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.currentFunctionNode);
    TEST_ASSERT_EQUAL_UINT(0U, savedEnv->variableTypes.length);
    TEST_ASSERT_EQUAL_UINT(savedStackDepth, g_compiler.typeEnvStack.length);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, g_result.baseType);
}

static void test_function_alias_reuses_parameterized_return_query(void) {
    SZrAstNodeArray *statements = parse_source(
            "fn sample(value: int) { return value; } let alias = sample;");
    SZrAstNodeArray declarations = *statements;
    SZrVariableDeclaration *alias = &statements->nodes[1]->data.variableDeclaration;
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    declarations.count = 1U;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, &declarations);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Compiler_RegisterCallableValueBinding(
            &g_compiler, alias->pattern->data.identifier.name, alias->value);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, alias->pattern->data.identifier.name, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resolved->returnType.baseType);
}

static void test_generic_parameter_query_keeps_declaration_context(void) {
    SZrAstNode *functionNode = parse_function(
            "fn identity<T>(value: T) { return value; }");
    SZrTypeEnvironment *savedEnv = g_compiler.typeEnv;

    TEST_ASSERT_TRUE(ZrParser_Compiler_InferCallableReturnType(
            &g_compiler, functionNode, &g_result));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(savedEnv, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.currentFunctionNode);
    TEST_ASSERT_NOT_NULL(g_result.typeName);
    TEST_ASSERT_EQUAL_STRING("T", ZrCore_String_GetNativeString(g_result.typeName));
}

static void test_class_method_query_binds_its_parameters(void) {
    SZrAstNodeArray *statements = parse_source(
            "class Widget { pub fn sample(value: int) { return value; } }");
    SZrAstNode *classNode = statements->nodes[0];
    SZrAstNode *method;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    method = classNode->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, method->type);
    assert_query_int(method);
}

static void test_meta_function_query_binds_its_parameters(void) {
    SZrAstNodeArray *statements = parse_source(
            "class Widget { pub @constructor(value: int) { return value; } }");
    SZrAstNode *classNode = statements->nodes[0];
    SZrAstNode *method;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    method = classNode->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, method->type);
    assert_query_int(method);
}

static void test_repeated_queries_do_not_publish_parameters_to_parent(void) {
    SZrAstNode *functionNode = parse_function(
            "fn sample(value: int) { var local = value; return local; }");
    SZrString *parameterName = functionNode->data.functionDeclaration.params->nodes[0]
                                      ->data.parameter.name->name;

    for (TZrSize index = 0U; index < 3U; index++) {
        assert_query_int(functionNode);
        TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindVariableBinding(
                g_compiler.typeEnv, parameterName));
    }
}

static void test_existing_error_preserves_query_result_and_environment(void) {
    SZrAstNode *functionNode = parse_function("fn sample() { return 7; }");
    SZrTypeEnvironment *savedEnv = g_compiler.typeEnv;

    g_compiler.hasError = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_Compiler_InferCallableReturnType(
            &g_compiler, functionNode, &g_result));
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(savedEnv, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.currentFunctionNode);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, g_result.baseType);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_literal_return_query_prerequisite);
    RUN_TEST(test_registered_parameter_expression_prerequisite);
    RUN_TEST(test_query_binds_typed_function_parameters);
    RUN_TEST(test_parameter_type_shadows_outer_same_spelling);
    RUN_TEST(test_query_binds_definition_site_parameter);
    RUN_TEST(test_untyped_parameter_does_not_capture_outer_exact_type);
    RUN_TEST(test_query_binds_lambda_parameters);
    RUN_TEST(test_conflicting_returns_restore_query_state);
    RUN_TEST(test_function_alias_reuses_parameterized_return_query);
    RUN_TEST(test_generic_parameter_query_keeps_declaration_context);
    RUN_TEST(test_class_method_query_binds_its_parameters);
    RUN_TEST(test_meta_function_query_binds_its_parameters);
    RUN_TEST(test_repeated_queries_do_not_publish_parameters_to_parent);
    RUN_TEST(test_existing_error_preserves_query_result_and_environment);
    return UNITY_END();
}
