//
// Scalar math callbacks.
//

#include "zr_vm_lib_math/scalar.h"

/* 同构标量回调统一经 binding 读取参数并写结果；arity 由 registry 描述符限制。 */
#define ZR_MATH_UNARY(NAME, EXPR) \
    TZrBool NAME(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 value = 0.0; \
        if (!ZrLib_CallContext_ReadFloat(context, 0, &value)) return ZR_FALSE; \
        ZrLib_Value_SetFloat(context->state, result, (EXPR)); return ZR_TRUE; }
#define ZR_MATH_BINARY(NAME, EXPR) \
    TZrBool NAME(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 lhs = 0.0, rhs = 0.0; \
        if (!ZrLib_CallContext_ReadFloat(context, 0, &lhs) || !ZrLib_CallContext_ReadFloat(context, 1, &rhs)) return ZR_FALSE; \
        ZrLib_Value_SetFloat(context->state, result, (EXPR)); return ZR_TRUE; }

ZR_MATH_UNARY(ZrMath_Scalar_Abs, ZrMath_AbsFloat(value))
ZR_MATH_BINARY(ZrMath_Scalar_Min, lhs < rhs ? lhs : rhs)
ZR_MATH_BINARY(ZrMath_Scalar_Max, lhs > rhs ? lhs : rhs)
ZR_MATH_UNARY(ZrMath_Scalar_Sqrt, sqrt(value))
ZR_MATH_UNARY(ZrMath_Scalar_Rsqrt, 1.0 / sqrt(value))
ZR_MATH_BINARY(ZrMath_Scalar_Pow, pow(lhs, rhs))
ZR_MATH_UNARY(ZrMath_Scalar_Exp, exp(value))
ZR_MATH_UNARY(ZrMath_Scalar_Log, log(value))
ZR_MATH_UNARY(ZrMath_Scalar_Sin, sin(value))
ZR_MATH_UNARY(ZrMath_Scalar_Cos, cos(value))
ZR_MATH_UNARY(ZrMath_Scalar_Tan, tan(value))
ZR_MATH_UNARY(ZrMath_Scalar_Asin, asin(value))
ZR_MATH_UNARY(ZrMath_Scalar_Acos, acos(value))
ZR_MATH_UNARY(ZrMath_Scalar_Atan, atan(value))
ZR_MATH_BINARY(ZrMath_Scalar_Atan2, atan2(lhs, rhs))
ZR_MATH_UNARY(ZrMath_Scalar_Floor, floor(value))
ZR_MATH_UNARY(ZrMath_Scalar_Ceil, ceil(value))
ZR_MATH_UNARY(ZrMath_Scalar_Round, round(value))
ZR_MATH_UNARY(ZrMath_Scalar_Sign, value < 0.0 ? -1.0 : (value > 0.0 ? 1.0 : 0.0))
ZR_MATH_UNARY(ZrMath_Scalar_Degrees, value * (180.0 / ZR_MATH_PI))
ZR_MATH_UNARY(ZrMath_Scalar_Radians, value * (ZR_MATH_PI / 180.0))

TZrBool ZrMath_Scalar_Clamp(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: docs/wiki/03-modules/math-api.md 声称先调整边界顺序；当前回调保持传入顺序。
     * 核对 low > high 的公开契约及现有调用，再同步文档或行为。 */
    TZrFloat64 value = 0.0, minimum = 0.0, maximum = 0.0;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &value) || !ZrLib_CallContext_ReadFloat(context, 1, &minimum) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &maximum)) return ZR_FALSE;
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    ZrLib_Value_SetFloat(context->state, result, value); return ZR_TRUE;
}
TZrBool ZrMath_Scalar_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 start = 0.0, end = 0.0, factor = 0.0;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &start) || !ZrLib_CallContext_ReadFloat(context, 1, &end) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &factor)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, start + (end - start) * factor); return ZR_TRUE;
}
TZrBool ZrMath_Scalar_AlmostEqual(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: docs/wiki/03-modules/math.md 称绝对/相对误差；ZrMath_AlmostEqual 只比较绝对误差。
     * 核对调用方是否依赖相对阈值，再明确文档中的比较规则。 */
    TZrFloat64 lhs = 0.0, rhs = 0.0, epsilon = ZR_MATH_EPSILON;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &lhs) || !ZrLib_CallContext_ReadFloat(context, 1, &rhs)) return ZR_FALSE;
    if (ZrLib_CallContext_ArgumentCount(context) >= 3 && !ZrLib_CallContext_ReadFloat(context, 2, &epsilon)) return ZR_FALSE;
    ZrLib_Value_SetBool(context->state, result, ZrMath_AlmostEqual(lhs, rhs, epsilon)); return ZR_TRUE;
}
TZrBool ZrMath_InvokeCallback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *callable = ZR_NULL; TZrFloat64 value = 0.0; SZrTypeValue argument;
    if (!ZrLib_CallContext_ReadFunction(context, 0, &callable) || !ZrLib_CallContext_ReadFloat(context, 1, &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, &argument, value);
    return ZrLib_CallValue(context->state, callable, ZR_NULL, &argument, 1, result);
}
