//
// Created by HeJiahui on 2025/6/21.
//

#ifndef ZR_VM_CORE_MATH_H
#define ZR_VM_CORE_MATH_H
#include "zr_vm_core/conf.h"

/** @brief 返回较小表达式的值；两项先比较，被选中的表达式会再求值一次。
 * @pre 实参不应带自增、分配等副作用；混合类型按 C 的比较与条件表达式规则转换。 */
#define ZR_MATH_MIN(x, y) ((x) < (y) ? (x) : (y))
/** @brief 返回较大表达式的值，供数组增长下限选择使用。
 * @pre 被选中的实参会重复求值；调用方须传无副作用且类型可比较的表达式。 */
#define ZR_MATH_MAX(x, y) ((x) > (y) ? (x) : (y))
/** @brief 以平方递推计算无符号整数幂，供解释器与 AOT 的 typed power 使用。
 * @return 指数为零时返回 1；正指数结果溢出时返回 0。零结果与溢出共用返回值。 */
ZR_FORCE_INLINE TZrUInt64 ZrCore_Math_UIntPower(TZrUInt64 base, TZrUInt64 exponent) {
    if (ZR_UNLIKELY(exponent == 0)) {
        return 1;
    }
    if (ZR_UNLIKELY(base == 0)) {
        return 0;
    }
    TZrUInt64 result = 1;
    while (exponent > 0) {
        if (exponent & 1) {
            if (ZR_UNLIKELY(result > ZR_UINT_MAX / base)) {
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

/** @brief 计算有符号整数幂；解释器与 AOT 在调用前另行检查语言级幂定义域。
 * @return 负指数或正指数结果溢出时返回 0；零指数返回 1。溢出与合法零结果共用返回值。 */
ZR_FORCE_INLINE TZrInt64 ZrCore_Math_IntPower(TZrInt64 base, TZrInt64 exponent) {
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
    TZrBool isNegative = base < 0;
    /* Unsigned subtraction computes |base| modulo 2^64, including INT64_MIN. */
    TZrUInt64 absBase = isNegative ? (TZrUInt64) 0 - (TZrUInt64) base : (TZrUInt64) base;
    TZrUInt64 absResult = ZrCore_Math_UIntPower(absBase, exponent);

    if (ZR_UNLIKELY(absResult == 0 && absBase != 0)) {
        // overflow!
        return 0;
    }
    if (isNegative && (exponent & 1)) {
        if (ZR_UNLIKELY(absResult>(TZrUInt64)ZR_INT_MAX + 1)) {
            // overflow!
            return 0;
        }
        if (absResult == (TZrUInt64) ZR_INT_MAX + 1) {
            return ZR_INT_MIN;
        }
        return -(TZrInt64) absResult;
    } {
        if (ZR_UNLIKELY(absResult>(TZrUInt64)ZR_INT_MAX)) {
            // overflow!
            return 0;
        }
        return (TZrInt64) absResult;
    }
}


#endif //ZR_VM_CORE_MATH_H
