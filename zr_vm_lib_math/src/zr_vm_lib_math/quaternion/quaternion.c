//
// Quaternion native callbacks.
//

#include "zr_vm_lib_math/quaternion.h"

/**
 * 按 Hamilton 乘法的左、右顺序计算值结果；供命名 mul 与 `*` 元方法共用，
 * 避免两个公开入口在符号或操作数次序上分叉。输入是回调栈上的数值快照，
 * 结果按值返回，不分配 VM 对象。
 */
static ZrMathQuaternion zr_math_quaternion_mul_value(ZrMathQuaternion lhs, ZrMathQuaternion rhs) {
    /* Hamilton 积保留乘法次序；命名 mul 与 `*` 元方法共同使用该语义。 */
    ZrMathQuaternion result;
    result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
    result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
    result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    return result;
}

/**
 * 把数值快照交给 math 模块的 Quaternion factory；成功时返回新建、由 VM GC
 * 管理的对象，失败时返回 NULL。ZrMath_MakeQuaternion 也由其他结果回调直接使用，
 * 而构造回调走 ZrMath_ConstructFloatObject；两条路径最终都调用 void 字段写入器。
 * 新实例只初始化空 nodeMap；构造 helper 也可能复用兼容 self，但新建路径没有默认字段。
 * BUG: 初始容量 4、负载阈值 3，写第四个 w 字段需要扩桶；此前的 pin、键字符串
 * 创建均成功而 HASH_BUCKET 原生扩容返回 NULL、保留旧块且没有 VM 异常时，void
 * setter 仅记录日志而不传回状态，factory 仍可成功交付缺 w 的对象。完整路径见
 * common.c 的 factory、Object_SetFieldCString、object_set_value_core、HashSet_Add/Rehash 与审查台账。
 */
static SZrObject *zr_math_quaternion_make_object(SZrState *state, ZrMathQuaternion q) {
    return ZrMath_MakeQuaternion(state, q.x, q.y, q.z, q.w);
}

TZrBool ZrMath_Quaternion_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 与注册表的 x/y/z/w 结构及 values 顺序对齐，供通用构造 helper 逐字段填值。 */
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0, w = 1.0;
    TZrFloat64 values[4];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z) || !ZrLib_CallContext_ReadFloat(context, 3, &w)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    values[3] = w;
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Quaternion_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 构造器允许有限 double 分量且无范围校验；(1e200,0,0,0) 平方溢出，length 返回无穷。 */
    ZrMathQuaternion q; if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w)); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q; if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 零长度采用单位四元数作为中性回退，区别于 Complex 的零结果。 */
    /* BUG: 合法有限输入 (1e200,0,0,0) 的平方范数溢出为无穷，除后得到零四元数。 */
    ZrMathQuaternion q; TZrFloat64 len; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    len = sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len <= ZR_MATH_EPSILON) object = ZrMath_MakeQuaternion(context->state, 0.0, 0.0, 0.0, 1.0);
    else object = ZrMath_MakeQuaternion(context->state, q.x/len, q.y/len, q.z/len, q.w/len);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Conjugate(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    object = ZrMath_MakeQuaternion(context->state, -q.x, -q.y, -q.z, q.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Inverse(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 合法有限输入 (1e200,0,0,0) 的平方范数溢出为无穷，使逆元退化为零。 */
    ZrMathQuaternion q; TZrFloat64 lenSq; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    lenSq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (lenSq <= ZR_MATH_EPSILON) object = ZrMath_MakeQuaternion(context->state, 0.0, 0.0, 0.0, 1.0);
    else object = ZrMath_MakeQuaternion(context->state, -q.x/lenSq, -q.y/lenSq, -q.z/lenSq, q.w/lenSq);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Dot(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z + lhs.w*rhs.w); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Mul(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    object = zr_math_quaternion_make_object(context->state, zr_math_quaternion_mul_value(lhs, rhs));
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Slerp(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 对单位端点，负点积翻转右端可选择较短插值弧；接近同向时走线性分支，避免除以小 sin(theta)。 */
    /* TODO: math-api.md 要求单位端点，但本回调不验证或归一化端点；该文档未约定 t 区间，
     * 近同向线性分支也不归一化结果。核对调用方是否依赖单位输出及 t 的范围约定，再以
     * 单位近邻端点检查插值结果；不要把非单位端点当作违反文档前提之外的证据。 */
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; TZrFloat64 t = 0.0; TZrFloat64 dot; TZrFloat64 theta; TZrFloat64 s0; TZrFloat64 s1; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs) || !ZrLib_CallContext_ReadFloat(context, 1, &t)) return ZR_FALSE;
    dot = lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z + lhs.w*rhs.w;
    if (dot < 0.0) { dot = -dot; rhs.x = -rhs.x; rhs.y = -rhs.y; rhs.z = -rhs.z; rhs.w = -rhs.w; }
    if (dot > 0.9995) {
        object = ZrMath_MakeQuaternion(context->state, lhs.x + (rhs.x - lhs.x) * t, lhs.y + (rhs.y - lhs.y) * t, lhs.z + (rhs.z - lhs.z) * t, lhs.w + (rhs.w - lhs.w) * t);
    } else {
        theta = acos(dot); s0 = sin((1.0 - t) * theta) / sin(theta); s1 = sin(t * theta) / sin(theta);
        object = ZrMath_MakeQuaternion(context->state, lhs.x * s0 + rhs.x * s1, lhs.y * s0 + rhs.y * s1, lhs.z * s0 + rhs.z * s1, lhs.w * s0 + rhs.w * s1);
    }
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeQuaternion(context->state, lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeQuaternion(context->state, lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) { return ZrMath_Quaternion_Mul(context, result); }
TZrBool ZrMath_Quaternion_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q; SZrObject *object;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    object = ZrMath_MakeQuaternion(context->state, -q.x, -q.y, -q.z, -q.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    /* VM 比较协议按模长平方排序；等模但不同旋转可能比较为相等。 */
    /* TODO: 两组有限大分量的平方和都可溢出为无穷并比较为相等；核对公开比较契约
     * 是否要求保留真实大小顺序，再用可表示但平方溢出的有限输入验证。 */
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    dl = lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z + lhs.w*lhs.w; dr = rhs.x*rhs.x + rhs.y*rhs.y + rhs.z*rhs.z + rhs.w*rhs.w;
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    /* BUG: 短串表满载时扩桶用原生分配；失败令 Create 返回 NULL，void value setter
     * 保留 dispatch 预置的 null，而 MakeStringResult 仍返回 true。合法 toString 调用
     * 因而可报告成功但得到 null；完整下游证据见 math-api.md 与 coverage 台账。 */
    return ZrMath_MakeStringResult(context->state, result, "Quaternion(%g, %g, %g, %g)", q.x, q.y, q.z, q.w);
}
