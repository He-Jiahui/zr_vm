//
// Vector4 native callbacks.
//

#include "zr_vm_lib_math/vector4.h"

TZrBool ZrMath_Vector4_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0, w = 0.0;
    TZrFloat64 values[4];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z) || !ZrLib_CallContext_ReadFloat(context, 3, &w)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    values[3] = w;
    return ZrMath_ConstructFloatObject(context, result, "Vector4", kFields, values, ZR_ARRAY_COUNT(kFields));
}
TZrBool ZrMath_Vector4_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4))); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; TZrFloat64 length; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    length = sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 4));
    object = length <= ZR_MATH_EPSILON ? ZrMath_MakeVector4(context->state, 0.0, 0.0, 0.0, 0.0)
                                       : ZrMath_MakeVector4(context->state, value.x / length, value.y / length, value.z / length, value.w / length);
    if (object == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Dot(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&rhs, 4)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
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
    object = ZrMath_MakeVector4(context->state, lhs.x + (rhs.x - lhs.x) * factor, lhs.y + (rhs.y - lhs.y) * factor, lhs.z + (rhs.z - lhs.z) * factor, lhs.w + (rhs.w - lhs.w) * factor);
    if (object == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
    if (object == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaSub(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
    if (object == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value; SZrObject *object;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeVector4(context->state, -value.x, -value.y, -value.z, -value.w);
    if (object == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 lhs; ZrMathVector4 rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector4Object(context->state, other, &rhs)) return ZR_FALSE;
    dl = ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&lhs, 4); dr = ZrMath_Dot((TZrFloat64 *)&rhs, (TZrFloat64 *)&rhs, 4);
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
TZrBool ZrMath_Vector4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector4 value;
    if (!ZrMath_ReadVector4Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Vector4(%g, %g, %g, %g)", value.x, value.y, value.z, value.w);
}
