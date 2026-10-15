/** @file
 * ## Summary
 * 实现 Complex 构造、命名方法及 VM 元方法回调；公开参数、返回与调用生命周期见 complex.h。
 * ## Constraints and lifecycle
 * 由注册表交给同步 native binding 调用；字段读取的普通失败返回 false，ReadFloat/ReadObject 的非法实参通过 VM 错误路径报告。
 * ## References
 * - zr_vm_lib_math/include/zr_vm_lib_math/complex.h：各回调的公开契约。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c：构造、字段读取、对象创建与格式化 helper。
 */

#include "zr_vm_lib_math/complex.h"

TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"real", "imag"}; /* 字段名为静态存储，仅在模块映像驻留期间借用。 */
    TZrFloat64 real = 0.0, imag = 0.0;
    TZrFloat64 values[2]; /* 每次调用独有的值快照，由通用构造 helper 同步消费。 */
    if (!ZrLib_CallContext_ReadFloat(context, 0, &real) || !ZrLib_CallContext_ReadFloat(context, 1, &imag)) return ZR_FALSE;
    values[0] = real;
    values[1] = imag;
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields)); /* 可复用兼容目标；字段发布状态不由 setter 返回。 */
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
    object = ZrMath_MakeComplex(context->state, value.real, -value.imag); /* 使用共享新对象字段工厂；窄化 pin 发布风险见 complex.h 文件级契约。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 模长不超过 ZR_MATH_EPSILON 时交付零复数；其余输入按计算模长缩放。 */
    /* BUG: (1e200, 0) 的平方范数溢出后按无穷模缩放，非零复数得到零复数。 */
    ZrMathComplex value; TZrFloat64 magnitude; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    magnitude = sqrt(value.real * value.real + value.imag * value.imag);
    object = magnitude <= ZR_MATH_EPSILON ? ZrMath_MakeComplex(context->state, 0.0, 0.0)
                                          : ZrMath_MakeComplex(context->state, value.real / magnitude, value.imag / magnitude); /* 两分支共用新对象字段工厂。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real + rhs.real, lhs.imag + rhs.imag); /* 使用共享字段发布路径；窄化风险见 complex.h 文件级契约。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real - rhs.real, lhs.imag - rhs.imag); /* 使用共享字段发布路径；窄化风险见 complex.h 文件级契约。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, lhs.real * rhs.real - lhs.imag * rhs.imag, lhs.real * rhs.imag + lhs.imag * rhs.real); /* BUG: 有限 (1e200+i1e200)^2 的实部按 Inf-Inf 计算；字段发布边界也与其他对象工厂共享。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value; SZrObject *object;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeComplex(context->state, -value.real, -value.imag); /* 使用共享字段发布路径；窄化风险见 complex.h 文件级契约。 */
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
/* BUG: 合法有限 (1e200, 0) 与 (2e200, 0) 的平方模长同为 Inf，严格不同的模长被错误比较为 0；
 * TODO: 单独核对 NaN/unordered 与 VM tri-state compare 约定；检查 complex.c:94-95 与 zr_vm_core/src/zr_vm_core/value/meta.c:29-31。 */
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 比较协议定义为模长平方的顺序，不解释复数本身的自然大小。 */
    ZrMathComplex lhs; ZrMathComplex rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadComplexObject(context->state, other, &rhs)) return ZR_FALSE;
    dl = lhs.real * lhs.real + lhs.imag * lhs.imag; dr = rhs.real * rhs.real + rhs.imag * rhs.imag;
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
/* BUG: 合法短串对象申请成功但哈希桶扩容失败时，zr_vm_core/src/zr_vm_core/base/hash_set.c:181 与 zr_vm_core/src/zr_vm_core/string/string.c:808 正常返回 NULL；
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1313 不写结果槽，常规派发在 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362 预置 NULL；zr_vm_lib_math/src/zr_vm_lib_math/common.c:519 仍 TRUE，toString 可报告成功而无字符串。 */
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathComplex value;
    if (!ZrMath_ReadComplexObject(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Complex(%g, %g)", value.real, value.imag);
}
