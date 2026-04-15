//
// Vector3 native callbacks.
//

#include "zr_vm_lib_math/vector3.h"

TZrBool ZrMath_Vector3_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"x", "y", "z"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0;
    TZrFloat64 values[3];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Vector3_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value; if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 3))); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value; if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&value, (TZrFloat64 *)&value, 3)); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
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
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((TZrFloat64 *)&lhs, (TZrFloat64 *)&rhs, 3)); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; TZrFloat64 d[3];
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs)) return ZR_FALSE;
    d[0] = lhs.x - rhs.x; d[1] = lhs.y - rhs.y; d[2] = lhs.z - rhs.z; ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot(d, d, 3))); return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 lhs; ZrMathVector3 rhs; SZrObject *other = ZR_NULL; TZrFloat64 factor = 0.0; SZrObject *object;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &rhs) || !ZrLib_CallContext_ReadFloat(context, 1, &factor)) return ZR_FALSE;
    object = ZrMath_MakeVector3(context->state, lhs.x + (rhs.x - lhs.x) * factor, lhs.y + (rhs.y - lhs.y) * factor, lhs.z + (rhs.z - lhs.z) * factor);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
TZrBool ZrMath_Vector3_Cross(ZrLibCallContext *context, SZrTypeValue *result) {
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
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}

TZrBool ZrMath_Vector3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector3 value;
    if (!ZrMath_ReadVector3Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Vector3(%g, %g, %g)", value.x, value.y, value.z);
}
