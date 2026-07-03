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

static void assert_bitwise_zero_minus_shift_supported_count_range(
        const char *sourceNameText,
        const char *source,
        TZrInt64 expectedMin,
        TZrInt64 expectedMax) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expression;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;

    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expression = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expression->type);
    register_int64_range_variable(cs, "zero", 0, 0);
    register_int64_range_variable(cs, "unit", 2, 3);
    register_int64_range_variable(cs, "negativeUnit", -3, -2);
    register_int64_range_variable(cs, "negativeFour", -4, -3);
    register_int64_range_variable(cs, "allOnes", -1, -1);
    register_int64_range_variable(cs, "span", -2, 3);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, result.minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_or_zero_left_with_shift_left_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit + unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit + unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + (span - span))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit - (span - span))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (unit + (span - span))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit - (span - span))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & unit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & unit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (unit & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (allOnes & (unit & unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> ((unit | (unit - zero)) & allOnes)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_chain_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_chain_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (zero - (zero - unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (zero - (zero - (unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (-negativeUnit)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (-negativeUnit)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (-(zero - (unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~negativeFour)) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(zero - unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~negativeFour)) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(zero - unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(unit + zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & negativeUnit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((zero - unit) & allOnes))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes | unit))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(unit | allOnes))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes | unit))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(unit | allOnes))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(negativeFour & negativeFour))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(negativeFour | (negativeFour - zero)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(zero - (negativeFour & negativeFour))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-(zero - (negativeFour | (negativeFour - zero)))))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(allOnes & (negativeFour & negativeFour)))) - (-(zero - unit)));",
            -3,
            -2);
}

static void test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(allOnes | (negativeFour | (negativeFour - zero))))) - (-(zero - unit)));",
            -3,
            -2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_additive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_subtractive_exact_zero_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_all_ones_mask_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_zero_minus_chain_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_unary_minus_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_direct_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_wrapped_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_zero_minus_all_ones_mask_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_zero_minus_same_identifier_and_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_zero_minus_same_identifier_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_all_ones_mask_same_identifier_and_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_all_ones_or_same_identifier_or_operand_count_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
