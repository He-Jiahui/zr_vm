//
// Created by HeJiahui on 2025/6/21.
//

#ifndef ZR_VM_CORE_MATH_H
#define ZR_VM_CORE_MATH_H
#include "zr_vm_core/conf.h"

#define ZR_MATH_MIN(x, y) ((x) < (y) ? (x) : (y))
#define ZR_MATH_MAX(x, y) ((x) > (y) ? (x) : (y))
ZR_FORCE_INLINE TUInt64 ZrMathUIntPower(TUInt64 base, TUInt64 exponent) {
    if (ZR_UNLIKELY(exponent == 0)) {
        return 1;
    }
    TUInt64 result = 1;
    while (exponent > 0) {
        if (exponent & 1) {
            if (ZR_UNLIKELY(base > ZR_UINT_MAX / base)) {
                // overflow!
                return 0;
            }
            result *= base;
        }
        exponent >>= 1;
        if (exponent == 0) {
            break;
        }
        if (ZR_UNLIKELY(base > ZR_UINT_MAX / base)) {
            // overflow!
            return 0;
        }
        base *= base;
    }
    return result;
}

ZR_FORCE_INLINE TInt64 ZrMathIntPower(TInt64 base, TInt64 exponent) {
    if (ZR_UNLIKELY(exponent < 0)) {
        return 0;
    }
    if (ZR_UNLIKELY(exponent == 0)) {
        return 1;
    }
    if (ZR_UNLIKELY(base == 0)) {
        return 0; // 0的正指数幂为0
    }
    if (ZR_UNLIKELY(base == 1)) {
        return 1;
    }
    if (ZR_UNLIKELY(base == -1)) {
        return (exponent & 1) ? -1 : 1;
    }
    TBool isNegative = base < 0;
    TUInt64 absBase = isNegative ? -base : base;
    TUInt64 absResult = ZrMathUIntPower(absBase, exponent);

    if (ZR_UNLIKELY(absResult == 0 && absBase != 0)) {
        // overflow!
        return 0;
    }
    if (isNegative && (exponent & 1)) {
        if (ZR_UNLIKELY(absResult>(TUInt64)ZR_INT_MAX + 1)) {
            // overflow!
            return 0;
        }
        return -(TInt64) absResult;
    } {
        if (ZR_UNLIKELY(absResult>(TUInt64)ZR_INT_MAX)) {
            // overflow!
            return 0;
        }
        return (TInt64) absResult;
    }
}


#endif //ZR_VM_CORE_MATH_H
