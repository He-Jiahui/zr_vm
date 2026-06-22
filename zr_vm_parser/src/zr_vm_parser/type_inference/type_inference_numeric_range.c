#include "type_inference_numeric_range.h"

#include <stdint.h>
#include <string.h>

#include "type_inference_semantic_facts.h"

static TZrBool type_inference_numeric_type_has_signed_range(EZrValueType baseType,
                                                            TZrInt64 *outMinValue,
                                                            TZrInt64 *outMaxValue) {
    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (baseType) {
        case ZR_VALUE_TYPE_INT8:
            *outMinValue = ZR_TYPE_RANGE_INT8_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT8_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT16:
            *outMinValue = ZR_TYPE_RANGE_INT16_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT16_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT32:
            *outMinValue = ZR_TYPE_RANGE_INT32_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT32_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT64:
            *outMinValue = ZR_TYPE_RANGE_INT64_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT64_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT8_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT16:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT16_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT32:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT32_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT64_MAX;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_int64_add(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left > ZR_TYPE_RANGE_INT64_MAX - right) ||
        (right < 0 && left < ZR_TYPE_RANGE_INT64_MIN - right)) {
        return ZR_FALSE;
    }

    *outValue = left + right;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_sub(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left < ZR_TYPE_RANGE_INT64_MIN + right) ||
        (right < 0 && left > ZR_TYPE_RANGE_INT64_MAX + right)) {
        return ZR_FALSE;
    }

    *outValue = left - right;
    return ZR_TRUE;
}

static void type_inference_numeric_range_clear_segments(SZrInferredType *type) {
    if (type == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_ResetRangeSegments(type);
}

static TZrBool type_inference_numeric_range_singleton_value(const SZrInferredType *type,
                                                            TZrInt64 *outValue) {
    if (type == ZR_NULL ||
        outValue == ZR_NULL ||
        !type->hasRangeConstraint) {
        return ZR_FALSE;
    }

    if (type->rangeSegmentCount == 1) {
        if (type->rangeSegments[0].minValue == type->rangeSegments[0].maxValue) {
            *outValue = type->rangeSegments[0].minValue;
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (type->rangeSegmentCount == 0 && type->minValue == type->maxValue) {
        *outValue = type->minValue;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_inference_int64_mul(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    long double product;

    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    product = (long double)left * (long double)right;
    if (product < (long double)ZR_TYPE_RANGE_INT64_MIN ||
        product > (long double)ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)product;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_div(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL || right == 0) {
        return ZR_FALSE;
    }

    if (left == ZR_TYPE_RANGE_INT64_MIN && right == -1) {
        return ZR_FALSE;
    }

    *outValue = left / right;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_mod(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL || right == 0) {
        return ZR_FALSE;
    }

    if (left == ZR_TYPE_RANGE_INT64_MIN && right == -1) {
        return ZR_FALSE;
    }

    *outValue = left % right;
    return ZR_TRUE;
}

TZrBool type_inference_numeric_range_checked_int64_binary_result(const TZrChar *op,
                                                                 TZrInt64 left,
                                                                 TZrInt64 right,
                                                                 TZrInt64 *outValue) {
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0) {
        return type_inference_int64_add(left, right, outValue);
    }
    if (strcmp(op, "-") == 0) {
        return type_inference_int64_sub(left, right, outValue);
    }
    if (strcmp(op, "*") == 0) {
        return type_inference_int64_mul(left, right, outValue);
    }
    if (strcmp(op, "/") == 0) {
        return type_inference_int64_div(left, right, outValue);
    }
    if (strcmp(op, "%") == 0) {
        return type_inference_int64_mod(left, right, outValue);
    }

    return ZR_FALSE;
}

static TZrBool type_inference_int64_mul_range(TZrInt64 leftMin,
                                              TZrInt64 leftMax,
                                              TZrInt64 rightMin,
                                              TZrInt64 rightMax,
                                              TZrInt64 *outMinValue,
                                              TZrInt64 *outMaxValue) {
    TZrInt64 products[4];
    TZrSize index;

    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!type_inference_int64_mul(leftMin, rightMin, &products[0]) ||
        !type_inference_int64_mul(leftMin, rightMax, &products[1]) ||
        !type_inference_int64_mul(leftMax, rightMin, &products[2]) ||
        !type_inference_int64_mul(leftMax, rightMax, &products[3])) {
        return ZR_FALSE;
    }

    *outMinValue = products[0];
    *outMaxValue = products[0];
    for (index = 1; index < 4; index++) {
        if (products[index] < *outMinValue) {
            *outMinValue = products[index];
        }
        if (products[index] > *outMaxValue) {
            *outMaxValue = products[index];
        }
    }

    return ZR_TRUE;
}

static TZrBool type_inference_int64_div_range(TZrInt64 leftMin,
                                              TZrInt64 leftMax,
                                              TZrInt64 rightMin,
                                              TZrInt64 rightMax,
                                              TZrInt64 *outMinValue,
                                              TZrInt64 *outMaxValue) {
    TZrInt64 quotients[4];
    TZrSize index;

    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rightMin <= 0 && rightMax >= 0) {
        return ZR_FALSE;
    }

    if (!type_inference_int64_div(leftMin, rightMin, &quotients[0]) ||
        !type_inference_int64_div(leftMin, rightMax, &quotients[1]) ||
        !type_inference_int64_div(leftMax, rightMin, &quotients[2]) ||
        !type_inference_int64_div(leftMax, rightMax, &quotients[3])) {
        return ZR_FALSE;
    }

    *outMinValue = quotients[0];
    *outMaxValue = quotients[0];
    for (index = 1; index < 4; index++) {
        if (quotients[index] < *outMinValue) {
            *outMinValue = quotients[index];
        }
        if (quotients[index] > *outMaxValue) {
            *outMaxValue = quotients[index];
        }
    }

    return ZR_TRUE;
}

static TZrUInt64 type_inference_int64_abs_magnitude(TZrInt64 value) {
    if (value >= 0) {
        return (TZrUInt64)value;
    }

    if (value == ZR_TYPE_RANGE_INT64_MIN) {
        return ((TZrUInt64)ZR_TYPE_RANGE_INT64_MAX) + 1u;
    }

    return (TZrUInt64)(-value);
}

static TZrBool type_inference_int64_mod_range(TZrInt64 leftMin,
                                              TZrInt64 leftMax,
                                              TZrInt64 rightMin,
                                              TZrInt64 rightMax,
                                              TZrInt64 *outMinValue,
                                              TZrInt64 *outMaxValue) {
    TZrUInt64 minAbsDivisor;
    TZrUInt64 maxAbsDivisor;
    TZrUInt64 remainderBound;

    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (rightMin <= 0 && rightMax >= 0) {
        return ZR_FALSE;
    }

    if (leftMin <= ZR_TYPE_RANGE_INT64_MIN &&
        leftMax >= ZR_TYPE_RANGE_INT64_MIN &&
        rightMin <= -1 &&
        rightMax >= -1) {
        return ZR_FALSE;
    }

    minAbsDivisor = type_inference_int64_abs_magnitude(rightMin);
    maxAbsDivisor = type_inference_int64_abs_magnitude(rightMax);
    if (minAbsDivisor > maxAbsDivisor) {
        remainderBound = minAbsDivisor;
        minAbsDivisor = maxAbsDivisor;
        maxAbsDivisor = remainderBound;
    }

    if (maxAbsDivisor == 0u) {
        return ZR_FALSE;
    }

    remainderBound = maxAbsDivisor > (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX
                             ? (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX
                             : maxAbsDivisor - 1u;

    if (leftMin >= 0) {
        *outMinValue = 0;
        *outMaxValue = (TZrInt64)remainderBound;
    } else if (leftMax <= 0) {
        *outMinValue = -(TZrInt64)remainderBound;
        *outMaxValue = 0;
    } else {
        *outMinValue = -(TZrInt64)remainderBound;
        *outMaxValue = (TZrInt64)remainderBound;
    }

    return ZR_TRUE;
}

void type_inference_apply_primitive_numeric_range(SZrInferredType *type) {
    if (type == ZR_NULL || !ZR_VALUE_IS_TYPE_NUMBER(type->baseType)) {
        return;
    }

    if (type_inference_numeric_type_has_signed_range(type->baseType, &type->minValue, &type->maxValue)) {
        type->hasRangeConstraint = ZR_TRUE;
        type_inference_numeric_range_clear_segments(type);
    }
}

static void type_inference_apply_shifted_numeric_segments(const TZrChar *op,
                                                          SZrState *state,
                                                          const SZrInferredType *leftType,
                                                          const SZrInferredType *rightType,
                                                          SZrInferredType *result) {
    const SZrInferredType *segmentedType;
    TZrInt64 scalarValue;
    TZrSize index;

    if (op == ZR_NULL ||
        state == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        result == ZR_NULL ||
        result->rangeSegmentCount != 0 ||
        (strcmp(op, "+") != 0 && strcmp(op, "-") != 0)) {
        return;
    }

    if (leftType->rangeSegmentCount > 0 &&
        type_inference_numeric_range_singleton_value(rightType, &scalarValue)) {
        segmentedType = leftType;
    } else if (rightType->rangeSegmentCount > 0 &&
               type_inference_numeric_range_singleton_value(leftType, &scalarValue) &&
               strcmp(op, "+") == 0) {
        segmentedType = rightType;
    } else {
        return;
    }

    for (index = 0; index < segmentedType->rangeSegmentCount; index++) {
        const SZrNumericRangeSegment *segment;
        TZrInt64 minValue;
        TZrInt64 maxValue;

        segment = ZrParser_InferredType_RangeSegmentAt(segmentedType, index);
        if (segment == ZR_NULL) {
            type_inference_numeric_range_clear_segments(result);
            return;
        }

        if (strcmp(op, "+") == 0) {
            if (!type_inference_int64_add(segment->minValue, scalarValue, &minValue) ||
                !type_inference_int64_add(segment->maxValue, scalarValue, &maxValue)) {
                type_inference_numeric_range_clear_segments(result);
                return;
            }
        } else {
            if (!type_inference_int64_sub(segment->minValue, scalarValue, &minValue) ||
                !type_inference_int64_sub(segment->maxValue, scalarValue, &maxValue)) {
                type_inference_numeric_range_clear_segments(result);
                return;
            }
        }

        if (!ZrParser_InferredType_AppendRangeSegment(state, result, minValue, maxValue)) {
            type_inference_numeric_range_clear_segments(result);
            return;
        }
    }
}

void type_inference_apply_binary_numeric_range(SZrState *state,
                                               const TZrChar *op,
                                               const SZrInferredType *leftType,
                                               const SZrInferredType *rightType,
                                               SZrInferredType *result) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (op == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        result == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(result->baseType) ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return;
    }
    type_inference_numeric_range_clear_segments(result);

    if (strcmp(op, "+") == 0) {
        if (!type_inference_int64_add(leftType->minValue, rightType->minValue, &minValue) ||
            !type_inference_int64_add(leftType->maxValue, rightType->maxValue, &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
        type_inference_apply_shifted_numeric_segments(op, state, leftType, rightType, result);
    } else if (strcmp(op, "-") == 0) {
        if (!type_inference_int64_sub(leftType->minValue, rightType->maxValue, &minValue) ||
            !type_inference_int64_sub(leftType->maxValue, rightType->minValue, &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
        type_inference_apply_shifted_numeric_segments(op, state, leftType, rightType, result);
    } else if (strcmp(op, "*") == 0) {
        if (!type_inference_int64_mul_range(leftType->minValue,
                                            leftType->maxValue,
                                            rightType->minValue,
                                            rightType->maxValue,
                                            &minValue,
                                            &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    } else if (strcmp(op, "/") == 0) {
        if (!type_inference_int64_div_range(leftType->minValue,
                                            leftType->maxValue,
                                            rightType->minValue,
                                            rightType->maxValue,
                                            &minValue,
                                            &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    } else if (strcmp(op, "%") == 0) {
        if (!type_inference_int64_mod_range(leftType->minValue,
                                            leftType->maxValue,
                                            rightType->minValue,
                                            rightType->maxValue,
                                            &minValue,
                                            &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    }
}

void type_inference_apply_unary_numeric_range(SZrState *state,
                                              const TZrChar *op,
                                              const SZrInferredType *operandType,
                                              SZrInferredType *result) {
    if (op == ZR_NULL || operandType == ZR_NULL || result == ZR_NULL || !operandType->hasRangeConstraint) {
        return;
    }

    if (strcmp(op, "+") == 0) {
        result->minValue = operandType->minValue;
        result->maxValue = operandType->maxValue;
        result->hasRangeConstraint = ZR_TRUE;
        ZrParser_InferredType_CopyRangeSegments(state, result, operandType);
    } else if (strcmp(op, "-") == 0) {
        TZrInt64 minValue;
        TZrInt64 maxValue;

        if (!type_inference_int64_sub(0, operandType->maxValue, &minValue) ||
            !type_inference_int64_sub(0, operandType->minValue, &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            type_inference_numeric_range_clear_segments(result);
            return;
        }

        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
        type_inference_numeric_range_clear_segments(result);
    }
}
