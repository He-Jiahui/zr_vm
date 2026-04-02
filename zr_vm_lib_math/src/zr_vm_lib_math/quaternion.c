//
// Quaternion native callbacks.
//

#include "zr_vm_lib_math/quaternion.h"

static ZrMathQuaternion zr_math_quaternion_mul_value(ZrMathQuaternion lhs, ZrMathQuaternion rhs) {
    ZrMathQuaternion result;
    result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
    result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
    result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    return result;
}

static SZrObject *zr_math_quaternion_make_object(SZrState *state, ZrMathQuaternion q) {
    return ZrMath_MakeQuaternion(state, q.x, q.y, q.z, q.w);
}

TZrBool ZrMath_Quaternion_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    TZrFloat64 x = 0.0, y = 0.0, z = 0.0, w = 1.0;
    TZrFloat64 values[4];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &z) || !ZrLib_CallContext_ReadFloat(context, 3, &w)) return ZR_FALSE;
    values[0] = x;
    values[1] = y;
    values[2] = z;
    values[3] = w;
    return ZrMath_ConstructFloatObject(context, result, "Quaternion", kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Quaternion_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q; if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w)); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q; if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
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
    ZrMathQuaternion lhs; ZrMathQuaternion rhs; SZrObject *other = ZR_NULL; TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadQuaternionObject(context->state, other, &rhs)) return ZR_FALSE;
    dl = lhs.x*lhs.x + lhs.y*lhs.y + lhs.z*lhs.z + lhs.w*lhs.w; dr = rhs.x*rhs.x + rhs.y*rhs.y + rhs.z*rhs.z + rhs.w*rhs.w;
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0)); return ZR_TRUE;
}
TZrBool ZrMath_Quaternion_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathQuaternion q;
    if (!ZrMath_ReadQuaternionObject(context->state, ZrMath_SelfObject(context), &q)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Quaternion(%g, %g, %g, %g)", q.x, q.y, q.z, q.w);
}
