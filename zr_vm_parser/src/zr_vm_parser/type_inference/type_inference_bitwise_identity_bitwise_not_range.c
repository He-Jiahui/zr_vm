#include "type_inference_bitwise_identity_bitwise_not_or_counterpart.h"

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

static TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_evaluated_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrAstNode *currentExpression = expression;
    const SZrAstNode *leafExpression;
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    int depth;
    int negationIndex;

    depth = 0;
    while (ZR_TRUE) {
        currentExpression = type_inference_bitwise_identity_skip_unary_plus_identity(
                currentExpression);
        if (currentExpression == ZR_NULL ||
            currentExpression->type != ZR_AST_BINARY_EXPRESSION) {
            return ZR_FALSE;
        }

        binary = &currentExpression->data.binaryExpression;
        if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
            !type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left)) {
            return ZR_FALSE;
        }

        currentExpression = binary->right;
        leafExpression = type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
                cs,
                currentExpression);
        if (!type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                    cs,
                    leafExpression,
                    &minValue,
                    &maxValue) ||
            !type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                    minValue,
                    maxValue)) {
            ++depth;
            continue;
        }

        for (negationIndex = 0; negationIndex <= depth; ++negationIndex) {
            if (!type_inference_bitwise_identity_bitwise_not_negate_range_in_place(
                        &minValue,
                        &maxValue)) {
                return ZR_FALSE;
            }
        }

        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (!type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_evaluated_int64_range(
                cs,
                expression,
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

TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_evaluated_int64_range(
                cs,
                unary->argument,
                &minValue,
                &maxValue) ||
        !type_inference_bitwise_identity_bitwise_not_negate_range_in_place(
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

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_or_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 exactMinValue;
    TZrInt64 exactMaxValue;

    if (type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                cs,
                left,
                &exactMinValue,
                &exactMaxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                exactMinValue,
                exactMaxValue) &&
        type_inference_bitwise_identity_expression_bitwise_not_or_counterpart_leaf_int64_range(
                cs,
                right)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = -1;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = -1;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                cs,
                right,
                &exactMinValue,
                &exactMaxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                exactMinValue,
                exactMaxValue) &&
        type_inference_bitwise_identity_expression_bitwise_not_or_counterpart_leaf_int64_range(
                cs,
                left)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = -1;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = -1;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
            cs,
            expression);
    if ((type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
                 cs,
                 expression,
                 &minValue,
                 &maxValue) ||
         type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                 cs,
                 expression,
                 &minValue,
                 &maxValue)) &&
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

    if (expression != ZR_NULL &&
        expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (type_inference_bitwise_identity_operator_is(binary->op.op, "-") &&
            type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
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
    }

    if (expression != ZR_NULL &&
        expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (type_inference_bitwise_identity_operator_is(unary->op.op, "-") &&
            type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
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
    }

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (type_inference_bitwise_identity_operator_is(binary->op.op, "|") &&
        type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_or_direct_leaf_int64_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if ((!type_inference_bitwise_identity_operator_is(binary->op.op, "&") &&
         !type_inference_bitwise_identity_operator_is(binary->op.op, "|")) ||
        !type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                cs,
                binary->left,
                ZR_NULL,
                ZR_NULL) ||
        !type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                cs,
                binary->right,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -1;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (type_inference_bitwise_identity_expression_bitwise_not_exact_signed_all_ones_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_exact_signed_all_ones_operand_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_basic_zero_minus_chain_leaf_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_count_side_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrAstNode *countExpression;

    countExpression = type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
            cs,
            expression);
    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                countExpression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue) &&
        type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_count_side_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = rightMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = rightMaxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue) &&
        type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue) &&
        type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_count_side_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = leftMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = leftMaxValue;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_or_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (!type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) ||
        !type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        return ZR_FALSE;
    }

    if (!type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue) &&
        !type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -1;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;

    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_is_exact_zero_value(cs, expression)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = 0;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = 0;
        }
        return ZR_TRUE;
    }

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "&") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "|")) &&
        type_inference_bitwise_identity_expression_signed_same_identifier_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(binary->op.op, "&")) {
        return type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue);
    }

    if (type_inference_bitwise_identity_operator_is(binary->op.op, "|")) {
        return type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_or_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue);
    }

    return ZR_FALSE;
}

TZrBool type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
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
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_bitwise_not_operand_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = (TZrInt64)(~((TZrUInt64)operandMaxValue));
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = (TZrInt64)(~((TZrUInt64)operandMinValue));
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    return type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}
