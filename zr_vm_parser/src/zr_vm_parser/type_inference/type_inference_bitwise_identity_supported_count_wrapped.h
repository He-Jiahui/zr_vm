#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_WRAPPED_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_WRAPPED_H

#include "type_inference_internal.h"

TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_unary_minus_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_double_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_bitwise_not_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_double_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_double_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_bitwise_not_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
