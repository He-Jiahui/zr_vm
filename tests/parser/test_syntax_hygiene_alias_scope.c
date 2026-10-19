#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/compiler_internal.h"
#include "compiler/compiler_type_value_alias.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrFunction *g_function;
static SZrFunction *g_executionFunction;
static SZrSyntaxContextTable *g_contextTable;
static SZrSyntaxContext g_defSite;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    expansion.stableIdentity = 0xA113U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_function = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_function);
    g_compiler.currentFunction = g_function;
}

void tearDown(void) {
    if (g_compilerInitialized) {
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_function != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_function);
        g_function = ZR_NULL;
    }
    if (g_executionFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_executionFunction);
        g_executionFunction = ZR_NULL;
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
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "alias_scope.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    return g_ast->data.script.statements;
}

static SZrString *register_outer_and_enter_scope(SZrAstNodeArray **outInner,
        SZrSyntaxContext outerContext, SZrSyntaxContext innerContext) {
    SZrAstNodeArray *statements = parse_source(
            "let Vector = int[]; fn nested() { let Vector = string[]; }");
    SZrAstNode *outer = statements->nodes[0];
    SZrAstNode *block;
    SZrAstNodeArray outerStatement = {0};

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[1]->type);
    block = statements->nodes[1]->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, block->type);
    *outInner = block->data.block.body;
    TEST_ASSERT_NOT_NULL(*outInner);
    outer->data.variableDeclaration.pattern->data.identifier.syntaxContext = outerContext;
    (*outInner)->nodes[0]->data.variableDeclaration.pattern->data.identifier.syntaxContext = innerContext;
    outerStatement.nodes = &outer;
    outerStatement.count = 1U;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, &outerStatement);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    enter_scope(&g_compiler);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.scopeStack.length);
    return outer->data.variableDeclaration.pattern->data.identifier.name;
}

static void assert_alias_element(SZrString *name, SZrSyntaxContext context,
        EZrValueType expected) {
    SZrTypeBinding *binding = compiler_find_type_value_alias(&g_compiler, name, context);
    SZrInferredType *element;

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, binding->type.baseType);
    TEST_ASSERT_EQUAL_UINT(1U, binding->type.elementTypes.length);
    element = ZrCore_Array_Get(&binding->type.elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(element);
    TEST_ASSERT_EQUAL_INT(expected, element->baseType);
}

static void test_outer_alias_prerequisite(void) {
    SZrAstNodeArray *inner;
    SZrString *name = register_outer_and_enter_scope(&inner,
            ZrParser_SyntaxContext_Root(), ZrParser_SyntaxContext_Root());

    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
}

static void test_nested_alias_keeps_outer_storage(void) {
    SZrAstNodeArray *inner;
    SZrString *name = register_outer_and_enter_scope(&inner, g_defSite, g_defSite);

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, inner);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_STRING);
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typeValueAliases.length);
}

static void test_nested_alias_exit_restores_outer_type(void) {
    SZrAstNodeArray *inner;
    SZrString *name = register_outer_and_enter_scope(&inner, g_defSite, g_defSite);

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, inner);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_STRING);
    exit_scope(&g_compiler);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_INT64);
}

static void test_nested_different_context_does_not_hide_outer(void) {
    SZrAstNodeArray *inner;
    SZrString *name = register_outer_and_enter_scope(&inner,
            ZrParser_SyntaxContext_Root(), g_defSite);

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, inner);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_STRING);
    exit_scope(&g_compiler);
    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, name, g_defSite));
}

static void assert_execution(TZrInt64 expected) {
    TZrInt64 result;

    g_executionFunction = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NOT_NULL(g_executionFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, g_executionFunction, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
}

static void test_block_alias_shadow_restores_outer_annotation(void) {
    parse_source(
            "let Vector = int[];"
            "if (true) { let Vector = string[]; let inner: Vector = [\"inner\"]; }"
            "let outer: Vector = [7]; return outer[0];");
    assert_execution(7);
}

static void test_function_alias_shadow_restores_outer_annotation(void) {
    parse_source(
            "let Vector = int[];"
            "fn sample(): int { let Vector = string[];"
            "    let inner: Vector = [\"inner\"]; return inner.length; }"
            "let outer: Vector = [7]; return outer[0] + sample();");
    assert_execution(8);
}

static void test_function_local_alias_does_not_escape(void) {
    SZrAstNodeArray *statements = parse_source(
            "fn sample(): int { let Hidden = int[]; return 7; }");
    SZrAstNode *functionNode = statements->nodes[0];
    SZrAstNode *alias = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    SZrString *name = alias->data.variableDeclaration.pattern->data.identifier.name;

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, functionNode);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(
            &g_compiler, name, ZrParser_SyntaxContext_Root()));
}

static void test_failed_block_restores_outer_alias(void) {
    SZrAstNodeArray *statements = parse_source(
            "let Vector = int[]; fn nested() { let Vector = string[]; missing(); }");
    SZrAstNode *outer = statements->nodes[0];
    SZrAstNodeArray outerStatement = {0};
    SZrString *name = outer->data.variableDeclaration.pattern->data.identifier.name;

    outerStatement.nodes = &outer;
    outerStatement.count = 1U;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, &outerStatement);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    enter_scope(&g_compiler);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statements->nodes[1]->type);
    ZrParser_Statement_Compile(&g_compiler,
            statements->nodes[1]->data.functionDeclaration.body);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.scopeStack.length);
}

static void test_same_scope_predeclaration_reuses_alias(void) {
    SZrAstNodeArray *statements = parse_source("let Vector = int[];");
    SZrString *name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeValueAliases.length);
    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
}

static void test_three_alias_scopes_restore_each_parent(void) {
    SZrAstNodeArray *inner;
    SZrString *name = register_outer_and_enter_scope(&inner, g_defSite, g_defSite);
    SZrAstNode *outerLiteral = g_ast->data.script.statements->nodes[0]
            ->data.variableDeclaration.value;

    enter_scope(&g_compiler);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, inner);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    enter_scope(&g_compiler);
    compiler_register_type_value_alias_literal(&g_compiler, name, outerLiteral, g_defSite);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_EQUAL_UINT(3U, g_compiler.typeValueAliases.length);
    exit_scope(&g_compiler);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_STRING);
    exit_scope(&g_compiler);
    assert_alias_element(name, g_defSite, ZR_VALUE_TYPE_INT64);
}

static void test_script_alias_remains_available_after_root_scope_exit(void) {
    SZrAstNodeArray *statements = parse_source(
            "let Vector = int[]; let values: Vector = [7]; return values[0];");
    SZrString *name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;

    compile_script(&g_compiler, g_ast);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.scopeStack.length);
    assert_alias_element(name, ZrParser_SyntaxContext_Root(), ZR_VALUE_TYPE_INT64);
}

static void test_lambda_alias_shadow_restores_outer_annotation(void) {
    parse_source(
            "let Vector = int[]; let sample = fn() => {"
            "    let Vector = string[]; let inner: Vector = [\"inner\"];"
            "    return inner.length; };"
            "let outer: Vector = [7]; return outer[0] + sample();");
    assert_execution(8);
}

static void test_root_import_alias_prerequisite(void) {
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    parse_source(
            "var math = import(\"zr.math\");"
            "var {Vec: Vector3} = import(\"zr.math\");"
            "var value: Vec = init math.Vector3(7.0, 2.0, 3.0);"
            "return <int>value.x;");
    assert_execution(7);
}

static void test_inner_import_alias_shadows_outer_array_alias(void) {
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    parse_source(
            "var math = import(\"zr.math\"); let Vec = int[]; var result = 0;"
            "if (true) { var {Vec: Vector3} = import(\"zr.math\");"
            "    var value: Vec = init math.Vector3(7.0, 2.0, 3.0);"
            "    result = <int>value.x; }"
            "var outer: Vec = [11]; return result + outer[0];");
    assert_execution(18);
}

static void test_inner_import_alias_shadows_outer_import_alias(void) {
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    parse_source(
            "var math = import(\"zr.math\");"
            "var {Vec: Vector2} = import(\"zr.math\"); var result = 0;"
            "if (true) { var {Vec: Vector3} = import(\"zr.math\");"
            "    var value: Vec = init math.Vector3(7.0, 2.0, 3.0);"
            "    result = <int>value.x; }"
            "return result;");
    assert_execution(7);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_outer_alias_prerequisite);
    RUN_TEST(test_nested_alias_keeps_outer_storage);
    RUN_TEST(test_nested_alias_exit_restores_outer_type);
    RUN_TEST(test_nested_different_context_does_not_hide_outer);
    RUN_TEST(test_block_alias_shadow_restores_outer_annotation);
    RUN_TEST(test_function_alias_shadow_restores_outer_annotation);
    RUN_TEST(test_function_local_alias_does_not_escape);
    RUN_TEST(test_failed_block_restores_outer_alias);
    RUN_TEST(test_same_scope_predeclaration_reuses_alias);
    RUN_TEST(test_three_alias_scopes_restore_each_parent);
    RUN_TEST(test_script_alias_remains_available_after_root_scope_exit);
    RUN_TEST(test_lambda_alias_shadow_restores_outer_annotation);
    RUN_TEST(test_root_import_alias_prerequisite);
    RUN_TEST(test_inner_import_alias_shadows_outer_array_alias);
    RUN_TEST(test_inner_import_alias_shadows_outer_import_alias);
    return UNITY_END();
}
