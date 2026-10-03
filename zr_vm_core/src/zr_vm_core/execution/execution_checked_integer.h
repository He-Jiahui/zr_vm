#ifndef ZR_EXECUTION_CHECKED_INTEGER_H
#define ZR_EXECUTION_CHECKED_INTEGER_H

#include <stdint.h>
#include "zr_vm_core/conf.h"

/* Check before multiplying. Failure leaves output untouched, including when
 * output aliases a caller's operand. No compiler overflow mode is required. */
static inline TZrBool execution_checked_i64_multiply(
        TZrInt64 left, TZrInt64 right, TZrInt64 *output) {
    if (output == ZR_NULL) return ZR_FALSE;
    if (left != 0 && right != 0) {
        if ((left == INT64_MIN && right == -1) ||
                (right == INT64_MIN && left == -1)) return ZR_FALSE;
        if (left > 0) {
            if (right > 0 && left > INT64_MAX / right) return ZR_FALSE;
            if (right < 0 && right < INT64_MIN / left) return ZR_FALSE;
        } else if (right > 0) {
            if (left < INT64_MIN / right) return ZR_FALSE;
        } else if (left < INT64_MAX / right) {
            return ZR_FALSE;
        }
    }
    *output = left * right;
    return ZR_TRUE;
}

/* Signed division truncates toward zero. Check both undefined host cases
 * before evaluating /; failure preserves output, including operand aliases. */
static inline TZrBool execution_checked_i64_divide(
        TZrInt64 left, TZrInt64 right, TZrInt64 *output) {
    if (output == ZR_NULL || right == 0 ||
            (left == INT64_MIN && right == -1)) return ZR_FALSE;
    *output = left / right;
    return ZR_TRUE;
}

#endif
