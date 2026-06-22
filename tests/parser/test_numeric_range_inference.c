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

static void register_int64_range_variable(SZrCompilerState *cs,
                                           const char *name,
                                           TZrInt64 minValue,
                                           TZrInt64 maxValue) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    type.hasRangeConstraint = ZR_TRUE;
    type.minValue = minValue;
    type.maxValue = maxValue;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
        &type));
    ZrParser_InferredType_Free(g_state, &type);
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

static void test_unary_minus_numeric_fact_inverts_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "-seed;";

    sourceName = ZrCore_String_Create(g_state,
                                      "unary_minus_interval_numeric_fact_test.zr",
                                      strlen("unary_minus_interval_numeric_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);

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

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(-4, result.minValue);
    TEST_ASSERT_EQUAL_INT64(-2, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_RANGE, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(-4, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(-2, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_unary_minus_numeric_fact_clears_overflowing_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "-seed;";

    sourceName = ZrCore_String_Create(g_state,
                                      "unary_minus_interval_overflow_test.zr",
                                      strlen("unary_minus_interval_overflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &seedType, ZR_VALUE_TYPE_INT64);
    seedType.hasRangeConstraint = ZR_TRUE;
    seedType.minValue = ZR_TYPE_RANGE_INT64_MIN;
    seedType.maxValue = ZR_TYPE_RANGE_INT64_MIN;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        g_state,
        cs->typeEnv,
        ZrCore_String_Create(g_state, "seed", 4),
        &seedType));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_FALSE(result.hasRangeConstraint);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_FALSE(numericFact->hasRange);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_division_numeric_fact_records_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed / 2;";

    sourceName = ZrCore_String_Create(g_state,
                                      "integer_division_interval_numeric_fact_test.zr",
                                      strlen("integer_division_interval_numeric_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING("/", expr->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 8, 12);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(4, result.minValue);
    TEST_ASSERT_EQUAL_INT64(6, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->maxValue);
    TEST_ASSERT_TRUE(numericFact->hasUnsignedRange);
    TEST_ASSERT_EQUAL_UINT64(4, numericFact->minUnsignedValue);
    TEST_ASSERT_EQUAL_UINT64(6, numericFact->maxUnsignedValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_division_numeric_fact_clears_zero_divisor_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed / denom;";

    sourceName = ZrCore_String_Create(g_state,
                                      "integer_division_zero_interval_numeric_fact_test.zr",
                                      strlen("integer_division_zero_interval_numeric_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING("/", expr->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 8, 12);
    register_int64_range_variable(cs, "denom", -1, 1);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_FALSE(result.hasRangeConstraint);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_FALSE(numericFact->hasRange);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_modulo_numeric_fact_records_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed % 5;";

    sourceName = ZrCore_String_Create(g_state,
                                      "integer_modulo_interval_numeric_fact_test.zr",
                                      strlen("integer_modulo_interval_numeric_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING("%", expr->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 8, 12);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(0, result.minValue);
    TEST_ASSERT_EQUAL_INT64(4, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(0, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->maxValue);
    TEST_ASSERT_TRUE(numericFact->hasUnsignedRange);
    TEST_ASSERT_EQUAL_UINT64(0, numericFact->minUnsignedValue);
    TEST_ASSERT_EQUAL_UINT64(4, numericFact->maxUnsignedValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_modulo_numeric_fact_clears_zero_divisor_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed % denom;";

    sourceName = ZrCore_String_Create(g_state,
                                      "integer_modulo_zero_interval_numeric_fact_test.zr",
                                      strlen("integer_modulo_zero_interval_numeric_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING("%", expr->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 8, 12);
    register_int64_range_variable(cs, "denom", -1, 1);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_FALSE(result.hasRangeConstraint);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_FALSE(numericFact->hasRange);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_integer_modulo_numeric_fact_clears_int64_min_negative_one_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source = "seed % denom;";

    sourceName = ZrCore_String_Create(g_state,
                                      "integer_modulo_int64_min_negative_one_test.zr",
                                      strlen("integer_modulo_int64_min_negative_one_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING("%", expr->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", ZR_TYPE_RANGE_INT64_MIN, ZR_TYPE_RANGE_INT64_MIN);
    register_int64_range_variable(cs, "denom", -1, -1);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_FALSE(result.hasRangeConstraint);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_FALSE(numericFact->hasRange);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void assert_interval_comparison_logical_fact(const char *source,
                                                    const char *sourceNameText,
                                                    const char *op,
                                                    EZrSemanticLogicalFactKind expectedKind,
                                                    TZrBool expectedValue) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticLogicalFact *logicalFact;

    sourceName = ZrCore_String_Create(g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);
    TEST_ASSERT_EQUAL_STRING(op, expr->data.binaryExpression.op.op);

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
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, expressionFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(expectedKind, logicalFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, logicalFact->exactness);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    if (expectedValue) {
        TEST_ASSERT_TRUE(logicalFact->knownValue);
    } else {
        TEST_ASSERT_FALSE(logicalFact->knownValue);
    }
    TEST_ASSERT_EQUAL_PTR(expr, logicalFact->node);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_interval_comparison_records_always_true_logical_fact(void) {
    assert_interval_comparison_logical_fact(
        "seed < 10;",
        "interval_comparison_true_logical_fact_test.zr",
        "<",
        ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE,
        ZR_TRUE);
}

static void test_interval_comparison_records_always_false_logical_fact(void) {
    assert_interval_comparison_logical_fact(
        "seed > 10;",
        "interval_comparison_false_logical_fact_test.zr",
        ">",
        ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
        ZR_FALSE);
}

static void test_overlapping_interval_comparison_stays_unknown(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType seedType;
    SZrInferredType result;
    const SZrSemanticLogicalFact *logicalFact;
    const char *source = "seed < 3;";

    sourceName = ZrCore_String_Create(g_state,
                                      "interval_comparison_unknown_logical_fact_test.zr",
                                      strlen("interval_comparison_unknown_logical_fact_test.zr"));
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
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NULL(logicalFact);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &seedType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unary_minus_numeric_fact_inverts_integer_interval_range);
    RUN_TEST(test_unary_minus_numeric_fact_clears_overflowing_integer_interval_range);
    RUN_TEST(test_integer_division_numeric_fact_records_interval_range);
    RUN_TEST(test_integer_division_numeric_fact_clears_zero_divisor_interval_range);
    RUN_TEST(test_integer_modulo_numeric_fact_records_interval_range);
    RUN_TEST(test_integer_modulo_numeric_fact_clears_zero_divisor_interval_range);
    RUN_TEST(test_integer_modulo_numeric_fact_clears_int64_min_negative_one_range);
    RUN_TEST(test_interval_comparison_records_always_true_logical_fact);
    RUN_TEST(test_interval_comparison_records_always_false_logical_fact);
    RUN_TEST(test_overlapping_interval_comparison_stays_unknown);
    return UNITY_END();
}
