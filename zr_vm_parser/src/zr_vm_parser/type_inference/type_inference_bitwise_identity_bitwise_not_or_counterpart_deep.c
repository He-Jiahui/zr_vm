#include "type_inference_bitwise_identity_bitwise_not_or_counterpart_deep.h"

#include "type_inference_bitwise_identity_bitwise_not_or_counterpart.h"

static TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                binary->right,
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

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_leaf_int64_range(
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

TZrBool type_inference_bitwise_identity_expression_bitwise_not_or_deep_counterpart_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_unary_minus_zero_minus_zero_minus_bitwise_not_direct_leaf_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
