#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;

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

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static SZrAstNode *first_declaration_initializer(SZrAstNode *ast) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }

    return statement->data.variableDeclaration.value;
}

static SZrAstNode *first_expression_statement_expression(SZrAstNode *ast) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static SZrAstNode *expression_statement_expression_at(SZrAstNode *ast, TZrSize statementIndex) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= statementIndex) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[statementIndex];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static void test_function_call_inference_records_expression_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrInferredType intType;
    SZrArray parameterTypes;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *callExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *callFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "var result = pick(42);";

    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &parameterTypes, &intType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "pick", 4),
        &intType,
        &parameterTypes));

    sourceName = ZrCore_String_Create(g_state, "call_expression_fact_test.zr", 28);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    callExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(callExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, callExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, callExpr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    callFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, callExpr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, callExpr);

    TEST_ASSERT_NOT_NULL(callFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_MEMBER, callFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, callFact->exactness);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, callFact->inferredType.baseType);
    TEST_ASSERT_FALSE(callFact->hasConstant);
    TEST_ASSERT_NULL(numericFact);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    ZrCore_Array_Free(g_state, &parameterTypes);
    ZrParser_InferredType_Free(g_state, &intType);
    destroy_compiler_state(cs);
}

static void test_function_call_expression_fact_records_call_target_payload(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrInferredType intType;
    SZrArray parameterTypes;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *callExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *callFact;
    const char *source = "var result = pick(42);";

    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &parameterTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &parameterTypes, &intType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "pick", 4),
        &intType,
        &parameterTypes));

    sourceName = ZrCore_String_Create(g_state, "call_payload_expression_fact_test.zr", 36);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    callExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(callExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, callExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, callExpr, &result));
    callFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, callExpr);

    TEST_ASSERT_NOT_NULL(callFact);
    TEST_ASSERT_TRUE(callFact->hasCallInfo);
    TEST_ASSERT_NOT_NULL(callFact->callTargetName);
    TEST_ASSERT_EQUAL_STRING("pick", ZrCore_String_GetNativeString(callFact->callTargetName));
    TEST_ASSERT_EQUAL_UINT64(13, callFact->callTargetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(17, callFact->callTargetRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(1, callFact->argumentCount);
    TEST_ASSERT_FALSE(callFact->hasNamedArguments);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    ZrCore_Array_Free(g_state, &parameterTypes);
    ZrParser_InferredType_Free(g_state, &intType);
    destroy_compiler_state(cs);
}

static void test_member_expression_fact_records_member_payload(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticExpressionFact *memberFact;
    const char *source = "seed.value;";

    sourceName = ZrCore_String_Create(g_state, "member_payload_expression_fact_test.zr", 38);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    memberFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(memberFact);
    TEST_ASSERT_TRUE(memberFact->hasMemberInfo);
    TEST_ASSERT_NOT_NULL(memberFact->memberName);
    TEST_ASSERT_EQUAL_STRING("value", ZrCore_String_GetNativeString(memberFact->memberName));
    TEST_ASSERT_EQUAL_UINT64(5, memberFact->memberRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(10, memberFact->memberRange.end.offset);
    TEST_ASSERT_FALSE(memberFact->memberIsComputed);
    TEST_ASSERT_FALSE(memberFact->hasCallInfo);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_literal_numeric_fact_records_exact_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "42;";

    sourceName = ZrCore_String_Create(g_state, "literal_numeric_fact_test.zr", 28);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_UINT64(0, expr->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(2, expr->location.end.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.start.offset, numericFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.end.offset, numericFact->range.end.offset);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_LITERAL, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(42, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(42, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_identifier_expression_fact_records_registered_type(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed;";

    sourceName = ZrCore_String_Create(g_state,
                                      "identifier_expression_fact_test.zr",
                                      strlen("identifier_expression_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    seedType.hasRangeConstraint = ZR_TRUE;
    seedType.minValue = 7;
    seedType.maxValue = 7;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, expressionFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_RANGE, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(7, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(7, numericFact->maxValue);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_integer_numeric_fact_records_exact_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "1 + 2;";

    sourceName = ZrCore_String_Create(g_state, "binary_numeric_fact_test.zr", 27);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, expressionFact->exactness);
    TEST_ASSERT_TRUE(expressionFact->hasConstant);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_KIND_INT64, expressionFact->valueKind);
    TEST_ASSERT_EQUAL_INT64(3, expressionFact->constantValue.int64Value);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_UINT64(0, expr->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(5, expr->location.end.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.start.offset, numericFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.end.offset, numericFact->range.end.offset);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_unary_integer_numeric_fact_records_exact_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "-42;";

    sourceName = ZrCore_String_Create(g_state, "unary_numeric_fact_test.zr", 26);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_UINT64(0, expr->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(3, expr->location.end.offset);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_UNARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, expressionFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_RANGE, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(-42, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(-42, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_unary_logical_not_records_exact_logical_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticLogicalFact *logicalFact;
    const char *source = "!true;";

    sourceName = ZrCore_String_Create(g_state,
                                      "unary_logical_fact_test.zr",
                                      strlen("unary_logical_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_UINT64(0, expr->location.start.offset);
    TEST_ASSERT_EQUAL_UINT64(5, expr->location.end.offset);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_UNARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, expressionFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE, logicalFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, logicalFact->exactness);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    TEST_ASSERT_FALSE(logicalFact->knownValue);
    TEST_ASSERT_EQUAL_PTR(expr->data.unaryExpression.argument, logicalFact->relatedNode);
    TEST_ASSERT_EQUAL_UINT64(expr->location.start.offset, logicalFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.end.offset, logicalFact->range.end.offset);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_logical_expression_range_covers_operator_gap(void) {
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    const char *source = "true || false;";

    sourceName = ZrCore_String_Create(g_state, "logical_expression_range_test.zr", 32);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_UINT64(0, expr->location.start.offset);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(5, expr->location.start.offset);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(6, expr->location.end.offset);
    TEST_ASSERT_EQUAL_UINT64(13, expr->location.end.offset);

    ZrParser_Ast_Free(g_state, ast);
}

static void test_logical_expression_inference_records_bool_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const char *source = "true || false;";

    sourceName = ZrCore_String_Create(g_state, "logical_expression_fact_test.zr", 31);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_KIND_BOOL, expressionFact->valueKind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, expressionFact->inferredType.baseType);
    TEST_ASSERT_EQUAL_UINT64(expr->location.start.offset, expressionFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.end.offset, expressionFact->range.end.offset);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_logical_expression_inference_records_short_circuit_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrAstNode *right;
    SZrInferredType result;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticReachabilityFact *reachabilityFact;
    const char *source = "true || false;";

    sourceName = ZrCore_String_Create(g_state, "logical_short_circuit_fact_test.zr", 34);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, expr->type);
    right = expr->data.logicalExpression.right;
    TEST_ASSERT_NOT_NULL(right);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);
    reachabilityFact = ZrParser_SemanticFacts_FindReachabilityAtPosition(cs->semanticContext,
                                                                         right->location);

    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT, logicalFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, logicalFact->exactness);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    TEST_ASSERT_TRUE(logicalFact->knownValue);
    TEST_ASSERT_NOT_NULL(logicalFact->relatedNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BOOLEAN_LITERAL, logicalFact->relatedNode->type);
    TEST_ASSERT_NOT_NULL(reachabilityFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, reachabilityFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT, reachabilityFact->cause);
    TEST_ASSERT_EQUAL_PTR(expr->data.logicalExpression.left, reachabilityFact->causeNode);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_integer_numeric_fact_marks_overflow(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "9223372036854775807 + 1;";

    sourceName = ZrCore_String_Create(g_state, "binary_overflow_fact_test.zr", 28);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->mayOverflow);
    TEST_ASSERT_FALSE(numericFact->hasRange);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_integer_multiply_numeric_fact_records_exact_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "3 * 4;";

    sourceName = ZrCore_String_Create(g_state, "binary_multiply_fact_test.zr", 29);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(12, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(12, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_integer_multiply_numeric_fact_marks_overflow(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "3037000500 * 3037000500;";

    sourceName = ZrCore_String_Create(g_state, "binary_multiply_overflow_fact_test.zr", 38);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->mayOverflow);
    TEST_ASSERT_FALSE(numericFact->hasRange);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_assignment_expression_fact_records_assignment_kind(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticExpressionFact *assignmentFact;
    const SZrSemanticExpressionFact *leftFact;
    const SZrSemanticExpressionFact *rightFact;
    const char *source = "seed = 3;";

    sourceName = ZrCore_String_Create(g_state,
                                      "assignment_expression_fact_test.zr",
                                      strlen("assignment_expression_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ASSIGNMENT_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    assignmentFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    leftFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext,
                                                           expr->data.assignmentExpression.left);
    rightFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext,
                                                            expr->data.assignmentExpression.right);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_NOT_NULL(assignmentFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT, assignmentFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, assignmentFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(leftFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER, leftFact->kind);
    TEST_ASSERT_NOT_NULL(rightFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_LITERAL, rightFact->kind);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_float_literal_numeric_fact_records_exact_double_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "1.5;";

    sourceName = ZrCore_String_Create(g_state, "float_literal_numeric_fact_test.zr", 34);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FLOAT_LITERAL, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.baseType);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_LITERAL, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, numericFact->minDoubleValue);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, numericFact->maxDoubleValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_float_numeric_fact_records_exact_double_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "1.5 + 2.25;";

    sourceName = ZrCore_String_Create(g_state, "float_binary_numeric_fact_test.zr", 33);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.baseType);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_DOUBLE(3.75, numericFact->minDoubleValue);
    TEST_ASSERT_EQUAL_DOUBLE(3.75, numericFact->maxDoubleValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_lambda_expression_fact_records_callable_type(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *lambdaExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *lambdaFact;
    const char *source = "var mapper = (x:int)->{ return x; };";

    sourceName = ZrCore_String_Create(g_state,
                                      "lambda_expression_fact_test.zr",
                                      strlen("lambda_expression_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    lambdaExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(lambdaExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambdaExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, lambdaExpr, &result));
    lambdaFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, lambdaExpr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, result.baseType);
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("%func(int)->int", ZrCore_String_GetNativeString(result.typeName));
    TEST_ASSERT_NOT_NULL(lambdaFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA, lambdaFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, lambdaFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(lambdaFact->inferredType.typeName);
    TEST_ASSERT_EQUAL_STRING("%func(int)->int", ZrCore_String_GetNativeString(lambdaFact->inferredType.typeName));

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_lambda_constant_true_loop_return_records_callable_type_and_body_facts(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *lambdaExpr;
    SZrAstNode *lambdaBlock;
    SZrAstNode *whileLoop;
    SZrAstNode *whileBlock;
    SZrAstNode *returnStatement;
    SZrAstNode *returnExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *lambdaFact;
    const SZrSemanticExpressionFact *returnExprFact;
    const SZrSemanticNumericFact *returnNumericFact;
    const char *source = "var worker = ()->{ while (true) { return 1 + 2; } };";

    sourceName = ZrCore_String_Create(g_state,
                                      "lambda_loop_return_fact_test.zr",
                                      strlen("lambda_loop_return_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    lambdaExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(lambdaExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambdaExpr->type);
    lambdaBlock = lambdaExpr->data.lambdaExpression.block;
    TEST_ASSERT_NOT_NULL(lambdaBlock);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, lambdaBlock->type);
    TEST_ASSERT_NOT_NULL(lambdaBlock->data.block.body);
    TEST_ASSERT_EQUAL_INT(1, (int)lambdaBlock->data.block.body->count);
    whileLoop = lambdaBlock->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(whileLoop);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileLoop->type);
    whileBlock = whileLoop->data.whileLoop.block;
    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, whileBlock->type);
    TEST_ASSERT_NOT_NULL(whileBlock->data.block.body);
    TEST_ASSERT_EQUAL_INT(1, (int)whileBlock->data.block.body->count);
    returnStatement = whileBlock->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(returnStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnStatement->type);
    returnExpr = returnStatement->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(returnExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, returnExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, lambdaExpr, &result));
    lambdaFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, lambdaExpr);
    returnExprFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, returnExpr);
    returnNumericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, returnExpr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, result.baseType);
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("%func()->int", ZrCore_String_GetNativeString(result.typeName));
    TEST_ASSERT_NOT_NULL(lambdaFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA, lambdaFact->kind);
    TEST_ASSERT_NOT_NULL(lambdaFact->inferredType.typeName);
    TEST_ASSERT_EQUAL_STRING("%func()->int", ZrCore_String_GetNativeString(lambdaFact->inferredType.typeName));
    TEST_ASSERT_NOT_NULL(returnExprFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, returnExprFact->kind);
    TEST_ASSERT_TRUE(returnExprFact->hasConstant);
    TEST_ASSERT_EQUAL_INT64(3, returnExprFact->constantValue.int64Value);
    TEST_ASSERT_NOT_NULL(returnNumericFact);
    TEST_ASSERT_TRUE(returnNumericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, returnNumericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(3, returnNumericFact->maxValue);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_ownership_builtin_expression_fact_records_builtin_kind(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrAstNode *targetExpr;
    SZrInferredType ownerType;
    SZrInferredType result;
    const SZrSemanticExpressionFact *builtinFact;
    const SZrSemanticExpressionFact *targetFact;
    const SZrSemanticOwnershipFact *ownershipFact;
    const char *source = "%borrow(owner);";

    sourceName = ZrCore_String_Create(g_state,
                                      "ownership_builtin_expression_fact_test.zr",
                                      strlen("ownership_builtin_expression_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expr->type);
    targetExpr = expr->data.constructExpression.target;
    TEST_ASSERT_NOT_NULL(targetExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, targetExpr->type);

    ZrParser_InferredType_Init(g_state, &ownerType, ZR_VALUE_TYPE_INT64);
    ownerType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "owner", 5),
        &ownerType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    builtinFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    targetFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, targetExpr);
    ownershipFact = ZrParser_SemanticFacts_FindOwnershipByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED, result.ownershipQualifier);
    TEST_ASSERT_NOT_NULL(builtinFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN, builtinFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED, builtinFact->inferredType.ownershipQualifier);
    TEST_ASSERT_NOT_NULL(targetFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER, targetFact->kind);
    TEST_ASSERT_NOT_NULL(ownershipFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_OWNERSHIP_FACT_BORROW, ownershipFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED, ownershipFact->qualifier);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &ownerType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_variable_numeric_fact_propagates_exact_integer_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *initializer;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "var seed = 2;\nseed + 3;";

    sourceName = ZrCore_String_Create(g_state, "variable_range_numeric_fact_test.zr", 35);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    initializer = first_declaration_initializer(ast);
    expr = expression_statement_expression_at(ast, 1);

    TEST_ASSERT_NOT_NULL(initializer);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, initializer->type);
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, initializer, &seedType));
    TEST_ASSERT_TRUE(seedType.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, seedType.minValue);
    TEST_ASSERT_EQUAL_INT64(2, seedType.maxValue);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(5, result.maxValue);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_variable_numeric_fact_propagates_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed + 3;";

    sourceName = ZrCore_String_Create(g_state, "variable_interval_numeric_fact_test.zr", 38);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    seedType.hasRangeConstraint = ZR_TRUE;
    seedType.minValue = 2;
    seedType.maxValue = 4;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(7, result.maxValue);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(7, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_identifier_unsigned_numeric_fact_records_unsigned_primitive_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed;";

    sourceName = ZrCore_String_Create(g_state, "identifier_unsigned_numeric_fact_test.zr", 40);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_UINT64);
    seedType.hasRangeConstraint = ZR_TRUE;
    seedType.minValue = 0;
    seedType.maxValue = ZR_TYPE_RANGE_UINT64_MAX;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, result.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_RANGE, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_TRUE(numericFact->hasUnsignedRange);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)0, numericFact->minUnsignedValue);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)UINT64_MAX, numericFact->maxUnsignedValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_binary_unsigned_variable_numeric_fact_keeps_unsigned_range_payload(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed + 3;";

    sourceName = ZrCore_String_Create(g_state, "unsigned_binary_numeric_fact_test.zr", 36);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_UINT8);
    seedType.hasRangeConstraint = ZR_TRUE;
    seedType.minValue = 0;
    seedType.maxValue = ZR_TYPE_RANGE_UINT8_MAX;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(3, result.minValue);
    TEST_ASSERT_EQUAL_INT64(258, result.maxValue);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(258, numericFact->maxValue);
    TEST_ASSERT_TRUE(numericFact->hasUnsignedRange);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)3, numericFact->minUnsignedValue);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)258, numericFact->maxUnsignedValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_object_literal_value_expression_records_nested_facts(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *objectExpr;
    SZrAstNode *propertyNode;
    SZrAstNode *valueExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *objectFact;
    const SZrSemanticExpressionFact *valueFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "var obj = {a: 1 + 2};";

    sourceName = ZrCore_String_Create(g_state,
                                      "object_literal_value_fact_test.zr",
                                      strlen("object_literal_value_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    objectExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(objectExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, objectExpr->type);
    TEST_ASSERT_NOT_NULL(objectExpr->data.objectLiteral.properties);
    TEST_ASSERT_EQUAL_UINT64(1, objectExpr->data.objectLiteral.properties->count);
    propertyNode = objectExpr->data.objectLiteral.properties->nodes[0];
    TEST_ASSERT_NOT_NULL(propertyNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_KEY_VALUE_PAIR, propertyNode->type);
    TEST_ASSERT_FALSE(propertyNode->data.keyValuePair.keyIsComputed);
    valueExpr = propertyNode->data.keyValuePair.value;
    TEST_ASSERT_NOT_NULL(valueExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, valueExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, objectExpr, &result));
    objectFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, objectExpr);
    valueFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, valueExpr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, valueExpr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
    TEST_ASSERT_NOT_NULL(objectFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_OBJECT, objectFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, objectFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(valueFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, valueFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, valueFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_object_literal_computed_key_expression_records_nested_facts(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *objectExpr;
    SZrAstNode *propertyNode;
    SZrAstNode *keyExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *objectFact;
    const SZrSemanticExpressionFact *keyFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "var obj = {[1 + 2]: 4};";

    sourceName = ZrCore_String_Create(g_state,
                                      "object_literal_computed_key_fact_test.zr",
                                      strlen("object_literal_computed_key_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    objectExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(objectExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_OBJECT_LITERAL, objectExpr->type);
    TEST_ASSERT_NOT_NULL(objectExpr->data.objectLiteral.properties);
    TEST_ASSERT_EQUAL_UINT64(1, objectExpr->data.objectLiteral.properties->count);
    propertyNode = objectExpr->data.objectLiteral.properties->nodes[0];
    TEST_ASSERT_NOT_NULL(propertyNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_KEY_VALUE_PAIR, propertyNode->type);
    TEST_ASSERT_TRUE(propertyNode->data.keyValuePair.keyIsComputed);
    keyExpr = propertyNode->data.keyValuePair.key;
    TEST_ASSERT_NOT_NULL(keyExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, keyExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, objectExpr, &result));
    objectFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, objectExpr);
    keyFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, keyExpr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, keyExpr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
    TEST_ASSERT_NOT_NULL(objectFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_OBJECT, objectFact->kind);
    TEST_ASSERT_NOT_NULL(keyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, keyFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, keyFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_array_literal_element_expression_records_nested_facts(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *arrayExpr;
    SZrAstNode *elementExpr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *arrayFact;
    const SZrSemanticExpressionFact *elementFact;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "var values = [1 + 2];";

    sourceName = ZrCore_String_Create(g_state,
                                      "array_literal_element_fact_test.zr",
                                      strlen("array_literal_element_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    arrayExpr = first_declaration_initializer(ast);

    TEST_ASSERT_NOT_NULL(arrayExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_ARRAY_LITERAL, arrayExpr->type);
    TEST_ASSERT_NOT_NULL(arrayExpr->data.arrayLiteral.elements);
    TEST_ASSERT_EQUAL_UINT64(1, arrayExpr->data.arrayLiteral.elements->count);
    elementExpr = arrayExpr->data.arrayLiteral.elements->nodes[0];
    TEST_ASSERT_NOT_NULL(elementExpr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, elementExpr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, arrayExpr, &result));
    arrayFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, arrayExpr);
    elementFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, elementExpr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, elementExpr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, result.baseType);
    TEST_ASSERT_NOT_NULL(arrayFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_ARRAY, arrayFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, arrayFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(elementFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, elementFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_constant_conditional_expression_records_selected_range_and_skipped_branch_fact(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrAstNode *alternate;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticReachabilityFact *reachabilityFact;
    const char *source = "true ? 1 : 2;";

    sourceName = ZrCore_String_Create(g_state, "conditional_branch_fact_test.zr", 31);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONDITIONAL_EXPRESSION, expr->type);
    alternate = expr->data.conditionalExpression.alternate;
    TEST_ASSERT_NOT_NULL(alternate);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, alternate->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext,
                                                           expr->data.conditionalExpression.test);
    reachabilityFact = ZrParser_SemanticFacts_FindReachabilityAtPosition(cs->semanticContext,
                                                                         alternate->location);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(1, result.minValue);
    TEST_ASSERT_EQUAL_INT64(1, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_RANGE, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE, logicalFact->kind);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    TEST_ASSERT_TRUE(logicalFact->knownValue);
    TEST_ASSERT_EQUAL_PTR(alternate, logicalFact->relatedNode);
    TEST_ASSERT_NOT_NULL(reachabilityFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, reachabilityFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, reachabilityFact->cause);
    TEST_ASSERT_EQUAL_PTR(expr->data.conditionalExpression.test, reachabilityFact->causeNode);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_call_inference_records_expression_fact);
    RUN_TEST(test_function_call_expression_fact_records_call_target_payload);
    RUN_TEST(test_member_expression_fact_records_member_payload);
    RUN_TEST(test_integer_literal_numeric_fact_records_exact_range);
    RUN_TEST(test_identifier_expression_fact_records_registered_type);
    RUN_TEST(test_binary_integer_numeric_fact_records_exact_range);
    RUN_TEST(test_unary_integer_numeric_fact_records_exact_range);
    RUN_TEST(test_unary_logical_not_records_exact_logical_fact);
    RUN_TEST(test_logical_expression_range_covers_operator_gap);
    RUN_TEST(test_logical_expression_inference_records_bool_fact);
    RUN_TEST(test_logical_expression_inference_records_short_circuit_fact);
    RUN_TEST(test_binary_integer_numeric_fact_marks_overflow);
    RUN_TEST(test_binary_integer_multiply_numeric_fact_records_exact_range);
    RUN_TEST(test_binary_integer_multiply_numeric_fact_marks_overflow);
    RUN_TEST(test_assignment_expression_fact_records_assignment_kind);
    RUN_TEST(test_float_literal_numeric_fact_records_exact_double_range);
    RUN_TEST(test_binary_float_numeric_fact_records_exact_double_range);
    RUN_TEST(test_lambda_expression_fact_records_callable_type);
    RUN_TEST(test_lambda_constant_true_loop_return_records_callable_type_and_body_facts);
    RUN_TEST(test_ownership_builtin_expression_fact_records_builtin_kind);
    RUN_TEST(test_binary_variable_numeric_fact_propagates_exact_integer_range);
    RUN_TEST(test_binary_variable_numeric_fact_propagates_integer_interval_range);
    RUN_TEST(test_identifier_unsigned_numeric_fact_records_unsigned_primitive_range);
    RUN_TEST(test_binary_unsigned_variable_numeric_fact_keeps_unsigned_range_payload);
    RUN_TEST(test_object_literal_value_expression_records_nested_facts);
    RUN_TEST(test_object_literal_computed_key_expression_records_nested_facts);
    RUN_TEST(test_array_literal_element_expression_records_nested_facts);
    RUN_TEST(test_constant_conditional_expression_records_selected_range_and_skipped_branch_fact);
    return UNITY_END();
}
