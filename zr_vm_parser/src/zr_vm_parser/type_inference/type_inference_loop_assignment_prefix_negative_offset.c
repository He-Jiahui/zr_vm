#include "type_inference_loop_assignment_prefix_offset.h"
#include "type_inference_loop_assignment_prefix_zero_inclusive_negative.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

static TZrBool prefix_negative_offset_operator_is(
        const TZrChar *actual,
        const TZrChar *expected) {
    return actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) == 0;
}

static TZrBool prefix_negative_offset_add_strict_negative_ranges(
        TZrInt64 leftMinValue,
        TZrInt64 leftMaxValue,
        TZrInt64 rightMinValue,
        TZrInt64 rightMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (leftMinValue > leftMaxValue ||
        rightMinValue > rightMaxValue ||
        leftMaxValue >= 0 ||
        rightMaxValue >= 0 ||
        leftMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        rightMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        leftMinValue <= ZR_TYPE_RANGE_INT64_MIN - rightMinValue) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = leftMinValue + rightMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = leftMaxValue + rightMaxValue;
    }
    return ZR_TRUE;
}

static TZrBool prefix_negative_offset_add_zero_inclusive_negative_and_strict_negative_ranges(
        TZrInt64 zeroInclusiveMinValue,
        TZrInt64 zeroInclusiveMaxValue,
        TZrInt64 strictMinValue,
        TZrInt64 strictMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 resultMinValue;
    TZrInt64 resultMaxValue;

    if (zeroInclusiveMinValue > zeroInclusiveMaxValue ||
        strictMinValue > strictMaxValue ||
        zeroInclusiveMinValue >= 0 ||
        zeroInclusiveMaxValue != 0 ||
        strictMaxValue >= 0 ||
        zeroInclusiveMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        strictMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        zeroInclusiveMinValue <= ZR_TYPE_RANGE_INT64_MIN - strictMinValue) {
        return ZR_FALSE;
    }

    resultMinValue = zeroInclusiveMinValue + strictMinValue;
    resultMaxValue = zeroInclusiveMaxValue + strictMaxValue;
    if (resultMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        resultMaxValue >= 0) {
        return ZR_FALSE;
    }
    if (outMinValue != ZR_NULL) {
        *outMinValue = resultMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = resultMaxValue;
    }
    return ZR_TRUE;
}

static TZrBool prefix_negative_offset_subtract_strict_positive_from_zero_inclusive_negative_range(
        TZrInt64 negativeMinValue,
        TZrInt64 negativeMaxValue,
        TZrInt64 positiveMinValue,
        TZrInt64 positiveMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 resultMinValue;
    TZrInt64 resultMaxValue;

    if (negativeMinValue > negativeMaxValue ||
        positiveMinValue > positiveMaxValue ||
        negativeMinValue >= 0 ||
        negativeMaxValue != 0 ||
        negativeMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        positiveMinValue <= 0 ||
        negativeMinValue < ZR_TYPE_RANGE_INT64_MIN + positiveMaxValue) {
        return ZR_FALSE;
    }

    resultMinValue = negativeMinValue - positiveMaxValue;
    resultMaxValue = negativeMaxValue - positiveMinValue;
    if (resultMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        resultMaxValue >= 0) {
        return ZR_FALSE;
    }
    if (outMinValue != ZR_NULL) {
        *outMinValue = resultMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = resultMaxValue;
    }
    return ZR_TRUE;
}

static TZrBool prefix_negative_offset_multiply_strict_negative_by_positive_offset_range(
        TZrInt64 negativeMinValue,
        TZrInt64 negativeMaxValue,
        TZrInt64 positiveMinValue,
        TZrInt64 positiveMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 resultMinValue;
    TZrInt64 resultMaxValue;

    if (negativeMinValue > negativeMaxValue ||
        positiveMinValue > positiveMaxValue ||
        negativeMaxValue >= 0 ||
        positiveMinValue <= 0 ||
        negativeMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        negativeMinValue < ZR_TYPE_RANGE_INT64_MIN / positiveMaxValue ||
        negativeMaxValue < ZR_TYPE_RANGE_INT64_MIN / positiveMinValue) {
        return ZR_FALSE;
    }

    resultMinValue = negativeMinValue * positiveMaxValue;
    resultMaxValue = negativeMaxValue * positiveMinValue;
    if (resultMinValue == ZR_TYPE_RANGE_INT64_MIN ||
        resultMaxValue >= 0) {
        return ZR_FALSE;
    }
    if (outMinValue != ZR_NULL) {
        *outMinValue = resultMinValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = resultMaxValue;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrTypeBinding *binding;
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 positiveMinValue;
    TZrInt64 positiveMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (prefix_negative_offset_operator_is(unary->op.op, "+")) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    unary->argument,
                    outMinValue,
                    outMaxValue);
        }
        if (prefix_negative_offset_operator_is(unary->op.op, "-")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    unary->argument,
                    &positiveMinValue,
                    &positiveMaxValue) ||
                positiveMinValue <= 0) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = -positiveMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = -positiveMinValue;
            }
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (prefix_negative_offset_operator_is(binary->op.op, "+")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue)) {
                return prefix_negative_offset_add_strict_negative_ranges(
                        leftMinValue,
                        leftMaxValue,
                        rightMinValue,
                        rightMaxValue,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->right,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) &&
                prefix_negative_offset_add_zero_inclusive_negative_and_strict_negative_ranges(
                        leftMinValue,
                        leftMaxValue,
                        rightMinValue,
                        rightMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) &&
                prefix_negative_offset_add_zero_inclusive_negative_and_strict_negative_ranges(
                        rightMinValue,
                        rightMaxValue,
                        leftMinValue,
                        leftMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        if (prefix_negative_offset_operator_is(binary->op.op, "*")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &positiveMinValue,
                        &positiveMaxValue) &&
                prefix_negative_offset_multiply_strict_negative_by_positive_offset_range(
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
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) &&
                prefix_negative_offset_multiply_strict_negative_by_positive_offset_range(
                        rightMinValue,
                        rightMaxValue,
                        positiveMinValue,
                        positiveMaxValue,
                        outMinValue,
                        outMaxValue)) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        if (prefix_negative_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    binary->left,
                    outMinValue,
                    outMaxValue);
        }
        if (prefix_negative_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left) &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->right,
                    &positiveMinValue,
                    &positiveMaxValue) &&
            positiveMinValue > 0) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = -positiveMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = -positiveMinValue;
            }
            return ZR_TRUE;
        }
        if (prefix_negative_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->right,
                    &positiveMinValue,
                    &positiveMaxValue) &&
            prefix_negative_offset_subtract_strict_positive_from_zero_inclusive_negative_range(
                    leftMinValue,
                    leftMaxValue,
                    positiveMinValue,
                    positiveMaxValue,
                    outMinValue,
                    outMaxValue)) {
            return ZR_TRUE;
        }
        if (prefix_negative_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->right,
                    &rightMinValue,
                    &rightMaxValue) &&
            leftMaxValue < rightMinValue) {
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue - rightMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue - rightMinValue;
            }
            return ZR_TRUE;
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
        binding->type.maxValue >= 0 ||
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
