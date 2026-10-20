#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "compiler/compiler_internal.h"
#include "compiler/compiler_type_value_alias.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static SZrState *g_state;
static SZrSyntaxContextTable *g_contextTable;
static SZrAstNode *g_ast;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrSyntaxContext g_callSite;
static SZrSyntaxContext g_defSite;
static SZrArray g_functionCandidates;
static SZrInferredType g_inferredType;
static SZrFunction *g_function;

void setUp(void) {
    SZrSyntaxExpansionInfo expansion;
    TZrSyntaxExpansionId expansionId;

    ZrCore_Array_Construct(&g_functionCandidates);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_inferredType, ZR_VALUE_TYPE_OBJECT);
    g_contextTable = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_contextTable);
    memset(&expansion, 0, sizeof(expansion));
    expansion.stableIdentity = 0xA110U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_contextTable, &expansion, &expansionId));
    g_callSite = ZrParser_SyntaxContext_CallSite(g_contextTable, expansionId);
    g_defSite = ZrParser_SyntaxContext_DefSite(g_contextTable, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_callSite));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(g_defSite));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(g_compiler.typeEnv);
}

void tearDown(void) {
    if (g_function != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_function);
        g_function = ZR_NULL;
    }
    ZrParser_InferredType_Free(g_state, &g_inferredType);
    ZrCore_Array_Free(g_state, &g_functionCandidates);
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

static SZrAstNodeArray *parse_aliases(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "module_alias_hygiene.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    return g_ast->data.script.statements;
}

static void set_alias_context(SZrAstNode *declaration, SZrSyntaxContext context) {
    SZrAstNode *pattern;

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    pattern = declaration->data.variableDeclaration.pattern;
    TEST_ASSERT_NOT_NULL(pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, pattern->type);
    pattern->data.identifier.syntaxContext = context;
}

static void assert_alias_type(
        SZrString *name, SZrSyntaxContext context, const TZrChar *moduleName) {
    const SZrTypeBinding *binding =
            ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                    g_compiler.typeEnv, name, context);

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_NULL(binding->type.typeName);
    TEST_ASSERT_EQUAL_STRING(
            moduleName, ZrCore_String_GetNativeString(binding->type.typeName));
}

static void test_module_alias_predeclaration_separates_definition_and_root(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let tools = import(\"provider.definition\");"
            "let tools = import(\"provider.call\");");
    SZrString *name;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    set_alias_context(statements->nodes[0], g_defSite);
    name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_type(name, g_defSite, "provider.definition");
    assert_alias_type(name, ZrParser_SyntaxContext_Root(), "provider.call");
}

static void test_module_alias_predeclaration_keeps_root_when_definition_is_last(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let tools = import(\"provider.call\");"
            "let tools = import(\"provider.definition\");");
    SZrString *name;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    set_alias_context(statements->nodes[1], g_defSite);
    name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_type(name, ZrParser_SyntaxContext_Root(), "provider.call");
    assert_alias_type(name, g_defSite, "provider.definition");
}

static void test_module_alias_call_site_predeclaration_resolves_from_root(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let tools = import(\"provider.call\");");
    SZrString *name;

    TEST_ASSERT_EQUAL_UINT(1U, statements->count);
    set_alias_context(statements->nodes[0], g_callSite);
    name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_alias_type(name, g_callSite, "provider.call");
    assert_alias_type(name, ZrParser_SyntaxContext_Root(), "provider.call");
}

static SZrString *predeclare_marked_functions(const TZrChar *source) {
    SZrAstNodeArray *statements = parse_aliases(source);
    SZrAstNode *declaration;

    TEST_ASSERT_TRUE(statements->count > 0U);
    declaration = statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.functionDeclaration.name);
    declaration->data.functionDeclaration.name->syntaxContext = g_defSite;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    return declaration->data.functionDeclaration.name->name;
}

static void test_root_function_lookup_excludes_definition_site_candidate(void) {
    SZrString *name = predeclare_marked_functions(
            "fn work(): int { return 1; }"
            "fn work(): string { return \"root\"; }");
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, name, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, resolved->returnType.baseType);
}

static void test_root_overload_set_excludes_definition_site_candidate(void) {
    SZrString *name = predeclare_marked_functions(
            "fn work(): int { return 1; }"
            "fn work(): string { return \"root\"; }");

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunctions(
            g_state, g_compiler.typeEnv, name, &g_functionCandidates));
    TEST_ASSERT_EQUAL_UINT(1U, g_functionCandidates.length);
}

static void test_equal_function_signatures_remain_distinct_across_contexts(void) {
    predeclare_marked_functions(
            "fn work(): int { return 1; }"
            "fn work(): int { return 2; }");

    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typeEnv->functionReturnTypes.length);
}

static SZrString *predeclare_outer_function_and_local_value(SZrSyntaxContext context) {
    SZrAstNodeArray *statements = parse_aliases("fn work(): int { return 1; }");
    SZrString *name = statements->nodes[0]->data.functionDeclaration.name->name;
    SZrTypeEnvironment *child;
    SZrInferredType intType;

    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    child = ZrParser_TypeEnvironment_New(g_state);
    TEST_ASSERT_NOT_NULL(child);
    child->parent = g_compiler.typeEnv;
    g_compiler.typeEnv = child;
    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableWithContext(
            g_state, child, name, &intType, context));
    ZrParser_InferredType_Free(g_state, &intType);
    return name;
}

static void test_definition_site_value_does_not_hide_outer_root_function(void) {
    SZrString *name = predeclare_outer_function_and_local_value(g_defSite);
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, name, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resolved->returnType.baseType);
}

static void test_call_site_value_hides_outer_root_function(void) {
    SZrString *name = predeclare_outer_function_and_local_value(g_callSite);
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    TEST_ASSERT_FALSE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, name, &resolved));
}

static void test_root_callable_alias_reads_definition_site_source(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(): int { return 7; } let task = work;");
    SZrVariableDeclaration *alias = &statements->nodes[1]->data.variableDeclaration;
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    statements->nodes[0]->data.functionDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, alias->value->type);
    alias->value->data.identifier.syntaxContext = g_defSite;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Compiler_RegisterCallableValueBinding(
            &g_compiler, alias->pattern->data.identifier.name, alias->value);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, alias->pattern->data.identifier.name, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resolved->returnType.baseType);
}

static SZrAstNode *marked_function_return_expression(const TZrChar *source) {
    predeclare_marked_functions(source);
    SZrAstNodeArray *statements = g_ast->data.script.statements;
    SZrAstNode *statement = statements->nodes[statements->count - 1U];

    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    return statement->data.returnStatement.expr;
}

static void test_definition_site_function_identifier_infers_callable_type(void) {
    SZrAstNode *expression = marked_function_return_expression(
            "fn work(): int { return 7; } return work;");

    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expression->type);
    expression->data.identifier.syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(ZrParser_IdentifierType_Infer(
            &g_compiler, expression, &g_inferredType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, g_inferredType.baseType);
}

static void test_definition_site_function_call_selects_marked_overload(void) {
    SZrAstNode *expression = marked_function_return_expression(
            "fn work(): int { return 7; }"
            "fn work(): string { return \"root\"; } return work();");
    SZrAstNode *target;

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    target = expression->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, target->type);
    target->data.identifier.syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(ZrParser_PrimaryExpressionType_Infer(
            &g_compiler, expression, &g_inferredType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, g_inferredType.baseType);
}

static void mark_call_target(SZrAstNode *expression, SZrSyntaxContext context) {
    SZrAstNode *target;

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    target = expression->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, target->type);
    target->data.identifier.syntaxContext = context;
}

static void assert_compiled_result(TZrInt64 expected) {
    TZrInt64 result = 0;

    g_function = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NOT_NULL(g_function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, g_function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
}

static void test_marked_equal_signature_functions_execute_separately(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(): int { return 7; }"
            "fn work(): int { return 11; } return work() + work();");
    SZrAstNode *sum = statements->nodes[2]->data.returnStatement.expr;

    statements->nodes[0]->data.functionDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, sum->type);
    mark_call_target(sum->data.binaryExpression.left, g_defSite);
    assert_compiled_result(18);
}

static void test_marked_named_calls_use_their_declarations(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(hidden: int): int { return hidden; }"
            "fn work(visible: int): int { return visible; }"
            "return work(hidden: 7) + work(visible: 11);");
    SZrAstNode *sum = statements->nodes[2]->data.returnStatement.expr;

    statements->nodes[0]->data.functionDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, sum->type);
    mark_call_target(sum->data.binaryExpression.left, g_defSite);
    assert_compiled_result(18);
}

static void test_marked_callable_alias_and_lambda_execute_separately(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(): int { return 7; } let task = work;"
            "let task = fn(): int => 11; return task() + task();");
    SZrAstNode *sum = statements->nodes[3]->data.returnStatement.expr;

    set_alias_context(statements->nodes[1], g_defSite);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, sum->type);
    mark_call_target(sum->data.binaryExpression.left, g_defSite);
    assert_compiled_result(18);
}

static void test_marked_typed_callable_parameter_executes(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn apply(task: fn()->int): int { return task(); }"
            "fn work(): int { return 7; } return apply(work);");
    SZrFunctionDeclaration *apply = &statements->nodes[0]->data.functionDeclaration;
    SZrAstNode *bodyReturn = apply->body->data.block.body->nodes[0];
    TZrBool hasTypedCall = ZR_FALSE;

    apply->params->nodes[0]->data.parameter.name->syntaxContext = g_defSite;
    mark_call_target(bodyReturn->data.returnStatement.expr, g_defSite);
    assert_compiled_result(7);
    TEST_ASSERT_TRUE(g_function->childFunctionLength > 0U);
    for (TZrUInt32 index = 0U;
         index < g_function->childFunctionList[0].callSiteCacheLength; index++) {
        if (g_function->childFunctionList[0].callSiteCaches[index]
                    .binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            hasTypedCall = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(hasTypedCall);
}

static void test_marked_typed_callable_parameter_rejects_wrong_argument(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn apply(task: fn(int)->int): int { return task(\"bad\"); }"
            "fn work(value: int): int { return value; } return apply(work);");
    SZrFunctionDeclaration *apply = &statements->nodes[0]->data.functionDeclaration;
    SZrAstNode *bodyReturn = apply->body->data.block.body->nodes[0];

    apply->params->nodes[0]->data.parameter.name->syntaxContext = g_defSite;
    mark_call_target(bodyReturn->data.returnStatement.expr, g_defSite);
    g_function = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NULL(g_function);
}

static void test_marked_typed_callable_result_rejects_wrong_argument(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn make(): fn(int)->int { return fn(value: int): int => value; }"
            "let task: fn(int)->int = make(); return task(\"bad\");");

    set_alias_context(statements->nodes[1], g_defSite);
    mark_call_target(statements->nodes[2]->data.returnStatement.expr, g_defSite);
    g_function = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NULL(g_function);
}

static void test_root_typed_callable_result_rejects_wrong_argument(void) {
    parse_aliases(
            "fn make(): fn(int)->int { return fn(value: int): int => value; }"
            "let task: fn(int)->int = make(); return task(\"bad\");");
    g_function = ZrParser_Compiler_Compile(g_state, g_ast);
    TEST_ASSERT_NULL(g_function);
}

static void test_marked_typed_callable_result_keeps_typed_contract(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn make(): fn(int)->int { return fn(value: int): int => value; }"
            "let task: fn(int)->int = make(); return task(9);");
    TZrBool hasTypedCall = ZR_FALSE;

    set_alias_context(statements->nodes[1], g_defSite);
    mark_call_target(statements->nodes[2]->data.returnStatement.expr, g_defSite);
    assert_compiled_result(9);
    for (TZrUInt32 index = 0U; index < g_function->callSiteCacheLength; index++) {
        if (g_function->callSiteCaches[index].binding.contract.bindingKind ==
                ZR_CALL_BINDING_TYPED_FUNCTION) {
            hasTypedCall = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(hasTypedCall);
}

static void test_marked_lambda_parameter_infers_callable_return(void) {
    SZrAstNodeArray *statements = parse_aliases("let task = fn(value: int) => value;");
    SZrVariableDeclaration *alias = &statements->nodes[0]->data.variableDeclaration;
    SZrLambdaExpression *lambda = &alias->value->data.lambdaExpression;
    SZrAstNode *bodyReturn;
    SZrFunctionTypeInfo *resolved = ZR_NULL;

    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, alias->value->type);
    lambda->params->nodes[0]->data.parameter.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, lambda->block->type);
    bodyReturn = lambda->block->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, bodyReturn->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, bodyReturn->data.returnStatement.expr->type);
    bodyReturn->data.returnStatement.expr->data.identifier.syntaxContext = g_defSite;
    ZrParser_Compiler_RegisterCallableValueBinding(
            &g_compiler, alias->pattern->data.identifier.name, alias->value);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(
            g_compiler.typeEnv, alias->pattern->data.identifier.name, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resolved->returnType.baseType);
}

static void test_marked_local_metadata_keeps_binding_types_separate(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn sample() { var value = 7; var value = \"root\"; return value; }"
            "return sample();");
    SZrAstNodeArray *body = statements->nodes[0]->data.functionDeclaration.body->data.block.body;
    SZrFunction *child;
    TZrUInt32 valueCount = 0U;

    set_alias_context(body->nodes[0], g_defSite);
    body->nodes[2]->data.returnStatement.expr->data.identifier.syntaxContext = g_defSite;
    assert_compiled_result(7);
    TEST_ASSERT_TRUE(g_function->childFunctionLength > 0U);
    child = &g_function->childFunctionList[0];
    for (TZrUInt32 index = 0U; index < child->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &child->typedLocalBindings[index];

        if (binding->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(binding->name), "value") == 0) {
            TEST_ASSERT_EQUAL_INT(valueCount == 0U ? ZR_VALUE_TYPE_INT64 : ZR_VALUE_TYPE_STRING,
                                  binding->type.baseType);
            valueCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2U, valueCount);
}

static SZrAstNodeArray *predeclare_marked_type_value_aliases(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let Vector = int[]; let Vector = string[];"
            "let hidden: Vector = [7]; let visible: Vector = [\"root\"];"
            "return hidden[0];");
    SZrType *hiddenType = statements->nodes[2]->data.variableDeclaration.typeInfo;

    set_alias_context(statements->nodes[0], g_defSite);
    TEST_ASSERT_NOT_NULL(hiddenType);
    TEST_ASSERT_NOT_NULL(hiddenType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, hiddenType->name->type);
    hiddenType->name->data.identifier.syntaxContext = g_defSite;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    return statements;
}

static void test_type_value_predeclaration_keeps_definition_and_root(void) {
    predeclare_marked_type_value_aliases();
    TEST_ASSERT_EQUAL_UINT(2U, g_compiler.typeValueAliases.length);
}

static void test_marked_type_value_annotation_selects_definition_alias(void) {
    SZrAstNodeArray *statements = predeclare_marked_type_value_aliases();
    SZrType *hiddenType = statements->nodes[2]->data.variableDeclaration.typeInfo;
    SZrInferredType *element;

    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, hiddenType, &g_inferredType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, g_inferredType.baseType);
    TEST_ASSERT_EQUAL_UINT(1U, g_inferredType.elementTypes.length);
    element = ZrCore_Array_Get(&g_inferredType.elementTypes, 0U);
    TEST_ASSERT_NOT_NULL(element);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, element->baseType);
}

static void test_marked_type_value_aliases_compile_and_execute(void) {
    predeclare_marked_type_value_aliases();
    assert_compiled_result(7);
}

static void test_definition_type_value_alias_does_not_escape_to_root(void) {
    SZrAstNodeArray *statements = parse_aliases("let Vector = int[];");
    SZrString *name = statements->nodes[0]->data.variableDeclaration.pattern->data.identifier.name;

    set_alias_context(statements->nodes[0], g_defSite);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(compiler_find_type_value_alias(&g_compiler, name, g_defSite));
    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, name, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, name, g_callSite));
}

static void test_later_definition_type_value_alias_keeps_root_annotation(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let Vector = int[]; let Vector = string[];"
            "let values: Vector = [7]; return values[0];");

    set_alias_context(statements->nodes[1], g_defSite);
    assert_compiled_result(7);
}

static void test_call_site_type_value_alias_resolves_from_root(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "let Vector = int[]; let values: Vector = [7]; return values[0];");
    SZrType *type = statements->nodes[1]->data.variableDeclaration.typeInfo;

    set_alias_context(statements->nodes[0], g_callSite);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_NULL(type->name);
    type->name->data.identifier.syntaxContext = g_callSite;
    assert_compiled_result(7);
}

static SZrAstNode *predeclare_finally_call(SZrSyntaxContext context) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(): int { return 7; } try { work(); } finally { }");
    SZrAstNode *tryNode;
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    tryNode = statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryNode->type);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.block);
    statements->nodes[0]->data.functionDeclaration.name->syntaxContext = context;
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunctionWithContext(
            g_compiler.typeEnv, statements->nodes[0]->data.functionDeclaration.name->name,
            context, &functionInfo));
    TEST_ASSERT_NOT_NULL(functionInfo);
    return tryNode;
}

static SZrAstNode *finally_protected_call(SZrAstNode *tryNode) {
    SZrAstNode *block = tryNode->data.tryCatchFinallyStatement.block;
    SZrAstNode *statement;

    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, block->type);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    TEST_ASSERT_EQUAL_UINT(1U, block->data.block.body->count);
    statement = block->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    return statement->data.expressionStatement.expr;
}

static void test_finally_rejects_function_from_another_definition_context(void) {
    SZrAstNode *tryNode = predeclare_finally_call(ZrParser_SyntaxContext_Root());

    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_finally_is_supported(&g_compiler, tryNode));
    mark_call_target(finally_protected_call(tryNode), g_defSite);
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_finally_is_supported(&g_compiler, tryNode));
}

static void test_finally_resolves_definition_site_function_call(void) {
    SZrAstNode *tryNode = predeclare_finally_call(g_defSite);
    SZrAstNode *call = finally_protected_call(tryNode);

    mark_call_target(call, g_defSite);
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_finally_is_supported(&g_compiler, tryNode));
}

static void test_finally_marked_assignment_call_executes_matching_function(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "fn work(): int { return 7; } fn work(): int { return 11; }"
            "var result: int = 0; try { result = work(); } finally { result += 1; }"
            "return result;");
    SZrAstNode *tryNode = statements->nodes[3];
    SZrAstNode *block;
    SZrAstNode *assignment;

    statements->nodes[0]->data.functionDeclaration.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryNode->type);
    block = tryNode->data.tryCatchFinallyStatement.block;
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, block->type);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    TEST_ASSERT_EQUAL_UINT(1U, block->data.block.body->count);
    assignment = block->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, assignment->type);
    assignment = assignment->data.expressionStatement.expr;
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, assignment->type);
    mark_call_target(assignment->data.assignmentExpression.right, g_defSite);
    assert_compiled_result(8);
}

static SZrAstNode *predeclare_catch(const TZrChar *body) {
    SZrAstNodeArray *statements = parse_aliases(body);
    SZrAstNode *tryNode;

    TEST_ASSERT_EQUAL_UINT(2U, statements->count);
    tryNode = statements->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryNode->type);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    TEST_ASSERT_EQUAL_UINT(1U, tryNode->data.tryCatchFinallyStatement.catchClauses->count);
    ZrParser_Compiler_PredeclareFunctionBindings(&g_compiler, statements);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
    return tryNode;
}

static SZrAstNode *first_catch_clause(SZrAstNode *tryNode) {
    SZrAstNode *clause = tryNode->data.tryCatchFinallyStatement.catchClauses->nodes[0];

    TEST_ASSERT_NOT_NULL(clause);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CATCH_CLAUSE, clause->type);
    return clause;
}

static SZrIdentifier *simple_expression_identifier(SZrAstNode *expression) {
    TEST_ASSERT_NOT_NULL(expression);
    if (expression->type == ZR_AST_PRIMARY_EXPRESSION) {
        TEST_ASSERT_TRUE(expression->data.primaryExpression.members == ZR_NULL ||
                         expression->data.primaryExpression.members->count == 0U);
        expression = expression->data.primaryExpression.property;
    }
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expression->type);
    return &expression->data.identifier;
}

static void test_catch_parameter_read_requires_matching_context(void) {
    SZrAstNode *tryNode = predeclare_catch(
            "fn work(): int { throw 7; } try { work(); } catch(error) { error; }");
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *read = clause->data.catchClause.block->data.block.body->nodes[0];

    simple_expression_identifier(read->data.expressionStatement.expr)->syntaxContext = g_defSite;
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_catch_return_requires_matching_context_for_termination(void) {
    SZrAstNode *tryNode = predeclare_catch(
            "fn work(): int { throw 7; } try { work(); } catch(error) { return error; }");
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *result = clause->data.catchClause.block->data.block.body->nodes[0];

    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_handler_terminates(tryNode, 0U));
    simple_expression_identifier(result->data.returnStatement.expr)->syntaxContext = g_defSite;
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_handler_terminates(tryNode, 0U));
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static SZrAstNode *predeclare_catch_alias(void) {
    return predeclare_catch(
            "fn work(): int { throw 7; } try { work(); } catch(error) { var value = error; value; }");
}

static void test_catch_alias_initializer_requires_parameter_context(void) {
    SZrAstNode *tryNode = predeclare_catch_alias();
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *alias = clause->data.catchClause.block->data.block.body->nodes[0];

    simple_expression_identifier(alias->data.variableDeclaration.value)->syntaxContext = g_defSite;
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_catch_alias_read_requires_destination_context(void) {
    SZrAstNode *tryNode = predeclare_catch_alias();
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *alias = clause->data.catchClause.block->data.block.body->nodes[0];

    set_alias_context(alias, g_defSite);
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_root_variable_does_not_hide_definition_catch_alias(void) {
    SZrAstNode *tryNode = predeclare_catch_alias();
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *parameter = clause->data.catchClause.pattern->nodes[0];
    SZrAstNode *alias = clause->data.catchClause.block->data.block.body->nodes[0];

    parameter->data.parameter.name->syntaxContext = g_defSite;
    simple_expression_identifier(alias->data.variableDeclaration.value)->syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableWithContext(
            g_state, g_compiler.typeEnv, parameter->data.parameter.name->name,
            &g_inferredType, ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_matching_definition_variable_keeps_catch_alias_fallback(void) {
    SZrAstNode *tryNode = predeclare_catch_alias();
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *parameter = clause->data.catchClause.pattern->nodes[0];
    SZrAstNode *alias = clause->data.catchClause.block->data.block.body->nodes[0];

    parameter->data.parameter.name->syntaxContext = g_defSite;
    simple_expression_identifier(alias->data.variableDeclaration.value)->syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableWithContext(
            g_state, g_compiler.typeEnv, parameter->data.parameter.name->name,
            &g_inferredType, g_defSite));
    TEST_ASSERT_FALSE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_definition_catch_alias_retains_independent_destination_context(void) {
    SZrAstNode *tryNode = predeclare_catch_alias();
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *parameter = clause->data.catchClause.pattern->nodes[0];
    SZrAstNode *alias = clause->data.catchClause.block->data.block.body->nodes[0];

    parameter->data.parameter.name->syntaxContext = g_defSite;
    simple_expression_identifier(alias->data.variableDeclaration.value)->syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_definition_catch_parameter_read_is_supported(void) {
    SZrAstNode *tryNode = predeclare_catch(
            "fn work(): int { throw 7; } try { work(); } catch(error) { error; }");
    SZrAstNode *clause = first_catch_clause(tryNode);
    SZrAstNode *parameter = clause->data.catchClause.pattern->nodes[0];
    SZrAstNode *read = clause->data.catchClause.block->data.block.body->nodes[0];

    parameter->data.parameter.name->syntaxContext = g_defSite;
    simple_expression_identifier(read->data.expressionStatement.expr)->syntaxContext = g_defSite;
    TEST_ASSERT_TRUE(compiler_semantic_cfg_try_catch_is_supported(&g_compiler, tryNode));
}

static void test_marked_catch_binding_executes_beside_root_shadow(void) {
    SZrAstNodeArray *statements = parse_aliases(
            "var error: int = 99; fn work(): int { throw 7; }"
            "try { work(); } catch(error) { return error.exception; }");
    SZrAstNode *clause = first_catch_clause(statements->nodes[2]);
    SZrAstNode *parameter = clause->data.catchClause.pattern->nodes[0];
    SZrAstNode *result = clause->data.catchClause.block->data.block.body->nodes[0];

    parameter->data.parameter.name->syntaxContext = g_defSite;
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, result->type);
    mark_call_target(result->data.returnStatement.expr, g_defSite);
    assert_compiled_result(7);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_module_alias_predeclaration_separates_definition_and_root);
    RUN_TEST(test_module_alias_predeclaration_keeps_root_when_definition_is_last);
    RUN_TEST(test_module_alias_call_site_predeclaration_resolves_from_root);
    RUN_TEST(test_root_function_lookup_excludes_definition_site_candidate);
    RUN_TEST(test_root_overload_set_excludes_definition_site_candidate);
    RUN_TEST(test_equal_function_signatures_remain_distinct_across_contexts);
    RUN_TEST(test_definition_site_value_does_not_hide_outer_root_function);
    RUN_TEST(test_call_site_value_hides_outer_root_function);
    RUN_TEST(test_root_callable_alias_reads_definition_site_source);
    RUN_TEST(test_definition_site_function_identifier_infers_callable_type);
    RUN_TEST(test_definition_site_function_call_selects_marked_overload);
    RUN_TEST(test_marked_equal_signature_functions_execute_separately);
    RUN_TEST(test_marked_named_calls_use_their_declarations);
    RUN_TEST(test_marked_callable_alias_and_lambda_execute_separately);
    RUN_TEST(test_marked_typed_callable_parameter_executes);
    RUN_TEST(test_marked_typed_callable_parameter_rejects_wrong_argument);
    RUN_TEST(test_marked_typed_callable_result_rejects_wrong_argument);
    RUN_TEST(test_root_typed_callable_result_rejects_wrong_argument);
    RUN_TEST(test_marked_typed_callable_result_keeps_typed_contract);
    RUN_TEST(test_marked_lambda_parameter_infers_callable_return);
    RUN_TEST(test_marked_local_metadata_keeps_binding_types_separate);
    RUN_TEST(test_type_value_predeclaration_keeps_definition_and_root);
    RUN_TEST(test_marked_type_value_annotation_selects_definition_alias);
    RUN_TEST(test_marked_type_value_aliases_compile_and_execute);
    RUN_TEST(test_definition_type_value_alias_does_not_escape_to_root);
    RUN_TEST(test_later_definition_type_value_alias_keeps_root_annotation);
    RUN_TEST(test_call_site_type_value_alias_resolves_from_root);
    RUN_TEST(test_finally_rejects_function_from_another_definition_context);
    RUN_TEST(test_finally_resolves_definition_site_function_call);
    RUN_TEST(test_finally_marked_assignment_call_executes_matching_function);
    RUN_TEST(test_catch_parameter_read_requires_matching_context);
    RUN_TEST(test_catch_return_requires_matching_context_for_termination);
    RUN_TEST(test_catch_alias_initializer_requires_parameter_context);
    RUN_TEST(test_catch_alias_read_requires_destination_context);
    RUN_TEST(test_root_variable_does_not_hide_definition_catch_alias);
    RUN_TEST(test_matching_definition_variable_keeps_catch_alias_fallback);
    RUN_TEST(test_definition_catch_alias_retains_independent_destination_context);
    RUN_TEST(test_definition_catch_parameter_read_is_supported);
    RUN_TEST(test_marked_catch_binding_executes_beside_root_shadow);
    return UNITY_END();
}
