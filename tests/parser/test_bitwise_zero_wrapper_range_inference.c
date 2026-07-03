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

static void assert_bitwise_zero_wrapper_range(const char *sourceNameText,
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
    register_int64_range_variable(cs, "allOnes", -1, -1);
    register_int64_range_variable(cs, "unit", 2, 3);
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

static void test_and_unary_plus_zero_left_wrapper_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_unary_plus_zero_left_wrapper_range.zr",
            "(+zero) & unit;",
            0,
            0);
}

static void test_and_additive_zero_right_wrapper_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_additive_zero_right_wrapper_range.zr",
            "unit & (zero + zero);",
            0,
            0);
}

static void test_and_subtract_zero_left_wrapper_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_subtract_zero_left_wrapper_range.zr",
            "(zero - zero) & unit;",
            0,
            0);
}

static void test_or_unary_plus_zero_left_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_unary_plus_zero_left_wrapper_range.zr",
            "(+zero) | unit;",
            2,
            3);
}

static void test_or_additive_zero_right_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_additive_zero_right_wrapper_range.zr",
            "unit | (zero + zero);",
            2,
            3);
}

static void test_or_subtract_zero_left_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_subtract_zero_left_wrapper_range.zr",
            "(zero - zero) | unit;",
            2,
            3);
}

static void test_xor_unary_plus_zero_right_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_unary_plus_zero_right_wrapper_range.zr",
            "unit ^ (+zero);",
            2,
            3);
}

static void test_xor_additive_zero_left_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_additive_zero_left_wrapper_range.zr",
            "(zero + zero) ^ unit;",
            2,
            3);
}

static void test_xor_subtract_zero_right_wrapper_records_unit_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_subtract_zero_right_wrapper_range.zr",
            "unit ^ (zero - zero);",
            2,
            3);
}

static void test_and_unary_plus_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_unary_plus_zero_right_wrapper_sign_crossing_range.zr",
            "span & (+zero);",
            0,
            0);
}

static void test_and_additive_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_additive_zero_right_wrapper_sign_crossing_range.zr",
            "span & (zero + zero);",
            0,
            0);
}

static void test_and_subtract_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_subtract_zero_right_wrapper_sign_crossing_range.zr",
            "span & (zero - zero);",
            0,
            0);
}

static void test_or_unary_plus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_unary_plus_zero_left_wrapper_sign_crossing_range.zr",
            "(+zero) | span;",
            -2,
            3);
}

static void test_or_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_additive_zero_left_wrapper_sign_crossing_range.zr",
            "(zero + zero) | span;",
            -2,
            3);
}

static void test_or_subtract_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_subtract_zero_left_wrapper_sign_crossing_range.zr",
            "(zero - zero) | span;",
            -2,
            3);
}

static void test_xor_unary_plus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_unary_plus_zero_left_wrapper_sign_crossing_range.zr",
            "(+zero) ^ span;",
            -2,
            3);
}

static void test_xor_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_additive_zero_left_wrapper_sign_crossing_range.zr",
            "(zero + zero) ^ span;",
            -2,
            3);
}

static void test_xor_subtract_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_subtract_zero_left_wrapper_sign_crossing_range.zr",
            "(zero - zero) ^ span;",
            -2,
            3);
}

static void test_and_unary_minus_zero_left_wrapper_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_unary_minus_zero_left_wrapper_range.zr",
            "(-zero) & unit;",
            0,
            0);
}

static void test_and_unary_minus_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_unary_minus_zero_right_wrapper_sign_crossing_range.zr",
            "span & (-zero);",
            0,
            0);
}

static void test_or_unary_minus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_unary_minus_zero_left_wrapper_sign_crossing_range.zr",
            "(-zero) | span;",
            -2,
            3);
}

static void test_xor_unary_minus_zero_right_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_unary_minus_zero_right_wrapper_sign_crossing_range.zr",
            "span ^ (-zero);",
            -2,
            3);
}

static void test_or_unary_minus_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_unary_minus_additive_zero_left_wrapper_sign_crossing_range.zr",
            "(-(zero + zero)) | span;",
            -2,
            3);
}

static void test_and_unary_minus_direct_range_left_with_zero_right_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_unary_minus_direct_range_left_zero_right_range.zr",
            "(-unit) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_unary_minus_direct_range_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_unary_minus_direct_range_range.zr",
            "(zero + zero) | (-unit);",
            -3,
            -2);
}

static void test_xor_zero_left_with_unary_minus_direct_range_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_unary_minus_direct_range_range.zr",
            "(zero - zero) ^ (-unit);",
            -3,
            -2);
}

static void test_or_zero_left_with_unary_minus_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_unary_minus_additive_identity_operand_range.zr",
            "(zero + zero) | (-(unit + zero));",
            -3,
            -2);
}

static void test_xor_zero_left_with_unary_minus_subtract_zero_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_unary_minus_subtract_zero_operand_range.zr",
            "(zero - zero) ^ (-(unit - zero));",
            -3,
            -2);
}

static void test_or_zero_right_with_unary_minus_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_right_unary_minus_additive_identity_operand_range.zr",
            "(-(zero + unit)) | (zero + zero);",
            -3,
            -2);
}

static void test_or_zero_left_with_zero_minus_direct_range_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_zero_minus_direct_range.zr",
            "(zero + zero) | (zero - unit);",
            -3,
            -2);
}

static void test_xor_zero_left_with_zero_minus_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_zero_minus_additive_identity_operand_range.zr",
            "(zero - zero) ^ (zero - (unit + zero));",
            -3,
            -2);
}

static void test_and_zero_right_with_zero_minus_subtract_zero_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_zero_minus_subtract_zero_operand_range.zr",
            "((+zero) - (unit - zero)) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_unary_minus_zero_minus_direct_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_unary_minus_zero_minus_direct_operand_range.zr",
            "(zero + zero) | (-(zero - unit));",
            2,
            3);
}

static void test_xor_zero_left_with_unary_minus_zero_minus_additive_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_unary_minus_zero_minus_additive_operand_range.zr",
            "(zero - zero) ^ (-(zero - (unit + zero)));",
            2,
            3);
}

static void test_and_zero_right_with_unary_minus_zero_minus_subtract_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_unary_minus_zero_minus_subtract_operand_range.zr",
            "(-(zero - (unit - zero))) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_unary_minus_zero_minus_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_unary_minus_zero_minus_sign_crossing_operand_range.zr",
            "(zero + zero) | (-(zero - span));",
            -2,
            3);
}

static void test_and_zero_right_with_nested_unary_minus_zero_or_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_nested_unary_minus_zero_or_range.zr",
            "((-zero) | span) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_nested_unary_minus_zero_and_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_nested_unary_minus_zero_and_range.zr",
            "(zero + zero) | (span & (-zero));",
            0,
            0);
}

static void test_xor_zero_left_with_nested_unary_minus_zero_or_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_nested_unary_minus_zero_or_range.zr",
            "(zero - zero) ^ ((-zero) | span);",
            -2,
            3);
}

static void test_or_zero_right_with_nested_unary_minus_zero_xor_and_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_right_nested_unary_minus_zero_xor_and_range.zr",
            "((span ^ (-zero)) & (+zero)) | (zero + zero);",
            0,
            0);
}

static void test_and_zero_right_with_sign_crossing_same_identifier_and_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_same_identifier_and_sign_crossing_range.zr",
            "(span & span) & (+zero);",
            0,
            0);
}

static void test_and_zero_right_with_sign_crossing_wrapped_same_identifier_and_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_wrapped_same_identifier_and_sign_crossing_range.zr",
            "((span + zero) & span) & (+zero);",
            0,
            0);
}

static void test_and_zero_right_with_sign_crossing_wrapped_same_identifier_or_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_wrapped_same_identifier_or_sign_crossing_range.zr",
            "(span | (span - zero)) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_sign_crossing_same_identifier_and_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_same_identifier_and_sign_crossing_range.zr",
            "(zero + zero) | (span & span);",
            -2,
            3);
}

static void test_or_zero_left_with_sign_crossing_wrapped_same_identifier_and_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_wrapped_same_identifier_and_sign_crossing_range.zr",
            "(+zero) | ((span + zero) & span);",
            -2,
            3);
}

static void test_xor_zero_left_with_sign_crossing_wrapped_same_identifier_or_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_wrapped_same_identifier_or_sign_crossing_range.zr",
            "(zero - zero) ^ (span | (span - zero));",
            -2,
            3);
}

static void test_and_zero_left_with_sign_crossing_same_identifier_xor_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_left_same_identifier_xor_sign_crossing_range.zr",
            "(span ^ span) & span;",
            0,
            0);
}

static void test_and_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_wrapped_same_identifier_xor_sign_crossing_range.zr",
            "span & (span ^ (span + zero));",
            0,
            0);
}

static void test_or_zero_left_with_sign_crossing_wrapped_same_identifier_xor_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_wrapped_same_identifier_xor_sign_crossing_range.zr",
            "(span ^ (span - zero)) | span;",
            -2,
            3);
}

static void test_xor_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_right_wrapped_same_identifier_xor_sign_crossing_range.zr",
            "span ^ ((span + zero) ^ span);",
            -2,
            3);
}

static void test_or_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_right_wrapped_same_identifier_xor_sign_crossing_range.zr",
            "(zero + zero) | (span ^ (span - zero));",
            0,
            0);
}

static void test_xor_both_zero_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_both_zero_wrapped_same_identifier_xor_sign_crossing_range.zr",
            "(span ^ span) ^ (span ^ (span + zero));",
            0,
            0);
}

static void test_and_signed_all_ones_mask_left_with_sign_crossing_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_sign_crossing_range.zr",
            "allOnes & span;",
            -2,
            3);
}

static void test_and_zero_right_with_signed_all_ones_mask_left_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_mask_left_sign_crossing_range.zr",
            "(allOnes & span) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_sign_crossing_range.zr",
            "(zero + zero) | (allOnes & span);",
            -2,
            3);
}

static void test_xor_zero_left_with_signed_all_ones_mask_left_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_signed_all_ones_mask_left_sign_crossing_range.zr",
            "(zero - zero) ^ (allOnes & span);",
            -2,
            3);
}

static void test_and_zero_right_with_signed_all_ones_mask_right_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_mask_right_sign_crossing_range.zr",
            "(span & allOnes) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_mask_right_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_right_sign_crossing_range.zr",
            "(zero + zero) | (span & allOnes);",
            -2,
            3);
}

static void test_xor_zero_left_with_signed_all_ones_mask_right_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_signed_all_ones_mask_right_sign_crossing_range.zr",
            "(zero - zero) ^ (span & allOnes);",
            -2,
            3);
}

static void test_and_signed_all_ones_additive_identity_mask_left_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_additive_identity_mask_left_range.zr",
            "(allOnes + zero) & span;",
            -2,
            3);
}

static void test_and_signed_all_ones_subtract_zero_identity_mask_right_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_subtract_zero_identity_mask_right_range.zr",
            "span & (allOnes - zero);",
            -2,
            3);
}

static void test_and_zero_right_with_signed_all_ones_additive_identity_mask_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_additive_identity_mask_range.zr",
            "((zero + allOnes) & span) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_subtract_zero_identity_mask_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_subtract_zero_identity_mask_range.zr",
            "(zero + zero) | ((allOnes - zero) & span);",
            -2,
            3);
}

static void test_xor_zero_left_with_signed_all_ones_additive_identity_mask_right_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_signed_all_ones_additive_identity_mask_right_range.zr",
            "(zero - zero) ^ (span & (zero + allOnes));",
            -2,
            3);
}

static void test_and_signed_all_ones_mask_left_with_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_additive_identity_operand_range.zr",
            "allOnes & (span + zero);",
            -2,
            3);
}

static void test_and_zero_right_with_signed_all_ones_mask_right_subtract_zero_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_mask_right_subtract_zero_operand_range.zr",
            "((span - zero) & allOnes) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_additive_identity_operand_range.zr",
            "(zero + zero) | (allOnes & (zero + span));",
            -2,
            3);
}

static void test_xor_zero_left_with_signed_all_ones_mask_right_subtract_zero_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_xor_zero_left_signed_all_ones_mask_right_subtract_zero_operand_range.zr",
            "(zero - zero) ^ ((span - zero) & allOnes);",
            -2,
            3);
}

static void test_and_signed_all_ones_mask_left_with_unary_minus_direct_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_unary_minus_direct_operand_range.zr",
            "allOnes & (-unit);",
            -3,
            -2);
}

static void test_and_zero_right_with_signed_all_ones_mask_right_unary_minus_direct_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_mask_right_unary_minus_direct_operand_range.zr",
            "((-unit) & allOnes) & (+zero);",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_direct_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_unary_minus_direct_operand_range.zr",
            "(zero + zero) | (allOnes & (-unit));",
            -3,
            -2);
}

static void test_and_signed_all_ones_mask_left_with_unary_minus_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_unary_minus_additive_identity_operand_range.zr",
            "allOnes & (-(unit + zero));",
            -3,
            -2);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_subtract_zero_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_unary_minus_subtract_zero_operand_range.zr",
            "(zero + zero) | (allOnes & (-(unit - zero)));",
            -3,
            -2);
}

static void test_and_signed_all_ones_mask_left_with_zero_minus_direct_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_zero_minus_direct_operand_range.zr",
            "allOnes & (zero - unit);",
            -3,
            -2);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_zero_minus_additive_identity_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_zero_minus_additive_identity_operand_range.zr",
            "(zero + zero) | (allOnes & (zero - (unit + zero)));",
            -3,
            -2);
}

static void test_and_signed_all_ones_mask_left_with_unary_minus_zero_minus_direct_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_signed_all_ones_mask_left_unary_minus_zero_minus_direct_operand_range.zr",
            "allOnes & (-(zero - unit));",
            2,
            3);
}

static void test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_zero_minus_operand_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_mask_left_unary_minus_zero_minus_operand_range.zr",
            "(zero + zero) | (allOnes & (-(zero - (unit + zero))));",
            2,
            3);
}

static void test_and_same_identifier_sign_crossing_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_same_identifier_sign_crossing_range.zr",
            "span & span;",
            -2,
            3);
}

static void test_and_wrapped_same_identifier_sign_crossing_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_wrapped_same_identifier_sign_crossing_range.zr",
            "(span + zero) & span;",
            -2,
            3);
}

static void test_or_same_identifier_sign_crossing_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_same_identifier_sign_crossing_range.zr",
            "span | span;",
            -2,
            3);
}

static void test_or_wrapped_same_identifier_sign_crossing_records_operand_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_wrapped_same_identifier_sign_crossing_range.zr",
            "span | (span - zero);",
            -2,
            3);
}

static void test_or_signed_all_ones_left_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_left_range.zr",
            "allOnes | span;",
            -1,
            -1);
}

static void test_or_signed_all_ones_right_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_right_range.zr",
            "span | allOnes;",
            -1,
            -1);
}

static void test_or_signed_all_ones_additive_identity_left_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_additive_identity_left_range.zr",
            "(allOnes + zero) | span;",
            -1,
            -1);
}

static void test_or_signed_all_ones_left_with_unary_minus_direct_operand_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_left_unary_minus_direct_operand_range.zr",
            "allOnes | (-unit);",
            -1,
            -1);
}

static void test_or_signed_all_ones_left_with_unary_minus_additive_identity_operand_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_left_unary_minus_additive_identity_operand_range.zr",
            "allOnes | (-(zero + unit));",
            -1,
            -1);
}

static void test_or_signed_all_ones_left_with_zero_minus_subtract_zero_operand_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_left_zero_minus_subtract_zero_operand_range.zr",
            "allOnes | (zero - (unit - zero));",
            -1,
            -1);
}

static void test_or_signed_all_ones_left_with_unary_minus_zero_minus_operand_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_signed_all_ones_left_unary_minus_zero_minus_operand_range.zr",
            "allOnes | (-(zero - (unit - zero)));",
            -1,
            -1);
}

static void test_and_zero_right_with_signed_all_ones_or_unary_minus_direct_operand_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_or_unary_minus_direct_operand_range.zr",
            "((allOnes | (-unit)) & (+zero));",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_or_unary_minus_direct_operand_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_or_unary_minus_direct_operand_range.zr",
            "(zero + zero) | ((-unit) | allOnes);",
            -1,
            -1);
}

static void test_and_zero_right_with_signed_all_ones_or_annihilator_records_exact_zero_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_and_zero_right_signed_all_ones_or_annihilator_range.zr",
            "((allOnes | span) & (+zero));",
            0,
            0);
}

static void test_or_zero_left_with_signed_all_ones_or_annihilator_records_all_ones_range(void) {
    assert_bitwise_zero_wrapper_range(
            "bitwise_or_zero_left_signed_all_ones_or_annihilator_range.zr",
            "(zero + zero) | (span | (zero + allOnes));",
            -1,
            -1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_and_unary_plus_zero_left_wrapper_records_exact_zero_range);
    RUN_TEST(test_and_additive_zero_right_wrapper_records_exact_zero_range);
    RUN_TEST(test_and_subtract_zero_left_wrapper_records_exact_zero_range);
    RUN_TEST(test_or_unary_plus_zero_left_wrapper_records_unit_range);
    RUN_TEST(test_or_additive_zero_right_wrapper_records_unit_range);
    RUN_TEST(test_or_subtract_zero_left_wrapper_records_unit_range);
    RUN_TEST(test_xor_unary_plus_zero_right_wrapper_records_unit_range);
    RUN_TEST(test_xor_additive_zero_left_wrapper_records_unit_range);
    RUN_TEST(test_xor_subtract_zero_right_wrapper_records_unit_range);
    RUN_TEST(test_and_unary_plus_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range);
    RUN_TEST(test_and_additive_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range);
    RUN_TEST(test_and_subtract_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range);
    RUN_TEST(test_or_unary_plus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_or_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_or_subtract_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_xor_unary_plus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_xor_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_xor_subtract_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_and_unary_minus_zero_left_wrapper_records_exact_zero_range);
    RUN_TEST(test_and_unary_minus_zero_right_wrapper_with_sign_crossing_operand_records_exact_zero_range);
    RUN_TEST(test_or_unary_minus_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_xor_unary_minus_zero_right_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_or_unary_minus_additive_zero_left_wrapper_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_and_unary_minus_direct_range_left_with_zero_right_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_unary_minus_direct_range_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_unary_minus_direct_range_records_operand_range);
    RUN_TEST(test_or_zero_left_with_unary_minus_additive_identity_operand_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_unary_minus_subtract_zero_operand_records_operand_range);
    RUN_TEST(test_or_zero_right_with_unary_minus_additive_identity_operand_records_operand_range);
    RUN_TEST(test_or_zero_left_with_zero_minus_direct_range_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_zero_minus_additive_identity_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_zero_minus_subtract_zero_operand_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_unary_minus_zero_minus_direct_operand_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_unary_minus_zero_minus_additive_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_unary_minus_zero_minus_subtract_operand_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_unary_minus_zero_minus_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_nested_unary_minus_zero_or_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_nested_unary_minus_zero_and_records_exact_zero_range);
    RUN_TEST(test_xor_zero_left_with_nested_unary_minus_zero_or_records_operand_range);
    RUN_TEST(test_or_zero_right_with_nested_unary_minus_zero_xor_and_records_exact_zero_range);
    RUN_TEST(test_and_zero_right_with_sign_crossing_same_identifier_and_records_exact_zero_range);
    RUN_TEST(test_and_zero_right_with_sign_crossing_wrapped_same_identifier_and_records_exact_zero_range);
    RUN_TEST(test_and_zero_right_with_sign_crossing_wrapped_same_identifier_or_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_sign_crossing_same_identifier_and_records_operand_range);
    RUN_TEST(test_or_zero_left_with_sign_crossing_wrapped_same_identifier_and_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_sign_crossing_wrapped_same_identifier_or_records_operand_range);
    RUN_TEST(test_and_zero_left_with_sign_crossing_same_identifier_xor_records_exact_zero_range);
    RUN_TEST(test_and_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_sign_crossing_wrapped_same_identifier_xor_records_operand_range);
    RUN_TEST(test_xor_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_operand_range);
    RUN_TEST(test_or_zero_right_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range);
    RUN_TEST(test_xor_both_zero_with_sign_crossing_wrapped_same_identifier_xor_records_exact_zero_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_sign_crossing_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_mask_left_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_signed_all_ones_mask_left_records_operand_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_mask_right_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_right_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_signed_all_ones_mask_right_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_additive_identity_mask_left_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_subtract_zero_identity_mask_right_records_operand_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_additive_identity_mask_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_subtract_zero_identity_mask_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_signed_all_ones_additive_identity_mask_right_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_additive_identity_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_mask_right_subtract_zero_operand_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_additive_identity_operand_records_operand_range);
    RUN_TEST(test_xor_zero_left_with_signed_all_ones_mask_right_subtract_zero_operand_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_unary_minus_direct_operand_records_operand_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_mask_right_unary_minus_direct_operand_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_direct_operand_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_unary_minus_additive_identity_operand_records_operand_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_subtract_zero_operand_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_zero_minus_direct_operand_records_operand_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_zero_minus_additive_identity_operand_records_operand_range);
    RUN_TEST(test_and_signed_all_ones_mask_left_with_unary_minus_zero_minus_direct_operand_records_operand_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_mask_left_unary_minus_zero_minus_operand_records_operand_range);
    RUN_TEST(test_and_same_identifier_sign_crossing_records_operand_range);
    RUN_TEST(test_and_wrapped_same_identifier_sign_crossing_records_operand_range);
    RUN_TEST(test_or_same_identifier_sign_crossing_records_operand_range);
    RUN_TEST(test_or_wrapped_same_identifier_sign_crossing_records_operand_range);
    RUN_TEST(test_or_signed_all_ones_left_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_right_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_additive_identity_left_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_left_with_unary_minus_direct_operand_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_left_with_unary_minus_additive_identity_operand_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_left_with_zero_minus_subtract_zero_operand_records_all_ones_range);
    RUN_TEST(test_or_signed_all_ones_left_with_unary_minus_zero_minus_operand_records_all_ones_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_or_unary_minus_direct_operand_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_or_unary_minus_direct_operand_records_all_ones_range);
    RUN_TEST(test_and_zero_right_with_signed_all_ones_or_annihilator_records_exact_zero_range);
    RUN_TEST(test_or_zero_left_with_signed_all_ones_or_annihilator_records_all_ones_range);
    return UNITY_END();
}
