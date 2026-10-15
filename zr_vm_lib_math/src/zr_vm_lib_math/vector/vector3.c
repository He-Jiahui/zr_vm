//
// Vector3 native callbacks.
//

/** @file
 * ## Summary
 * 实现 Vector3 native callback；registry 提供 callback 地址，native binding 在 VM 调用期间同步派发。
 *
 * ## Constraints and lifecycle
 * 分量快照和格式缓冲是栈上临时数据；callback 不保留 receiver、参数、context 或 result。
 * 通过 value setter 写入的对象/字符串由当前 VM 管理。普通字段失败仅在线程状态仍为 FINE 时归一化为 null；
 * ReadFloat/ReadObject 标签异常走 VM 错误清理路径。
 *
 * BUG: 构造和 MakeVector3 工厂都调用 void 字段 setter。ignored-root 数组扩容分配失败可令字段写入静默跳过，
 * 外层仍可能交付对象。三字段对象表只到 3/4 threshold，不触发第四字段 nodeMap 扩桶。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。 Length/LengthSquared/Normalized/Dot/MetaCompare 均适用；Distance 使用显式数组。
 *
 * ## References
 * ZrMath_Vector3Registry_GetType；ZrMath_ConstructFloatObject；ZrMath_MakeVector3；
 * native_binding_auto_check_arity 与 native callback finish path。
 */
#include "zr_vm_lib_math/vector3.h"

TZrBool ZrMath_Vector3_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 字段名为静态字面量，并按顺序对应本次调用的 values[0..2]；registry 使用相同顺序。 */
    static const TZrChar *const kFields[] = {"x", "y", "z"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0;
    /* 仅在同步 helper 调用期间借用；与上方静态字段名表不是同一存储期。 */
    TZrFloat64 values[3];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    /* BUG: shared helper 的字段 setter 为 void；key pin 的 ignored-root 扩容失败可跳过写字段，helper 仍发布 receiver。 */
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Vector3_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限 (1e200, 0, 0) 的平方和先溢出为无穷，sqrt 后仍为无穷，而数学长度可由 double 表示。 */
    ZrMathVector3 value; if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 3))); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value; if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 3)); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限 (1e200, 0, 0) 的平方范数溢出；非零分量再除以无穷变成零，返回零而非单位向量。 */
    ZrMathVector3 value; TZrFloat64 length; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    length = sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 3));
    object = length <= ZR_MATH_EPSILON ? ZrMath_MakeVector3(context->state, 0.0, 0.0, 0.0)
                                       : ZrMath_MakeVector3(context->state, value.x / length, value.y / length, value.z / length);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Dot(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    /* BUG: 有限 (1e200,1e200,0) 与 (1e200,-1e200,0) 点积中间乘积为 +Inf/-Inf，累加成 NaN，精确结果为 0。 */
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&rhs, 3)); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限 (1e200, 0, 0) 到零点的距离可表示，但平方差先溢出，最终回调返回无穷。 */
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; TZrFloat64 d[3];
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    d[0] = lhs.x - rhs.x; d[1] = lhs.y - rhs.y; d[2] = lhs.z - rhs.z; ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot(d, d, 3))); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; TZrFloat64 factor = 0.0; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs) || !ZrLib_CallContext_ReadFloat(context, 1, &factor)) return ZR_FALSE;
    /* BUG: 有限 x 端点 1e308/-1e308、factor=0.5 时差分先溢出为 -Inf；数学中点为 0。 */
    object = ZrMath_MakeVector3(context->state, lhs.x + (rhs.x - lhs.x) * factor, lhs.y + (rhs.y - lhs.y) * factor, lhs.z + (rhs.z - lhs.z) * factor);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Cross(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: (0,1e200,1e200) 与自身做叉积时乘积先变为 Inf，Inf-Inf 得 NaN，精确结果为零向量。 */
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector3(context->state, lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector3_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector3(context->state, lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector3_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector3(context->state, lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector3_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeVector3(context->state, -value.x, -value.y, -value.z);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector3_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    dl = ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&lhs, 3); dr = ZrMath_Dot((TZrFloat64 *)&rhs, (TZrFloat64 *)&rhs, 3);
    /* TODO: 严格比较使 NaN 或双侧平方和均为 Inf 时落入 0；0 不代表相等，需核实际比较消费者契约。 */
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}

TZrBool ZrMath_Vector3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    /* BUG: 未驻留短输出触发 HASH_BUCKET rehash 且原生分配失败时，SetString 不写结果而 helper 仍报成功；dispatch 可交付预置 null。 */
    return ZrMath_MakeStringResult(context->state, result, "Vector3(%g, %g, %g)", value.x, value.y, value.z);
}
