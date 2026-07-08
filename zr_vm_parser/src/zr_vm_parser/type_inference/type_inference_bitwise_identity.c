#include "type_inference_internal.h"
#include "type_inference_bitwise_identity_direct_range.h"

#include <limits.h>
#include <string.h>

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
