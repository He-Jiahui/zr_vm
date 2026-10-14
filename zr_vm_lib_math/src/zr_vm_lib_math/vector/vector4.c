//
// Vector4 native callbacks.
//

/** @file
 * ## Summary
 * 实现由 Vector4 descriptor 注册的构造、数值方法和 meta callback；native binding 在 VM dispatch 内同步调用。
 *
 * ## Constraints and lifecycle
 * 分量快照、delta 和格式化缓冲均为本次调用的栈数据；callback 不保留 context、receiver、实参或 result。
 * ReadFloat/ReadObject 的标签错误走 VM 异常清理；字段读取或 helper 普通失败只有在 thread 仍为 FINE 时才归一化为 null。
 * 对象/字符串结果由当前 VM 管理。
 *
 * BUG: ConstructFloatObject 与 MakeVector4 都通过 void 字段 setter 写入字段。若 ignored-root 表原有 count 为
 * capacity-1 且 receiver 与字段键都尚未登记，setter 先登记 receiver、再登记字段键；后一步的 ARRAY 扩容 RawMallocWithType
 * 失败会静默漏写，外层仍可发布对象。四字段 nodeMap 的 HASH_BUCKET 扩桶是另一条独立失败路径。
 *
 * TODO: Length、LengthSquared、Normalized、Dot 和 MetaCompare 将 ZrMathVector4 地址转成 double 数组供 ZrMath_Dot
 * 读取四项；需核对 math_common.h 的结构布局和跨成员指针运算契约。Distance 使用自己的 `d[4]` 数组，不依赖该 cast。
 *
 * ## References
 * ZrMath_Vector4Registry_GetType；ZrMath_ConstructFloatObject；ZrMath_MakeVector4；ZrMath_ReadVector4Object；
 * native_binding_auto_check_arity；native_binding_invoke_entry_callback_inline
 */
#include "zr_vm_lib_math/vector4.h"

TZrBool ZrMath_Vector4_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 静态键按 x/y/z/w 顺序对应本次 helper 调用的栈上 values；二者生命周期不同。 */
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0, w = 0.0;
    TZrFloat64 values[4];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z) || !ZrLib_CallContext_ReadFloat(context, 3, &w)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    values[3] = w;
    /* BUG: 普通 compiler seed 是空 nodeMap struct；前三个字段达到初始容量 4、3/4 load threshold，
     * 写第四个 w 时需 HASH_BUCKET rehash。若该次 Memory_Allocate 失败，void setter 保留缺 w 对象，
     * helper 仍继续并 FinishConstructObject。独立地，ignored-root 表处于 capacity-1 且 receiver、字段键均未登记时，
     * receiver pin 占末槽后字段键的 ARRAY 扩容 RawMallocWithType 也可能失败并漏写当前字段。 */
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}
TZrBool ZrMath_Vector4_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限 (1e200,0,0,0) 的平方和溢出为无穷，虽然数学长度仍可由 double 表示。 */
    ZrMathVector4 value; if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4))); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: (1e200, 0, 0, 0) 的平方范数溢出后按无穷长度缩放，非零输入得到零向量。 */
    ZrMathVector4 value; TZrFloat64 length; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    length = sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4));
    /* BUG: MakeVector4 的新对象初始 nodeMap 容量为 4、load threshold 为 3；前三字段后第四个 w 触发扩桶，
     * HASH_BUCKET Memory_Allocate 失败会静默漏字段。factory 仍返回非空对象，调用方可继续发布它。
     * 同一 factory 发布路径也被 Lerp、MetaAdd、MetaSub、MetaNeg 和 Matrix4x4.mulVector 使用。 */
    object = length <= ZR_MATH_EPSILON ? ZrMath_MakeVector4(context->state, 0.0, 0.0, 0.0, 0.0)
                                       : ZrMath_MakeVector4(context->state, value.x / length, value.y / length, value.z / length, value.w / length);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Dot(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    /* BUG: 有限 (1e200,1e200,0,0) 与 (1e200,-1e200,0,0) 的乘积先成 +Inf/-Inf，累加为 NaN，实数点积为 0。 */
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&rhs, 4)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: (1e200,0,0,0) 到零点的有限距离可表示，但局部差分的平方先溢出，结果返回无穷。 */
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; TZrFloat64 d[4];
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    d[0] = lhs.x - rhs.x; d[1] = lhs.y - rhs.y; d[2] = lhs.z - rhs.z; d[3] = lhs.w - rhs.w;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot(d, d, 4))); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; TZrFloat64 factor = 0.0; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs) || !ZrLib_CallContext_ReadFloat(context, 1, &factor)) return ZR_FALSE;
    /* BUG: 有限 x 端点 1e308/-1e308 且 factor=0.5 时差分先溢出为 -Inf，数学中点本应为 0。 */
    object = ZrMath_MakeVector4(context->state, lhs.x + (rhs.x - lhs.x) * factor, lhs.y + (rhs.y - lhs.y) * factor, lhs.z + (rhs.z - lhs.z) * factor, lhs.w + (rhs.w - lhs.w) * factor);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, -value.x, -value.y, -value.z, -value.w);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    dl = ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&lhs, 4); dr = ZrMath_Dot((TZrFloat64 *)&rhs, (TZrFloat64 *)&rhs, 4);
    /* TODO: NaN 或两边平方长度都溢出为 Inf 时两个严格比较均为假并写 0；需核 compare consumer 的 unordered 策略。 */
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    /* BUG: 未驻留短串写入 HASH_BUCKET 表需扩桶而 Memory_Allocate 失败时，String_Create 返回 null；
     * SetStringObject 不改已预置的 null result，MakeStringResult 仍返回 true，dispatch 随后成功回写 null。 */
    return ZrMath_MakeStringResult(context->state, result, "Vector4(%g, %g, %g, %g)", value.x, value.y, value.z, value.w);
}
