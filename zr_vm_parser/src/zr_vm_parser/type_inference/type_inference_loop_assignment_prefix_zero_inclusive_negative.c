#include "type_inference_loop_assignment_prefix_zero_inclusive_negative.h"
#include "type_inference_loop_assignment_prefix_offset.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool prefix_zero_inclusive_negative_operator_is(
        const TZrChar *actual,
        const TZrChar *expected) {
    return actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) == 0;
}

static TZrBool prefix_zero_inclusive_negative_add_ranges(
        TZrInt64 leftMinValue,
        TZrInt64 leftMaxValue,
        TZrInt64 rightMinValue,
        TZrInt64 rightMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (leftMinValue > leftMaxValue ||
        rightMinValue > rightMaxValue ||
        leftMinValue >= 0 ||
        rightMinValue >= 0 ||
        leftMaxValue != 0 ||
        rightMaxValue != 0 ||
        leftMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        rightMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        leftMinValue <= ZR_TYPE_RANGE_INT64_MIN - rightMinValue) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = leftMinValue + rightMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = 0;
    }
    return ZR_TRUE;
}

static TZrBool prefix_zero_inclusive_negative_multiply_by_offset(
        TZrInt64 negativeMinValue,
        TZrInt64 negativeMaxValue,
        TZrInt64 offsetMinValue,
        TZrInt64 offsetMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 productMinValue;

    if (negativeMinValue > negativeMaxValue ||
        offsetMinValue > offsetMaxValue ||
        negativeMinValue >= 0 ||
        negativeMaxValue != 0 ||
        negativeMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        offsetMinValue < 0 ||
        offsetMaxValue < 0) {
        return ZR_FALSE;
    }

    if (offsetMaxValue == 0) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = 0;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = 0;
        }
        return ZR_TRUE;
    }

    if (negativeMinValue < ZR_TYPE_RANGE_INT64_MIN / offsetMaxValue) {
        return ZR_FALSE;
    }

    productMinValue = negativeMinValue * offsetMaxValue;
    if (productMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }
    if (outMinValue != ZR_NULL) {
        *outMinValue = productMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = 0;
    }
    return ZR_TRUE;
}

static TZrBool prefix_zero_inclusive_negative_subtract_offset(
        TZrInt64 negativeMinValue,
        TZrInt64 negativeMaxValue,
        TZrInt64 offsetMinValue,
        TZrInt64 offsetMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 resultMinValue;

    if (negativeMinValue > negativeMaxValue ||
        offsetMinValue > offsetMaxValue ||
        negativeMinValue >= 0 ||
        negativeMaxValue != 0 ||
        negativeMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        offsetMinValue != 0 ||
        offsetMaxValue <= 0 ||
        negativeMinValue < ZR_TYPE_RANGE_INT64_MIN + offsetMaxValue) {
        return ZR_FALSE;
    }

    resultMinValue = negativeMinValue - offsetMaxValue;
    if (resultMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }
    if (outMinValue != ZR_NULL) {
        *outMinValue = resultMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = 0;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrTypeBinding *binding;
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;
    TZrInt64 positiveMinValue;
    TZrInt64 positiveMaxValue;

    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (prefix_zero_inclusive_negative_operator_is(unary->op.op, "+")) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                    cs,
                    unary->argument,
                    outMinValue,
                    outMaxValue);
        }
        if (prefix_zero_inclusive_negative_operator_is(unary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    unary->argument,
                    &positiveMinValue,
                    &positiveMaxValue) &&
            positiveMinValue == 0 &&
            positiveMaxValue > 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = -positiveMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = 0;
            }
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (prefix_zero_inclusive_negative_operator_is(binary->op.op, "+")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                        cs,
                        binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->right,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                        cs,
                        binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) &&
                prefix_zero_inclusive_negative_add_ranges(
                        leftMinValue,
                        leftMaxValue,
                        rightMinValue,
                        rightMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
        }
        if (prefix_zero_inclusive_negative_operator_is(binary->op.op, "-")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                        cs,
                        binary->left) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &positiveMinValue,
                        &positiveMaxValue) &&
                positiveMinValue == 0 &&
                positiveMaxValue > 0) {
                if (outMinValue != ZR_NULL) {
                    *outMinValue = -positiveMaxValue;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = 0;
                }
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &positiveMinValue,
                        &positiveMaxValue) &&
                prefix_zero_inclusive_negative_subtract_offset(
                        leftMinValue,
                        leftMaxValue,
                        positiveMinValue,
                        positiveMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
        }
        if (prefix_zero_inclusive_negative_operator_is(binary->op.op, "*")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &positiveMinValue,
                        &positiveMaxValue) &&
                prefix_zero_inclusive_negative_multiply_by_offset(
                        leftMinValue,
                        leftMaxValue,
                        positiveMinValue,
                        positiveMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        &positiveMinValue,
                        &positiveMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) &&
                prefix_zero_inclusive_negative_multiply_by_offset(
                        rightMinValue,
                        rightMaxValue,
                        positiveMinValue,
                        positiveMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }
    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        expression->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv,
            expression->data.identifier.name);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint ||
        binding->type.minValue > binding->type.maxValue ||
        binding->type.minValue >= 0 ||
        binding->type.maxValue != 0 ||
        binding->type.minValue == ZR_TYPE_RANGE_INT64_MIN) {
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
