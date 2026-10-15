//
// Vector2 native callbacks.
//

#include "zr_vm_lib_math/vector2.h"

/* 回调只把 VM 字段复制到本地双精度快照；结果值和新对象由 dispatcher/VM 接管。 */
TZrBool ZrMath_Vector2_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"x", "y"};
    TZrFloat64 x = 0.0;
    TZrFloat64 y = 0.0;
    TZrFloat64 values[2];
    /* BUG: CREATE_OBJECT/TO_STRUCT 后的 __constructor FUNCTION_CALL 满足数值 fast lane；
     * fast lane 只浅复制 receiver 值槽，不 pin。ignored-root 仅余一槽且 self/key 未登记时，
     * setter 先 pin self，再为字段 key 扩表；RawMalloc 失败软早退但 helper 仍发布对象。 */
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y)) {
        return ZR_FALSE;
    }
    values[0] = x;
    values[1] = y;
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Vector2_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: 有限向量 (1e200, 0) 的点积中间值溢出，length 得到无穷；
     * normalized 沿用该范数后把非零向量压成零向量。详见 ZrMath_Vector2_Normalized 回调。 */
    /* BUG: count=2 的 struct 输入在 AVX2 尾循环/纯 scalar 读取下标1越界；SSE2/NEON 单次 intrinsic 对象范围仍待核，见 math_common.h:134。 */
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((const TZrFloat64 *)&value, (const TZrFloat64 *)&value, 2)));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 与 length 共享平方和，但本方法直接暴露 IEEE 浮点平方和结果。 */
    /* BUG: count=2 的 struct 输入在 AVX2 尾循环/纯 scalar 读取下标1越界；SSE2/NEON 单次 intrinsic 对象范围仍待核，见 math_common.h:134。 */
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((const TZrFloat64 *)&value, (const TZrFloat64 *)&value, 2));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: (1e200, 0) 的平方范数溢出后按无穷长度缩放，非零输入得到零向量。
     * BUG: 当新对象/字段键 pin 要扩容满载的 ignored-root 表且 RawMalloc 失败时，
     * SetFieldCString 可静默漏写 x/y，下面仍把新对象作为成功结果交付。 */
    /* BUG: count=2 的 struct 输入在 AVX2 尾循环/纯 scalar 读取下标1越界；SSE2/NEON 单次 intrinsic 对象范围仍待核，见 math_common.h:134。 */
    ZrMathVector2 value; TZrFloat64 length; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    length = sqrt(ZrMath_Dot((const TZrFloat64 *)&value, (const TZrFloat64 *)&value, 2));
    object = length <= ZR_MATH_EPSILON ? ZrMath_MakeVector2(context->state, 0.0, 0.0)
                                       : ZrMath_MakeVector2(context->state, value.x / length, value.y / length);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Dot(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    /* BUG: count=2 的 struct 输入在 AVX2 尾循环/纯 scalar 读取下标1越界；SSE2/NEON 单次 intrinsic 对象范围仍待核，见 math_common.h:134。 */
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((const TZrFloat64 *)&lhs, (const TZrFloat64 *)&rhs, 2));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: (1e200, 0) 到零点的有限距离在平方差阶段溢出，回调返回无穷。 */
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL;
    TZrFloat64 dx; TZrFloat64 dy;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    dx = lhs.x - rhs.x; dy = lhs.y - rhs.y;
    {
        const TZrFloat64 delta[2] = {dx, dy};
        ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot(delta, delta, 2)));
    }
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 插值因子未限制在 [0,1]，调用方可以用同一入口外推。
     * BUG: 有限端点 (1e308,0)、(-1e308,0) 且 factor=0.5 时，rhs-lhs 先溢出，
     * x 变为负无穷，而数学中点为零。新对象字段 pin 的满表扩容失败也会软漏写。 */
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL; TZrFloat64 factor = 0.0; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs) ||
        !ZrLib_CallContext_ReadFloat(context, 1, &factor)) return ZR_FALSE;
    object = ZrMath_MakeVector2(context->state, lhs.x + (rhs.x - lhs.x) * factor, lhs.y + (rhs.y - lhs.y) * factor);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    /* BUG: MakeVector2 的字段 setter 在 ignored-root 扩表 RawMalloc 失败时可漏写，helper 不检查。 */
    object = ZrMath_MakeVector2(context->state, lhs.x + rhs.x, lhs.y + rhs.y);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    /* BUG: MakeVector2 的字段 setter 在 ignored-root 扩表 RawMalloc 失败时可漏写，helper 不检查。 */
    object = ZrMath_MakeVector2(context->state, lhs.x - rhs.x, lhs.y - rhs.y);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    /* BUG: MakeVector2 的字段 setter 在 ignored-root 扩表 RawMalloc 失败时可漏写，helper 不检查。 */
    object = ZrMath_MakeVector2(context->state, -value.x, -value.y);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    /* BUG: count=2 的 struct 输入在 AVX2 尾循环/纯 scalar 读取下标1越界；SSE2/NEON 单次 intrinsic 对象范围仍待核，见 math_common.h:134。 */
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL;
    TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    /* TODO: 对照 docs/wiki/03-modules/math-api.md:296-308 的 Vector2 compare 契约，为 NaN 和
     * 双边平方和溢出为 Inf 的分支补断言；确认 unordered/溢出是否应报为相等。 */

    dl = ZrMath_Dot((const TZrFloat64 *)&lhs, (const TZrFloat64 *)&lhs, 2);
    dr = ZrMath_Dot((const TZrFloat64 *)&rhs, (const TZrFloat64 *)&rhs, 2);
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    /* BUG: 若格式结果为未驻留短串且表已达 resize threshold，short-intern 插入会触发
     * HASH_BUCKET rehash；RawMalloc 失败使创建返回 null，SetString 不写 result，而 helper 仍报 true。 */
    return ZrMath_MakeStringResult(context->state, result, "Vector2(%g, %g)", value.x, value.y);
}
