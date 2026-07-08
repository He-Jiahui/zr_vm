#include "type_inference_bitwise_identity_bitwise_not_or_counterpart.h"
#include "type_inference_bitwise_identity_bitwise_not_or_counterpart_deep.h"

TZrBool type_inference_bitwise_identity_bitwise_not_negate_range_in_place(
        TZrInt64 *minValue,
        TZrInt64 *maxValue) {
    TZrInt64 oldMinValue;
    TZrInt64 oldMaxValue;

    if (minValue == ZR_NULL ||
        maxValue == ZR_NULL ||
        *minValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    oldMinValue = *minValue;
    oldMaxValue = *maxValue;
    *minValue = -oldMaxValue;
    *maxValue = -oldMinValue;
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_unary_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue) ||
        !type_inference_bitwise_identity_bitwise_not_negate_range_in_place(
                &operandMinValue,
                &operandMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = operandMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = operandMaxValue;
    }
    return ZR_TRUE;
}

const SZrAstNode *type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
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

TZrBool type_inference_bitwise_identity_expression_bitwise_not_or_counterpart_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    expression = type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
            cs,
            expression);
    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                expression,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                cs,
                expression,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_bitwise_not_or_deep_counterpart_leaf_int64_range(
                cs,
                expression,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_TRUE;
    }
    return type_inference_bitwise_identity_expression_signed_same_identifier_range(
            cs,
            expression,
            ZR_NULL,
            ZR_NULL);
}
