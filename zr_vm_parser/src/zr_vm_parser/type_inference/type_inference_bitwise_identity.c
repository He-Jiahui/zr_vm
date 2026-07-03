#include "type_inference_internal.h"
#include "type_inference_bitwise_identity_direct_range.h"

#include <limits.h>
#include <string.h>

static TZrBool type_inference_bitwise_identity_mask_range(TZrInt64 leftMinValue,
                                                          TZrInt64 leftMaxValue,
                                                          TZrInt64 rightMinValue,
                                                          TZrInt64 rightMaxValue,
                                                          TZrInt64 *outMinValue,
                                                          TZrInt64 *outMaxValue);
static TZrBool type_inference_bitwise_identity_expression_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

TZrBool type_inference_bitwise_identity_expression_is_exact_zero_value(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrTypeBinding *binding;
    const SZrAstNode *sameIdentifierExpression;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;
    TZrInt64 int64SignBitShiftCount;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        return type_inference_bitwise_identity_operator_is(unary->op.op, "-") &&
               type_inference_bitwise_identity_expression_is_exact_zero_value(
                       cs,
                       unary->argument);
    }

    if (type_inference_bitwise_identity_expression_is_zero_literal(expression)) {
        return ZR_TRUE;
    }

    binding = type_inference_bitwise_identity_expression_binding(cs, expression);
    if (binding != ZR_NULL &&
        binding->type.baseType == ZR_VALUE_TYPE_INT64 &&
        binding->type.hasRangeConstraint &&
        binding->type.minValue == 0 &&
        binding->type.maxValue == 0) {
        return ZR_TRUE;
    }

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "&") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "*")) &&
        ((type_inference_bitwise_identity_expression_is_exact_zero_value(
                  cs,
                  binary->left) &&
          type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                  cs,
                  binary->right,
                  ZR_NULL,
                  ZR_NULL)) ||
         (type_inference_bitwise_identity_expression_is_exact_zero_value(
                  cs,
                  binary->right) &&
          type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                  cs,
                  binary->left,
                  ZR_NULL,
                  ZR_NULL)))) {
        return ZR_TRUE;
    }

    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "&") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "*")) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->right)) {
        return ZR_TRUE;
    }

    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "|") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "^")) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->right)) {
        return ZR_TRUE;
    }

    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "/") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "%")) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) &&
        type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                cs,
                binary->right,
                &rightMinValue,
                &rightMaxValue) &&
        (rightMinValue > 0 || rightMaxValue < 0)) {
        return ZR_TRUE;
    }

    int64SignBitShiftCount = (TZrInt64)(sizeof(TZrInt64) * CHAR_BIT - 1u);
    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "<<") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, ">>")) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) &&
        (type_inference_bitwise_identity_expression_is_exact_zero_value(
                 cs,
                 binary->right) ||
         (type_inference_bitwise_identity_expression_supported_nonnegative_range(
                  cs,
                  binary->right,
                  &rightMinValue,
                  &rightMaxValue) &&
          rightMinValue >= 0 &&
          rightMaxValue < int64SignBitShiftCount))) {
        return ZR_TRUE;
    }

    if ((!type_inference_bitwise_identity_operator_is(binary->op.op, "^") &&
         !type_inference_bitwise_identity_operator_is(binary->op.op, "-") &&
         !type_inference_bitwise_identity_operator_is(binary->op.op, "%")) ||
        !type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                cs,
                binary->left,
                binary->right,
                &sameIdentifierExpression)) {
        return ZR_FALSE;
    }

    binding = type_inference_bitwise_identity_expression_binding(cs, sameIdentifierExpression);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint ||
        binding->type.maxValue < binding->type.minValue) {
        return ZR_FALSE;
    }

    if (type_inference_bitwise_identity_operator_is(binary->op.op, "%") &&
        binding->type.minValue <= 0 &&
        binding->type.maxValue >= 0) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrTypeBinding *binding;
    const SZrAstNode *sameIdentifierExpression;
    const TZrChar *op;
    TZrInt64 identityMinValue;
    TZrInt64 identityMaxValue;
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;
    TZrInt64 int64SignBitShiftCount;

    int64SignBitShiftCount = (TZrInt64)(sizeof(TZrInt64) * CHAR_BIT - 1u);
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);

    if (type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
                cs,
                expression,
                &identityMinValue,
                &identityMaxValue) &&
        identityMinValue >= 0) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = identityMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = identityMaxValue;
        }
        return ZR_TRUE;
    }

    if (expression != ZR_NULL &&
        expression->type == ZR_AST_INTEGER_LITERAL &&
        expression->data.integerLiteral.value >= 0) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = expression->data.integerLiteral.value;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = expression->data.integerLiteral.value;
        }
        return ZR_TRUE;
    }

    if (expression != ZR_NULL &&
        expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        op = unary->op.op;
        if (op != ZR_NULL && strcmp(op, "+") == 0) {
            return type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    unary->argument,
                    outMinValue,
                    outMaxValue);
        }
        if (op != ZR_NULL &&
            strcmp(op, "-") == 0 &&
            type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
                    cs,
                    expression,
                    &identityMinValue,
                    &identityMaxValue) &&
            identityMinValue >= 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }
        if (op != ZR_NULL &&
            strcmp(op, "~") == 0 &&
            type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
                    cs,
                    expression,
                    &identityMinValue,
                    &identityMaxValue) &&
            identityMinValue >= 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (expression != ZR_NULL &&
        expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        op = binary->op.op;
        if (op == ZR_NULL) {
            return ZR_FALSE;
        }

        if (strcmp(op, "+") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            leftMaxValue <= ZR_TYPE_RANGE_INT64_MAX - rightMaxValue) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue + rightMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue + rightMaxValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "-") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            leftMinValue >= rightMaxValue) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue - rightMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue - rightMinValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "*") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            (leftMaxValue == 0 || rightMaxValue <= ZR_TYPE_RANGE_INT64_MAX / leftMaxValue)) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue * rightMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue * rightMaxValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "/") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            rightMinValue > 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue / rightMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue / rightMinValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "%") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            rightMinValue > 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = 0;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = rightMaxValue - 1;
                if (leftMaxValue < *outMaxValue) {
                    *outMaxValue = leftMaxValue;
                }
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "<<") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            rightMinValue >= 0 &&
            rightMaxValue < int64SignBitShiftCount &&
            leftMaxValue <= (ZR_TYPE_RANGE_INT64_MAX >> (unsigned int)rightMaxValue)) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = (TZrInt64)((TZrUInt64)leftMinValue << (unsigned int)rightMinValue);
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = (TZrInt64)((TZrUInt64)leftMaxValue << (unsigned int)rightMaxValue);
            }
            return ZR_TRUE;
        }

        if (strcmp(op, ">>") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            rightMinValue >= 0 &&
            rightMaxValue < int64SignBitShiftCount) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = (TZrInt64)((TZrUInt64)leftMinValue >> (unsigned int)rightMaxValue);
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = (TZrInt64)((TZrUInt64)leftMaxValue >> (unsigned int)rightMinValue);
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "^") == 0 &&
            type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
            if (!type_inference_bitwise_identity_expression_supported_nonnegative_range(
                        cs,
                        sameIdentifierExpression,
                        ZR_NULL,
                        ZR_NULL)) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = 0;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = 0;
            }
            return ZR_TRUE;
        }

        if ((strcmp(op, "&") == 0 || strcmp(op, "|") == 0) &&
            type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
            return type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    sameIdentifierExpression,
                    outMinValue,
                    outMaxValue);
        }

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                    leftMinValue,
                    leftMaxValue) &&
            type_inference_bitwise_identity_expression_signed_same_identifier_range(
                    cs,
                    binary->right,
                    &identityMinValue,
                    &identityMaxValue) &&
            identityMinValue >= 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                    rightMinValue,
                    rightMaxValue) &&
            type_inference_bitwise_identity_expression_signed_same_identifier_range(
                    cs,
                    binary->left,
                    &identityMinValue,
                    &identityMaxValue) &&
            identityMinValue >= 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
                    cs,
                    binary->left,
                    binary->right,
                    &identityMinValue,
                    &identityMaxValue) &&
            identityMinValue >= 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "&") == 0 &&
            ((type_inference_bitwise_identity_expression_is_exact_zero_value(
                      cs,
                      binary->left) &&
              type_inference_bitwise_identity_expression_supported_nonnegative_range(
                      cs,
                      binary->right,
                      ZR_NULL,
                      ZR_NULL)) ||
             (type_inference_bitwise_identity_expression_is_exact_zero_value(
                      cs,
                      binary->right) &&
              type_inference_bitwise_identity_expression_supported_nonnegative_range(
                      cs,
                      binary->left,
                      ZR_NULL,
                      ZR_NULL)))) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = 0;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = 0;
            }
            return ZR_TRUE;
        }

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            type_inference_bitwise_identity_mask_range(
                    leftMinValue,
                    leftMaxValue,
                    rightMinValue,
                    rightMaxValue,
                    &identityMinValue,
                    &identityMaxValue)) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = identityMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = identityMaxValue;
            }
            return ZR_TRUE;
        }

        if ((strcmp(op, "|") == 0 || strcmp(op, "^") == 0) &&
            type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->right,
                    outMinValue,
                    outMaxValue)) {
            return ZR_TRUE;
        }

        if ((strcmp(op, "|") == 0 || strcmp(op, "^") == 0) &&
            type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->right) &&
            type_inference_bitwise_identity_expression_supported_nonnegative_range(
                    cs,
                    binary->left,
                    outMinValue,
                    outMaxValue)) {
            return ZR_TRUE;
        }

        return ZR_FALSE;
    }

    binding = type_inference_bitwise_identity_expression_binding(cs, expression);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint ||
        binding->type.minValue < 0 ||
        binding->type.maxValue < binding->type.minValue) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = binding->type.minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = binding->type.maxValue;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_supported_zero_identity_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const TZrChar *op;

    if (type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (expression == ZR_NULL || expression->type != ZR_AST_BINARY_EXPRESSION) {
        return type_inference_bitwise_identity_expression_signed_same_identifier_range(
                cs,
                expression,
                outMinValue, outMaxValue);
    }

    binary = &expression->data.binaryExpression;
    op = binary->op.op;
    if (type_inference_bitwise_identity_operator_is(op, "&") &&
        type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(op, "|") &&
        type_inference_bitwise_identity_expression_signed_all_ones_or_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(op, "&") &&
        ((type_inference_bitwise_identity_expression_is_exact_zero_value(cs, binary->left) &&
          type_inference_bitwise_identity_expression_supported_zero_identity_range(
                  cs,
                  binary->right,
                  ZR_NULL,
                  ZR_NULL)) ||
         (type_inference_bitwise_identity_expression_is_exact_zero_value(cs, binary->right) &&
          type_inference_bitwise_identity_expression_supported_zero_identity_range(
                  cs,
                  binary->left,
                  ZR_NULL,
                  ZR_NULL)))) {
        if (outMinValue != ZR_NULL) { *outMinValue = 0; }
        if (outMaxValue != ZR_NULL) { *outMaxValue = 0; }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(op, "|") ||
        type_inference_bitwise_identity_operator_is(op, "^")) {
        if (type_inference_bitwise_identity_expression_is_exact_zero_value(cs, binary->left) &&
            type_inference_bitwise_identity_expression_supported_zero_identity_range(
                    cs,
                    binary->right,
                    outMinValue,
                    outMaxValue)) {
            return ZR_TRUE;
        }

        if (type_inference_bitwise_identity_expression_is_exact_zero_value(cs, binary->right) &&
            type_inference_bitwise_identity_expression_supported_zero_identity_range(
                    cs,
                    binary->left,
                    outMinValue,
                    outMaxValue)) {
            return ZR_TRUE;
        }
    }

    return type_inference_bitwise_identity_expression_signed_same_identifier_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_value_is_all_ones_mask(TZrInt64 value) {
    TZrUInt64 mask;

    if (value < 0) {
        return ZR_FALSE;
    }

    mask = (TZrUInt64)value;
    return (mask & (mask + 1u)) == 0;
}

static TZrBool type_inference_bitwise_identity_mask_range(TZrInt64 leftMinValue,
                                                          TZrInt64 leftMaxValue,
                                                          TZrInt64 rightMinValue,
                                                          TZrInt64 rightMaxValue,
                                                          TZrInt64 *outMinValue,
                                                          TZrInt64 *outMaxValue) {
    if (rightMinValue == rightMaxValue &&
        type_inference_bitwise_identity_value_is_all_ones_mask(rightMinValue) &&
        leftMinValue >= 0 &&
        leftMaxValue <= rightMinValue) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = leftMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = leftMaxValue;
        }
        return ZR_TRUE;
    }

    if (leftMinValue == leftMaxValue &&
        type_inference_bitwise_identity_value_is_all_ones_mask(leftMinValue) &&
        rightMinValue >= 0 &&
        rightMaxValue <= leftMinValue) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = rightMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = rightMaxValue;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void type_inference_bitwise_identity_set_exact_zero_range(SZrInferredType *result) {
    result->minValue = 0;
    result->maxValue = 0;
    result->hasRangeConstraint = ZR_TRUE;
    ZrParser_InferredType_ResetRangeSegments(result);
}

static void type_inference_bitwise_identity_set_range(SZrInferredType *result,
                                                      TZrInt64 minValue,
                                                      TZrInt64 maxValue) {
    result->minValue = minValue;
    result->maxValue = maxValue;
    result->hasRangeConstraint = ZR_TRUE;
    ZrParser_InferredType_ResetRangeSegments(result);
}

TZrBool type_inference_apply_bitwise_identity_range(SZrCompilerState *cs,
                                                    const TZrChar *op,
                                                    const SZrAstNode *left,
                                                    const SZrAstNode *right,
                                                    SZrInferredType *result) {
    TZrInt64 identityMinValue;
    TZrInt64 identityMaxValue;
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;
    const SZrAstNode *sameIdentifierExpression;

    if (op == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "^") == 0 &&
        type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                cs,
                left,
                right,
                ZR_NULL)) {
        type_inference_bitwise_identity_set_exact_zero_range(result);
        return ZR_TRUE;
    }

    if ((strcmp(op, "&") == 0 || strcmp(op, "|") == 0) &&
        type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                cs,
                left,
                right,
                &sameIdentifierExpression) &&
        type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                sameIdentifierExpression,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    if (strcmp(op, "&") == 0 &&
        type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
                cs,
                left,
                right,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    if (strcmp(op, "|") == 0 &&
        type_inference_bitwise_identity_expression_signed_all_ones_or_range(
                cs,
                left,
                right,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    if (strcmp(op, "&") == 0 &&
        ((type_inference_bitwise_identity_expression_is_exact_zero_value(cs, left) &&
          type_inference_bitwise_identity_expression_supported_zero_identity_range(
                  cs,
                  right,
                  ZR_NULL,
                  ZR_NULL)) ||
         (type_inference_bitwise_identity_expression_is_exact_zero_value(cs, right) &&
          type_inference_bitwise_identity_expression_supported_zero_identity_range(
                  cs,
                  left,
                  ZR_NULL,
                  ZR_NULL)))) {
        type_inference_bitwise_identity_set_exact_zero_range(result);
        return ZR_TRUE;
    }

    if (strcmp(op, "&") == 0 &&
        type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) &&
        type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue) &&
        type_inference_bitwise_identity_mask_range(
                leftMinValue,
                leftMaxValue,
                rightMinValue,
                rightMaxValue,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    if ((strcmp(op, "|") == 0 || strcmp(op, "^") == 0) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(cs, left) &&
        type_inference_bitwise_identity_expression_supported_zero_identity_range(
                cs,
                right,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    if ((strcmp(op, "|") == 0 || strcmp(op, "^") == 0) &&
        type_inference_bitwise_identity_expression_is_exact_zero_value(cs, right) &&
        type_inference_bitwise_identity_expression_supported_zero_identity_range(
                cs,
                left,
                &identityMinValue,
                &identityMaxValue)) {
        type_inference_bitwise_identity_set_range(result, identityMinValue, identityMaxValue);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
