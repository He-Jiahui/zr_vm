#include "type_inference_bitwise_identity_supported_count_wrapped.h"

#include "type_inference_bitwise_identity_direct_range.h"
#include "type_inference_bitwise_identity_supported_count_all_ones_side.h"

static TZrBool type_inference_bitwise_identity_supported_count_negate_range_in_place(
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

static const SZrAstNode *type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
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

TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrAstNode *argument;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    argument = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            unary->argument);
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        argument == ZR_NULL ||
        argument->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &argument->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                binary->right,
                &minValue,
                &maxValue)) {
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

TZrBool type_inference_bitwise_identity_expression_unary_minus_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "-") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue) ||
        maxValue == ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue + 1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue + 1;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_double_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "-") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue)) {
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

TZrBool type_inference_bitwise_identity_expression_bitwise_not_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "~") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue) ||
        minValue <= 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue - 1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue - 1;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_double_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "~") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue)) {
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

TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrAstNode *rightExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    rightExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            binary->right);
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        rightExpression == ZR_NULL ||
        rightExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &rightExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                unary->argument,
                &minValue,
                &maxValue)) {
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

TZrBool type_inference_bitwise_identity_expression_double_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *outerBinary;
    const SZrBinaryExpression *innerBinary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerBinary = &expression->data.binaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerBinary->right);
    if (!type_inference_bitwise_identity_operator_is(outerBinary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                outerBinary->left) ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerBinary = &innerExpression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerBinary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                innerBinary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerBinary->right,
                &minValue,
                &maxValue)) {
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

TZrBool type_inference_bitwise_identity_expression_bitwise_not_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrAstNode *argument;
    TZrInt64 countMinValue;
    TZrInt64 countMaxValue;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    argument = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            unary->argument);
    argument = type_inference_bitwise_identity_skip_unary_plus_identity(argument);
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        argument == ZR_NULL ||
        argument->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &argument->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                binary->right,
                &countMinValue,
                &countMaxValue)) {
        return ZR_FALSE;
    }

    operandMinValue = -countMaxValue;
    operandMaxValue = -countMinValue;
    minValue = (TZrInt64)(~((TZrUInt64)operandMaxValue));
    maxValue = (TZrInt64)(~((TZrUInt64)operandMinValue));
    if (minValue < 0) {
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

static TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
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
        leafExpression = type_inference_bitwise_identity_skip_zero_identity_wrappers(
                cs,
                currentExpression);
        if (!type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
                    cs,
                    leafExpression,
                    &minValue,
                    &maxValue) ||
            minValue < 0) {
            ++depth;
            continue;
        }

        for (negationIndex = 0; negationIndex <= depth; ++negationIndex) {
            if (!type_inference_bitwise_identity_supported_count_negate_range_in_place(
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

TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (!type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
                cs,
                expression,
                &minValue,
                &maxValue) ||
        minValue < 0) {
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

TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
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
        !type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
                cs,
                unary->argument,
                &minValue,
                &maxValue) ||
        !type_inference_bitwise_identity_supported_count_negate_range_in_place(
                &minValue,
                &maxValue) ||
        minValue < 0) {
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

TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (!((type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                   cs,
                   expression,
                   &minValue,
                   &maxValue) &&
           minValue >= 0) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_supported_nonnegative_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue))) {
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

static TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_side_bitwise_not_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "&")) {
        return ZR_FALSE;
    }

    if ((type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                 cs,
                 binary->left,
                 ZR_NULL,
                 ZR_NULL) &&
         type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                 cs,
                 binary->right,
                 &minValue,
                 &maxValue)) ||
        (type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                 cs,
                 binary->right,
                 ZR_NULL,
                 ZR_NULL) &&
         type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                 cs,
                 binary->left,
                 &minValue,
                 &maxValue))) {
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

TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrAstNode *rightExpression;
    TZrInt64 oldMinValue;
    TZrInt64 oldMaxValue;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    rightExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            binary->right);
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        rightExpression == ZR_NULL ||
        rightExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &rightExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        !((type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                   cs,
                   unary->argument,
                   &minValue,
                   &maxValue) &&
           minValue >= 0) ||
          type_inference_bitwise_identity_expression_supported_count_all_ones_side_bitwise_not_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_supported_nonnegative_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue))) {
        return ZR_FALSE;
    }

    oldMinValue = minValue;
    oldMaxValue = maxValue;
    minValue = (TZrInt64)(~((TZrUInt64)oldMaxValue));
    maxValue = (TZrInt64)(~((TZrUInt64)oldMinValue));
    if (!type_inference_bitwise_identity_supported_count_negate_range_in_place(
                &minValue,
                &maxValue) ||
        minValue < 0) {
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
