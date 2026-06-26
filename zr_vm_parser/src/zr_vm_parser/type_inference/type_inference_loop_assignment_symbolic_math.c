#include "type_inference_loop_assignment_symbolic_math.h"

TZrBool loop_assignment_sequence_symbolic_int64_add(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue) {
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

TZrBool loop_assignment_sequence_symbolic_int64_mul(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left == 0 || right == 0) {
        *outValue = 0;
        return ZR_TRUE;
    }
    if (left == -1) {
        if (right == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        *outValue = -right;
        return ZR_TRUE;
    }
    if (right == -1) {
        if (left == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        *outValue = -left;
        return ZR_TRUE;
    }
    if ((left > 0 && right > 0 && left > ZR_TYPE_RANGE_INT64_MAX / right) ||
        (left > 0 && right < 0 && right < ZR_TYPE_RANGE_INT64_MIN / left) ||
        (left < 0 && right > 0 && left < ZR_TYPE_RANGE_INT64_MIN / right) ||
        (left < 0 && right < 0 && right < ZR_TYPE_RANGE_INT64_MAX / left)) {
        return ZR_FALSE;
    }

    *outValue = left * right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_div(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue) {
    if (outValue == ZR_NULL || right == 0) {
        return ZR_FALSE;
    }
    if (left == ZR_TYPE_RANGE_INT64_MIN && right == -1) {
        return ZR_FALSE;
    }

    *outValue = left / right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_mod(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue) {
    if (outValue == ZR_NULL || right == 0) {
        return ZR_FALSE;
    }
    if (left == ZR_TYPE_RANGE_INT64_MIN && right == -1) {
        return ZR_FALSE;
    }

    *outValue = left % right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_shift_left(TZrInt64 left,
                                                           TZrInt64 right,
                                                           TZrInt64 *outValue) {
    if (outValue == ZR_NULL || left < 0 || right < 0 || right >= 63) {
        return ZR_FALSE;
    }
    if (left > (ZR_TYPE_RANGE_INT64_MAX >> (unsigned int)right)) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)((TZrUInt64)left << (unsigned int)right);
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_shift_right(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL || left < 0 || right < 0 || right >= 63) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)((TZrUInt64)left >> (unsigned int)right);
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_and(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL || left < 0 || right < 0) {
        return ZR_FALSE;
    }

    *outValue = left & right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_or(TZrInt64 left,
                                                           TZrInt64 right,
                                                           TZrInt64 *outValue) {
    if (outValue == ZR_NULL || left < 0 || right < 0) {
        return ZR_FALSE;
    }

    *outValue = left | right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_xor(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL || left < 0 || right < 0) {
        return ZR_FALSE;
    }

    *outValue = left ^ right;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_range_add(TZrInt64 leftMin,
                                                          TZrInt64 leftMax,
                                                          TZrInt64 rightMin,
                                                          TZrInt64 rightMax,
                                                          TZrInt64 *outMin,
                                                          TZrInt64 *outMax) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (outMin == ZR_NULL || outMax == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!loop_assignment_sequence_symbolic_int64_add(leftMin, rightMin, &minValue) ||
        !loop_assignment_sequence_symbolic_int64_add(leftMax, rightMax, &maxValue)) {
        return ZR_FALSE;
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_int64_range_mul(TZrInt64 leftMin,
                                                          TZrInt64 leftMax,
                                                          TZrInt64 rightMin,
                                                          TZrInt64 rightMax,
                                                          TZrInt64 *outMin,
                                                          TZrInt64 *outMax) {
    TZrInt64 products[4];
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrSize index;

    if (outMin == ZR_NULL || outMax == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!loop_assignment_sequence_symbolic_int64_mul(leftMin, rightMin, &products[0]) ||
        !loop_assignment_sequence_symbolic_int64_mul(leftMin, rightMax, &products[1]) ||
        !loop_assignment_sequence_symbolic_int64_mul(leftMax, rightMin, &products[2]) ||
        !loop_assignment_sequence_symbolic_int64_mul(leftMax, rightMax, &products[3])) {
        return ZR_FALSE;
    }

    minValue = products[0];
    maxValue = products[0];
    for (index = 1; index < 4; index++) {
        if (products[index] < minValue) {
            minValue = products[index];
        }
        if (products[index] > maxValue) {
            maxValue = products[index];
        }
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

TZrBool loop_assignment_sequence_symbolic_add_signed_range(TZrInt64 currentMin,
                                                           TZrInt64 currentMax,
                                                           TZrInt64 termMin,
                                                           TZrInt64 termMax,
                                                           TZrInt32 sign,
                                                           TZrInt64 *outMin,
                                                           TZrInt64 *outMax) {
    TZrInt64 signedTermMin;
    TZrInt64 signedTermMax;

    if (outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (sign > 0) {
        signedTermMin = termMin;
        signedTermMax = termMax;
    } else {
        if (termMin == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        signedTermMin = -termMax;
        signedTermMax = -termMin;
    }

    return loop_assignment_sequence_symbolic_int64_range_add(
            currentMin,
            currentMax,
            signedTermMin,
            signedTermMax,
            outMin,
            outMax);
}
