#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_DIRECT_RANGE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_DIRECT_RANGE_H

#include "type_inference_internal.h"

TZrBool type_inference_bitwise_identity_operator_is(
        const TZrChar *actual,
        const TZrChar *expected);
TZrBool type_inference_bitwise_identity_expressions_are_same_identifier(
        const SZrAstNode *left,
        const SZrAstNode *right);
const SZrAstNode *type_inference_bitwise_identity_skip_unary_plus_identity(
        const SZrAstNode *expression);
const SZrAstNode *type_inference_bitwise_identity_skip_zero_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression);
TZrBool type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        const SZrAstNode **outIdentifierExpression);
TZrBool type_inference_bitwise_identity_expression_is_zero_literal(
        const SZrAstNode *expression);
const SZrTypeBinding *type_inference_bitwise_identity_expression_binding(
        SZrCompilerState *cs,
        const SZrAstNode *expression);
TZrBool type_inference_bitwise_identity_expression_is_exact_zero_value(
        SZrCompilerState *cs,
        const SZrAstNode *expression);
TZrBool type_inference_bitwise_identity_expression_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_mask_range(
        TZrInt64 leftMinValue,
        TZrInt64 leftMaxValue,
        TZrInt64 rightMinValue,
        TZrInt64 rightMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_basic_zero_minus_chain_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_signed_same_identifier_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_signed_same_identifier_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
        TZrInt64 minValue,
        TZrInt64 maxValue);
TZrBool type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_signed_all_ones_or_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_signed_same_identifier_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
