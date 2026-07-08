#include "type_inference_bitwise_identity_supported_count_all_ones_side.h"

#include "type_inference_bitwise_identity_direct_range.h"

static const SZrAstNode *type_inference_bitwise_identity_supported_count_all_ones_side_skip_zero_bitwise_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;

    while (expression != ZR_NULL) {
        expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(
                cs,
                expression);
        expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
        if (expression == ZR_NULL ||
            expression->type != ZR_AST_BINARY_EXPRESSION) {
            break;
        }

        binary = &expression->data.binaryExpression;
        if (!type_inference_bitwise_identity_operator_is(binary->op.op, "|") &&
            !type_inference_bitwise_identity_operator_is(binary->op.op, "^")) {
            break;
        }

        if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left)) {
            expression = binary->right;
            continue;
        }
        if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->right)) {
            expression = binary->left;
            continue;
        }
        break;
    }

    return expression;
}

TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_supported_count_all_ones_side_skip_zero_bitwise_identity_wrappers(
            cs,
            expression);

    if (type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
                cs,
                expression,
                &minValue,
                &maxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "|") ||
        !type_inference_bitwise_identity_expression_signed_all_ones_or_range(
                cs,
                binary->left,
                binary->right,
                &minValue,
                &maxValue) ||
        !type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                minValue,
                maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}
