#include "unity.h"

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

static void assert_int64_range_result_and_fact(SZrCompilerState *cs,
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

static void run_scale_product_coefficient_range_case(const char *sourceNameText,
                                                     const char *source,
                                                     TZrBool hasGate,
                                                     TZrInt64 scaleMin,
                                                     TZrInt64 scaleMax,
                                                     TZrInt64 maskMin,
                                                     TZrInt64 maskMax,
                                                     TZrInt64 gateMin,
                                                     TZrInt64 gateMax,
                                                     TZrInt64 expectedMin,
                                                     TZrInt64 expectedMax) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;

    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", 1, 3);
    register_int64_range_variable(cs, "factor", -1, 1);
    register_int64_range_variable(cs, "scale", scaleMin, scaleMax);
    register_int64_range_variable(cs, "outer", 1, 1);
    register_int64_range_variable(cs, "span", 1, 1);
    register_int64_range_variable(cs, "mask", maskMin, maskMax);
    if (hasGate) {
        register_int64_range_variable(cs, "gate", gateMin, gateMax);
    }

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(
            cs,
            targetExpression,
            &targetResult,
            expectedMin,
            expectedMax);

    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void run_scale_product_coefficient_residual_case(const char *sourceNameText,
                                                        const char *source,
                                                        TZrBool hasGate,
                                                        TZrInt64 scaleMin,
                                                        TZrInt64 scaleMax,
                                                        TZrInt64 maskMin,
                                                        TZrInt64 maskMax,
                                                        TZrInt64 gateMin,
                                                        TZrInt64 gateMax) {
    run_scale_product_coefficient_range_case(
            sourceNameText,
            source,
            hasGate,
            scaleMin,
            scaleMax,
            maskMin,
            maskMax,
            gateMin,
            gateMax,
            5,
            ZR_TYPE_RANGE_INT64_MAX);
}

static void run_direct_scale_product_coefficient_residual_case(const char *sourceNameText,
                                                               const char *source,
                                                               TZrInt64 scaleMin,
                                                               TZrInt64 scaleMax,
                                                               TZrInt64 maskMin,
                                                               TZrInt64 maskMax) {
    run_scale_product_coefficient_residual_case(
            sourceNameText,
            source,
            ZR_FALSE,
            scaleMin,
            scaleMax,
            maskMin,
            maskMax,
            0,
            0);
}

static void run_chained_scale_product_coefficient_residual_case(const char *sourceNameText,
                                                                const char *source,
                                                                TZrInt64 scaleMin,
                                                                TZrInt64 scaleMax,
                                                                TZrInt64 maskMin,
                                                                TZrInt64 maskMax,
                                                                TZrInt64 gateMin,
                                                                TZrInt64 gateMax) {
    run_scale_product_coefficient_residual_case(
            sourceNameText,
            source,
            ZR_TRUE,
            scaleMin,
            scaleMax,
            maskMin,
            maskMax,
            gateMin,
            gateMax);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_positive_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_positive_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            2,
            2,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_positive_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_positive_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            1,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_positive_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_positive_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            1,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_positive_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_positive_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            2,
            3,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            0,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_negative_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_negative_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -2,
            -1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_only_scale_product_coefficient_noop_preserves_target(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (outer * (mask * gate)))));\n"
            "    other = narrowed;\n"
            "}\n"
            "narrowed + 0;\n";

    run_scale_product_coefficient_range_case(
            "numeric_while_self_dependent_symbolic_deeper_two_additional_level_zero_only_scale_product_coefficient_noop_dataflow_test.zr",
            source,
            ZR_TRUE,
            0,
            0,
            1,
            1,
            1,
            1,
            5,
            5);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_positive_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_positive_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            2,
            3,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_negative_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_negative_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -2,
            -1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_zero_inclusive_positive_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_zero_inclusive_negative_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            0,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_chained_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            1,
            1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_only_scale_product_coefficient_noop_preserves_target(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * (mask * gate))));\n"
            "    other = narrowed;\n"
            "}\n"
            "narrowed + 0;\n";

    run_scale_product_coefficient_range_case(
            "numeric_while_self_dependent_symbolic_deeper_chained_zero_only_scale_product_coefficient_noop_dataflow_test.zr",
            source,
            ZR_TRUE,
            0,
            0,
            1,
            1,
            1,
            1,
            5,
            5);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_positive_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_positive_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            2,
            3,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_zero_inclusive_positive_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_zero_inclusive_negative_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            0,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_negative_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_negative_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -2,
            -1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            1,
            1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_double_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_double_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            1,
            -1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_positive_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_mixed_zero_inclusive_positive_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            -1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_negative_sign_crossing_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_mixed_zero_inclusive_negative_sign_crossing_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            0,
            -1,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_positive_negative_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_mixed_zero_inclusive_positive_negative_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            -1,
            0);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_double_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_double_zero_inclusive_positive_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            0,
            1,
            0,
            1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_double_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_double_zero_inclusive_negative_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -1,
            0,
            -1,
            0);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_double_negative_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_double_negative_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -2,
            -1,
            -1,
            -1);
}

static void test_while_self_dependent_target_reading_symbolic_deeper_double_negative_non_singleton_scale_product_coefficient_residual_widens_upward(void) {
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (scale * mask)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed + (step + step + step + step);\n"
            "}\n"
            "narrowed + 0;\n";

    run_direct_scale_product_coefficient_residual_case(
            "numeric_while_self_dependent_symbolic_deeper_double_negative_non_singleton_scale_product_coefficient_residual_dataflow_test.zr",
            source,
            -2,
            -1,
            -2,
            -1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_positive_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_positive_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_positive_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_positive_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_negative_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_two_additional_level_zero_only_scale_product_coefficient_noop_preserves_target);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_positive_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_negative_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_chained_zero_only_scale_product_coefficient_noop_preserves_target);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_positive_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_negative_non_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_double_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_positive_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_negative_sign_crossing_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_mixed_zero_inclusive_positive_negative_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_double_zero_inclusive_positive_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_double_zero_inclusive_negative_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_double_negative_singleton_scale_product_coefficient_residual_widens_upward);
    RUN_TEST(test_while_self_dependent_target_reading_symbolic_deeper_double_negative_non_singleton_scale_product_coefficient_residual_widens_upward);
    return UNITY_END();
}
