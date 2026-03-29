//
// Matrix4x4 native callbacks.
//

#include "zr_vm_lib_math/matrix4x4.h"

static void zr_math_matrix4_identity(TZrFloat64 *m) {
    TZrSize i; for (i = 0; i < 16; i++) m[i] = 0.0; m[0] = m[5] = m[10] = m[15] = 1.0;
}
static void zr_math_matrix4_mul(const TZrFloat64 *a, const TZrFloat64 *b, TZrFloat64 *out) {
    TZrSize r, c, k; for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) { out[r * 4 + c] = 0.0; for (k = 0; k < 4; k++) out[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c]; }
}
static TZrFloat64 zr_math_matrix4_det(TZrFloat64 *m) {
    TZrFloat64 a[16]; TZrFloat64 det = 1.0; TZrSize i, j, k; memcpy(a, m, sizeof(a));
    for (i = 0; i < 4; i++) {
        TZrSize pivot = i; for (j = i + 1; j < 4; j++) if (fabs(a[j * 4 + i]) > fabs(a[pivot * 4 + i])) pivot = j;
        if (fabs(a[pivot * 4 + i]) <= ZR_MATH_EPSILON) return 0.0;
        if (pivot != i) { for (k = 0; k < 4; k++) { TZrFloat64 t = a[i * 4 + k]; a[i * 4 + k] = a[pivot * 4 + k]; a[pivot * 4 + k] = t; } det = -det; }
        det *= a[i * 4 + i];
        for (j = i + 1; j < 4; j++) { TZrFloat64 f = a[j * 4 + i] / a[i * 4 + i]; for (k = i; k < 4; k++) a[j * 4 + k] -= f * a[i * 4 + k]; }
    }
    return det;
}
static TZrBool zr_math_matrix4_inverse(const TZrFloat64 *m, TZrFloat64 *out) {
    TZrFloat64 a[16], inv[16]; TZrSize i, j, k; memcpy(a, m, sizeof(a)); zr_math_matrix4_identity(inv);
    for (i = 0; i < 4; i++) {
        TZrSize pivot = i; for (j = i + 1; j < 4; j++) if (fabs(a[j * 4 + i]) > fabs(a[pivot * 4 + i])) pivot = j;
        if (fabs(a[pivot * 4 + i]) <= ZR_MATH_EPSILON) return ZR_FALSE;
        if (pivot != i) for (k = 0; k < 4; k++) { TZrFloat64 t = a[i * 4 + k]; a[i * 4 + k] = a[pivot * 4 + k]; a[pivot * 4 + k] = t; t = inv[i * 4 + k]; inv[i * 4 + k] = inv[pivot * 4 + k]; inv[pivot * 4 + k] = t; }
        { TZrFloat64 d = a[i * 4 + i]; for (k = 0; k < 4; k++) { a[i * 4 + k] /= d; inv[i * 4 + k] /= d; } }
        for (j = 0; j < 4; j++) if (j != i) { TZrFloat64 f = a[j * 4 + i]; for (k = 0; k < 4; k++) { a[j * 4 + k] -= f * a[i * 4 + k]; inv[j * 4 + k] -= f * inv[i * 4 + k]; } }
    }
    memcpy(out, inv, sizeof(inv)); return ZR_TRUE;
}
TZrBool ZrMath_Matrix4x4_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {
            "m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"
    };
    TZrFloat64 values[16]; TZrSize i;
    if (ZrLib_CallContext_ArgumentCount(context) == 0) zr_math_matrix4_identity(values);
    else { if (ZrLib_CallContext_ArgumentCount(context) != 16) { ZrLib_CallContext_RaiseArityError(context, 0, 16); return ZR_FALSE; } for (i = 0; i < 16; i++) if (!ZrLib_CallContext_ReadFloat(context, i, &values[i])) return ZR_FALSE; }
    return ZrMath_ConstructFloatObject(context, result, "Matrix4x4", kFields, values, ZR_ARRAY_COUNT(kFields));
}
TZrBool ZrMath_Matrix4x4_Identity(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; SZrObject *o; zr_math_matrix4_identity(m); o = ZrMath_MakeMatrix4x4(context->state, m); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Transpose(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16], t[16]; TZrSize r, c; SZrObject *o; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) t[r * 4 + c] = m[c * 4 + r]; o = ZrMath_MakeMatrix4x4(context->state, t); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Determinant(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; ZrLib_Value_SetFloat(context->state, result, zr_math_matrix4_det(m)); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Inverse(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16], inv[16]; SZrObject *o; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m) || !zr_math_matrix4_inverse(m, inv)) return ZR_FALSE; o = ZrMath_MakeMatrix4x4(context->state, inv); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_MulVector(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 m[16]; ZrMathVector4 v; SZrObject *other = ZR_NULL; SZrObject *o; TZrFloat64 r[4];
    if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m) || !ZrLib_CallContext_ReadObject(context, 0, &other) || !ZrMath_ReadVector4Object(context->state, other, &v)) return ZR_FALSE;
    r[0] = m[0]*v.x + m[1]*v.y + m[2]*v.z + m[3]*v.w; r[1] = m[4]*v.x + m[5]*v.y + m[6]*v.z + m[7]*v.w; r[2] = m[8]*v.x + m[9]*v.y + m[10]*v.z + m[11]*v.w; r[3] = m[12]*v.x + m[13]*v.y + m[14]*v.z + m[15]*v.w;
    o = ZrMath_MakeVector4(context->state, r[0], r[1], r[2], r[3]); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Matrix4x4_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 a[16], b[16], out[16]; SZrObject *other = ZR_NULL; SZrObject *o;
    if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), a) || !ZrLib_CallContext_ReadObject(context, 0, &other) || !ZrMath_ReadMatrix4Object(context->state, other, b)) return ZR_FALSE;
    zr_math_matrix4_mul(a, b, out); o = ZrMath_MakeMatrix4x4(context->state, out); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Matrix4x4_Translation(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 x = 0.0, y = 0.0, z = 0.0, m[16]; SZrObject *o; if (!ZrLib_CallContext_ReadFloat(context,0,&x) || !ZrLib_CallContext_ReadFloat(context,1,&y) || !ZrLib_CallContext_ReadFloat(context,2,&z)) return ZR_FALSE; zr_math_matrix4_identity(m); m[3]=x; m[7]=y; m[11]=z; o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Scale(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 x = 1.0, y = 1.0, z = 1.0, m[16]; SZrObject *o; if (!ZrLib_CallContext_ReadFloat(context,0,&x) || !ZrLib_CallContext_ReadFloat(context,1,&y) || !ZrLib_CallContext_ReadFloat(context,2,&z)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=x; m[5]=y; m[10]=z; o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationX(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[5]=cos(a); m[6]=-sin(a); m[9]=sin(a); m[10]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationY(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=cos(a); m[2]=sin(a); m[8]=-sin(a); m[10]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationZ(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=cos(a); m[1]=-sin(a); m[4]=sin(a); m[5]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) { SZrObject *other = ZR_NULL; if (!ZrLib_CallContext_ReadObject(context, 0, &other)) return ZR_FALSE; return ZrMath_ObjectTypeEquals(context->state, other, "Vector4") ? ZrMath_Matrix4x4_MulVector(context, result) : ZrMath_Matrix4x4_MulMatrix(context, result); }
TZrBool ZrMath_Matrix4x4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; return ZrMath_MakeStringResult(context->state, result, "Matrix4x4([%g,%g,%g,%g],[%g,%g,%g,%g],[%g,%g,%g,%g],[%g,%g,%g,%g])", m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11],m[12],m[13],m[14],m[15]); }
