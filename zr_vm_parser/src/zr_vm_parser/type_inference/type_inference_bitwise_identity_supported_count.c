#include "type_inference_bitwise_identity_direct_range.h"
#include "type_inference_bitwise_identity_supported_count_all_ones_side.h"
#include "type_inference_bitwise_identity_supported_count_wrapped.h"

#include <limits.h>
#include <string.h>

static TZrBool type_inference_bitwise_identity_value_is_all_ones_mask(TZrInt64 value) {
    TZrUInt64 mask;

    if (value < 0) {
        return ZR_FALSE;
    }

    mask = (TZrUInt64)value;
    return (mask & (mask + 1u)) == 0;
}

TZrBool type_inference_bitwise_identity_mask_range(TZrInt64 leftMinValue,
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

TZrBool type_inference_bitwise_identity_expression_supported_nonnegative_range(
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

    if (type_inference_bitwise_identity_expression_zero_minus_unary_minus_supported_nonnegative_range(
                cs,
                expression,
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

    if (type_inference_bitwise_identity_expression_double_zero_minus_supported_nonnegative_range(
                cs,
                expression,
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

    if ((type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
                 cs,
                 expression,
                 &identityMinValue,
                 &identityMaxValue) ||
         type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
                 cs,
                 expression,
                 &identityMinValue,
                 &identityMaxValue)) &&
        identityMinValue >= 0) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = identityMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = identityMaxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
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
            type_inference_bitwise_identity_expression_double_unary_minus_supported_nonnegative_range(
                    cs,
                    expression,
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
        if (op != ZR_NULL &&
            strcmp(op, "-") == 0 &&
            type_inference_bitwise_identity_expression_unary_minus_bitwise_not_supported_nonnegative_range(
                    cs,
                    expression,
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
        if (op != ZR_NULL &&
            strcmp(op, "-") == 0 &&
            type_inference_bitwise_identity_expression_unary_minus_zero_minus_supported_nonnegative_range(
                    cs,
                    expression,
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
            type_inference_bitwise_identity_expression_double_bitwise_not_supported_nonnegative_range(
                    cs,
                    expression,
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
        if (op != ZR_NULL &&
            strcmp(op, "~") == 0 &&
            type_inference_bitwise_identity_expression_bitwise_not_unary_minus_supported_nonnegative_range(
                    cs,
                    expression,
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
        if (op != ZR_NULL &&
            strcmp(op, "~") == 0 &&
            type_inference_bitwise_identity_expression_bitwise_not_zero_minus_supported_nonnegative_range(
                    cs,
                    expression,
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
            type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                    cs,
                    binary->left,
                    ZR_NULL,
                    ZR_NULL) &&
            type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                    cs,
                    binary->right,
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

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                    cs,
                    binary->right,
                    ZR_NULL,
                    ZR_NULL) &&
            type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                    cs,
                    binary->left,
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

        if (strcmp(op, "&") == 0 &&
            type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            (type_inference_bitwise_identity_expression_signed_same_identifier_range(
                     cs,
                     binary->right,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_zero_minus_signed_same_identifier_range(
                     cs,
                     binary->right,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_unary_minus_zero_minus_signed_same_identifier_range(
                     cs,
                     binary->right,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                     cs,
                     binary->right,
                     &identityMinValue,
                     &identityMaxValue)) &&
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
            type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            (type_inference_bitwise_identity_expression_signed_same_identifier_range(
                     cs,
                     binary->left,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_zero_minus_signed_same_identifier_range(
                     cs,
                     binary->left,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_unary_minus_zero_minus_signed_same_identifier_range(
                     cs,
                     binary->left,
                     &identityMinValue,
                     &identityMaxValue) ||
             type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                     cs,
                     binary->left,
                     &identityMinValue,
                     &identityMaxValue)) &&
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
