//
// Vector2 native callbacks.
//

#include "zr_vm_lib_math/vector2.h"

TZrBool ZrMath_Vector2_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"x", "y"};
    TZrFloat64 x = 0.0;
    TZrFloat64 y = 0.0;
    TZrFloat64 values[2];
    if (!ZrLib_CallContext_ReadFloat(context, 0, &x) || !ZrLib_CallContext_ReadFloat(context, 1, &y)) {
        return ZR_FALSE;
    }
    values[0] = x;
    values[1] = y;
    return ZrMath_ConstructFloatObject(context, result, "Vector2", kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Vector2_Length(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, sqrt(ZrMath_Dot((const TZrFloat64 *)&value, (const TZrFloat64 *)&value, 2)));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((const TZrFloat64 *)&value, (const TZrFloat64 *)&value, 2));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Normalized(ZrLibCallContext *context, SZrTypeValue *result) {
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
    ZrLib_Value_SetFloat(context->state, result, ZrMath_Dot((const TZrFloat64 *)&lhs, (const TZrFloat64 *)&rhs, 2));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_Distance(ZrLibCallContext *context, SZrTypeValue *result) {
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
    object = ZrMath_MakeVector2(context->state, lhs.x - rhs.x, lhs.y - rhs.y);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value; SZrObject *object;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    object = ZrMath_MakeVector2(context->state, -value.x, -value.y);
    if (object == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 lhs; ZrMathVector2 rhs; SZrObject *other = ZR_NULL;
    TZrFloat64 dl; TZrFloat64 dr;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector2Object(context->state, other, &rhs)) return ZR_FALSE;
    dl = ZrMath_Dot((const TZrFloat64 *)&lhs, (const TZrFloat64 *)&lhs, 2);
    dr = ZrMath_Dot((const TZrFloat64 *)&rhs, (const TZrFloat64 *)&rhs, 2);
    ZrLib_Value_SetInt(context->state, result, dl > dr ? 1 : (dl < dr ? -1 : 0));
    return ZR_TRUE;
}

TZrBool ZrMath_Vector2_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathVector2 value;
    if (!ZrMath_ReadVector2Object(context->state, ZrMath_SelfObject(context), &value)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Vector2(%g, %g)", value.x, value.y);
}
