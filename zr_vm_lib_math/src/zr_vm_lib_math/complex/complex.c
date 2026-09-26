//
// Complex native callbacks.
//

#include "zr_vm_lib_math/complex.h"

TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"real", "imag"};
    TZrFloat64 real = 0.0, imag = 0.0;
    TZrFloat64 values[2];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &real) || !ZrLib_CallContext_ReadFloat(context, 1, &imag)) return ZR_FALSE;
    values[0] = real;
    values[1] = imag;
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}
TZrBool ZrMath_Complex_Magnitude(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限输入 (1e200, 0) 的平方中间值溢出，magnitude 返回无穷；
     * 同一计算也使 normalized 把非零复数错误地归一化为零。 */
    ZrMathComplex value; if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(value.real * value.real + value.imag * value.imag)); return ZR_TRUE;
}
TZrBool ZrMath_Complex_Phase(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value; if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, atan2(value.imag, value.real)); return ZR_TRUE;
}
TZrBool ZrMath_Complex_Conjugate(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, value.real, -value.imag);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 零模选择零复数作为结果；其余输入的范数由 magnitude 同款计算路径决定。 */
    ZrMathComplex value; TZrFloat64 magnitude; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    magnitude = sqrt(value.real * value.real + value.imag * value.imag);
    object = magnitude <= ZR_MATH_EPSILON ? ZrMath_MakeComplex(context->state, 0.0, 0.0)
                                          : ZrMath_MakeComplex(context->state, value.real / magnitude, value.imag / magnitude);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real + rhs.real, lhs.imag + rhs.imag);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real - rhs.real, lhs.imag - rhs.imag);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real * rhs.real - lhs.imag * rhs.imag, lhs.real * rhs.imag + lhs.imag * rhs.real);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, -value.real, -value.imag);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 比较协议定义为模长平方的顺序，不解释复数本身的自然大小。 */
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    dl = lhs.real * lhs.real + lhs.imag * lhs.imag; dr = rhs.real * rhs.real + rhs.imag * rhs.imag;
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Complex(%g, %g)", value.real, value.imag);
}
