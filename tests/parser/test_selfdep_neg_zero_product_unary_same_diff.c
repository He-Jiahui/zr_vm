#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
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

static void register_bool_variable(SZrCompilerState *cs, const char *name) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_BOOL);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &type));
    ZrParser_InferredType_Free(g_state, &type);
}

static void register_int64_range_variable(
        SZrCompilerState *cs,
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

static SZrAstNode *statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static SZrAstNode *expression_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static void assert_int64_range_result_and_fact(
        SZrCompilerState *cs,
        SZrAstNode *expression,
        SZrInferredType *result,
        TZrInt64 expectedMin,
        TZrInt64 expectedMax) {
    const SZrSemanticNumericFact *numericFact;

    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, result));
    TEST_ASSERT_TRUE(result->hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, result->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, result->maxValue);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);
}

static void run_unary_same_identifier_difference_product_case(
        const char *sourceNameText,
        const char *offsetExpression,
        const char *deltaExpression,
        TZrInt64 expectedTargetMin,
        TZrInt64 expectedTargetMax,
        TZrInt64 expectedObserverMin,
        TZrInt64 expectedObserverMax,
        TZrInt64 expectedMirrorMin,
        TZrInt64 expectedMirrorMax) {
    SZrCompilerState *cs = create_compiler_state();
    char source[1024];
    int written;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrAstNode *mirrorExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    SZrInferredType mirrorResult;

    register_bool_variable(cs, "flag");
    register_bool_variable(cs, "choose");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "mirror", 0, 0);
    register_int64_range_variable(cs, "zero", 0, 0);
    register_int64_range_variable(cs, "span", -1, 1);
    register_int64_range_variable(cs, "factor", 2, 3);
    register_int64_range_variable(cs, "step", 1, 3);
    register_int64_range_variable(cs, "bias", 1, 2);

    written = snprintf(
            source,
            sizeof(source),
            "while (flag) {\n"
            "    other = (+narrowed) - (zero - (%s));\n"
            "    mirror = other;\n"
            "    narrowed = narrowed %s;\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n"
            "mirror + 0;\n",
            offsetExpression,
            deltaExpression);
    TEST_ASSERT_TRUE(written > 0 && (size_t)written < sizeof(source));

    sourceName = ZrCore_String_Create(g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    mirrorExpression = expression_statement_expression(statement_at(ast, 3));
    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_NOT_NULL(mirrorExpression);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &mirrorResult, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(
            cs,
            targetExpression,
            &targetResult,
            expectedTargetMin,
            expectedTargetMax);
    assert_int64_range_result_and_fact(
            cs,
            observerExpression,
            &observerResult,
            expectedObserverMin,
            expectedObserverMax);
    assert_int64_range_result_and_fact(
            cs,
            mirrorExpression,
            &mirrorResult,
            expectedMirrorMin,
            expectedMirrorMax);

    ZrParser_InferredType_Free(g_state, &mirrorResult);
    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void run_upward_case(const char *sourceNameText, const char *offsetExpression) {
    run_unary_same_identifier_difference_product_case(
            sourceNameText,
            offsetExpression,
            "+ (choose ? step : step + bias)",
            5,
            ZR_TYPE_RANGE_INT64_MAX,
            0,
            ZR_TYPE_RANGE_INT64_MAX,
            0,
            ZR_TYPE_RANGE_INT64_MAX);
}

static void run_downward_case(const char *sourceNameText, const char *offsetExpression) {
    run_unary_same_identifier_difference_product_case(
            sourceNameText,
            offsetExpression,
            "- (choose ? step : step + bias)",
            ZR_TYPE_RANGE_INT64_MIN,
            5,
            ZR_TYPE_RANGE_INT64_MIN,
            5,
            ZR_TYPE_RANGE_INT64_MIN,
            5);
}

static void assert_inferred_exact_zero_expression(const char *sourceNameText, const char *expressionText) {
    SZrCompilerState *cs = create_compiler_state();
    char source[512];
    int written;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expression;
    SZrInferredType result;

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "zero", 0, 0);
    register_int64_range_variable(cs, "span", -1, 1);
    register_int64_range_variable(cs, "factor", 2, 3);

    written = snprintf(source, sizeof(source), "%s;\n", expressionText);
    TEST_ASSERT_TRUE(written > 0 && (size_t)written < sizeof(source));
    sourceName = ZrCore_String_Create(g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    expression = expression_statement_expression(statement_at(ast, 0));
    TEST_ASSERT_NOT_NULL(expression);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    assert_int64_range_result_and_fact(cs, expression, &result, 0, 0);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_sign_crossing_unary_left_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_unary_left_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((+factor) - factor)");
}

static void test_commuted_unary_right_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_unary_right_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "(factor - (+factor)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_left_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor + zero) - factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_right_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor - (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_left_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "((zero + factor) - factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_right_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "(factor - (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_left_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor - zero) - factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_right_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor - (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_left_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "((factor - zero) - factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_difference_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_right_same_identifier_difference_product_inferred_exact_zero_dataflow_test.zr",
            "(factor - (factor - zero)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_left_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor + zero) % factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_right_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor % (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_left_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "((zero + factor) % factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_right_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "(factor % (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_left_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor - zero) % factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_right_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor % (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_left_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "((factor - zero) % factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_modulo_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_right_same_identifier_modulo_product_inferred_exact_zero_dataflow_test.zr",
            "(factor % (factor - zero)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_left_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor + zero) ^ factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_add_zero_right_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor ^ (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_left_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "((zero + factor) ^ factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_leading_add_zero_right_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "(factor ^ (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_left_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "span * ((factor - zero) ^ factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_subtract_zero_right_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "span * (factor ^ (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_left_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "((factor - zero) ^ factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_xor_product_infers_exact_zero(void) {
    assert_inferred_exact_zero_expression(
            "numeric_commuted_subtract_zero_right_same_identifier_xor_product_inferred_exact_zero_dataflow_test.zr",
            "(factor ^ (factor - zero)) * span");
}

static void test_sign_crossing_unary_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_unary_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((+factor) - factor)");
}

static void test_sign_crossing_unary_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_unary_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((+factor) - factor)");
}

static void test_sign_crossing_unary_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_unary_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor - (+factor))");
}

static void test_sign_crossing_unary_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_unary_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor - (+factor))");
}

static void test_commuted_unary_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_unary_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((+factor) - factor) * span");
}

static void test_commuted_unary_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_unary_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((+factor) - factor) * span");
}

static void test_commuted_unary_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_unary_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor - (+factor)) * span");
}

static void test_commuted_unary_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_unary_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor - (+factor)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor + zero) - factor)");
}

static void test_sign_crossing_add_zero_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor + zero) - factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor - (factor + zero))");
}

static void test_sign_crossing_add_zero_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor - (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((zero + factor) - factor) * span");
}

static void test_commuted_leading_add_zero_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((zero + factor) - factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor - (zero + factor)) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor - (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor - zero) - factor)");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor - zero) - factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor - (factor - zero))");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor - (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((factor - zero) - factor) * span");
}

static void test_commuted_subtract_zero_left_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((factor - zero) - factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_difference_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_difference_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor - (factor - zero)) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_difference_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_difference_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor - (factor - zero)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor + zero) % factor)");
}

static void test_sign_crossing_add_zero_left_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor + zero) % factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor % (factor + zero))");
}

static void test_sign_crossing_add_zero_right_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor % (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((zero + factor) % factor) * span");
}

static void test_commuted_leading_add_zero_left_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((zero + factor) % factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor % (zero + factor)) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor % (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor - zero) % factor)");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor - zero) % factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor % (factor - zero))");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor % (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((factor - zero) % factor) * span");
}

static void test_commuted_subtract_zero_left_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((factor - zero) % factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_modulo_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_modulo_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor % (factor - zero)) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_modulo_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_modulo_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor % (factor - zero)) * span");
}

static void test_sign_crossing_add_zero_left_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor + zero) ^ factor)");
}

static void test_sign_crossing_add_zero_left_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_left_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor + zero) ^ factor)");
}

static void test_sign_crossing_add_zero_right_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor ^ (factor + zero))");
}

static void test_sign_crossing_add_zero_right_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_add_zero_right_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor ^ (factor + zero))");
}

static void test_commuted_leading_add_zero_left_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((zero + factor) ^ factor) * span");
}

static void test_commuted_leading_add_zero_left_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_left_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((zero + factor) ^ factor) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor ^ (zero + factor)) * span");
}

static void test_commuted_leading_add_zero_right_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_leading_add_zero_right_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor ^ (zero + factor)) * span");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * ((factor - zero) ^ factor)");
}

static void test_sign_crossing_subtract_zero_left_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_left_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * ((factor - zero) ^ factor)");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "span * (factor ^ (factor - zero))");
}

static void test_sign_crossing_subtract_zero_right_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_sign_crossing_subtract_zero_right_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "span * (factor ^ (factor - zero))");
}

static void test_commuted_subtract_zero_left_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "((factor - zero) ^ factor) * span");
}

static void test_commuted_subtract_zero_left_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_left_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "((factor - zero) ^ factor) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_xor_product_positive_delta(void) {
    run_upward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_xor_product_offset_conditional_positive_delta_dataflow_test.zr",
            "(factor ^ (factor - zero)) * span");
}

static void test_commuted_subtract_zero_right_same_identifier_xor_product_negative_delta(void) {
    run_downward_case(
            "numeric_while_self_dependent_target_reading_subtract_zero_minus_commuted_subtract_zero_right_same_identifier_xor_product_offset_conditional_negative_delta_dataflow_test.zr",
            "(factor ^ (factor - zero)) * span");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sign_crossing_unary_left_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_commuted_unary_right_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_difference_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_modulo_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_xor_product_infers_exact_zero);
    RUN_TEST(test_sign_crossing_unary_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_unary_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_unary_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_unary_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_unary_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_unary_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_unary_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_unary_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_difference_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_difference_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_modulo_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_modulo_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_left_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_sign_crossing_add_zero_right_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_left_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_commuted_leading_add_zero_right_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_left_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_sign_crossing_subtract_zero_right_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_left_same_identifier_xor_product_negative_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_xor_product_positive_delta);
    RUN_TEST(test_commuted_subtract_zero_right_same_identifier_xor_product_negative_delta);
    return UNITY_END();
}
