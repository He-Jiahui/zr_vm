//
// Matrix3x3 native callbacks.
//

#include "zr_vm_lib_math/matrix3x3.h"

static void zr_math_matrix3_identity(TZrFloat64 *m) {
    TZrSize i;
    for (i = 0; i < 9; i++) {
        m[i] = 0.0;
    }
    m[0] = 1.0;
    m[4] = 1.0;
    m[8] = 1.0;
}

static void zr_math_matrix3_mul(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrFloat64 *out) {
    TZrSize row;
    TZrSize column;
    TZrSize k;
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            out[row * 3 + column] = 0.0;
            for (k = 0; k < 3; k++) {
                out[row * 3 + column] += lhs[row * 3 + k] * rhs[k * 3 + column];
            }
        }
    }
}

static TZrFloat64 zr_math_matrix3_det(const TZrFloat64 *m) {
    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}

static TZrBool zr_math_matrix3_inverse(const TZrFloat64 *m, TZrFloat64 *out) {
    TZrFloat64 determinant = zr_math_matrix3_det(m);
    if (fabs(determinant) <= ZR_MATH_EPSILON) {
        return ZR_FALSE;
    }

    out[0] = (m[4] * m[8] - m[5] * m[7]) / determinant;
    out[1] = (m[2] * m[7] - m[1] * m[8]) / determinant;
    out[2] = (m[1] * m[5] - m[2] * m[4]) / determinant;
    out[3] = (m[5] * m[6] - m[3] * m[8]) / determinant;
    out[4] = (m[0] * m[8] - m[2] * m[6]) / determinant;
    out[5] = (m[2] * m[3] - m[0] * m[5]) / determinant;
    out[6] = (m[3] * m[7] - m[4] * m[6]) / determinant;
    out[7] = (m[1] * m[6] - m[0] * m[7]) / determinant;
    out[8] = (m[0] * m[4] - m[1] * m[3]) / determinant;
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"};
    TZrFloat64 values[9];
    TZrSize index;

    if (ZrLib_CallContext_ArgumentCount(context) == 0) {
        zr_math_matrix3_identity(values);
    } else {
        if (ZrLib_CallContext_ArgumentCount(context) != 9) {
            ZrLib_CallContext_RaiseArityError(context, 0, 9);
            return ZR_FALSE;
        }
        for (index = 0; index < 9; index++) {
            if (!ZrLib_CallContext_ReadFloat(context, index, &values[index])) {
                return ZR_FALSE;
            }
        }
    }

    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Matrix3x3_Identity(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 values[9];
    SZrObject *object;
    zr_math_matrix3_identity(values);
    object = ZrMath_MakeMatrix3x3(context->state, values);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Transpose(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    TZrFloat64 transpose[9];
    TZrSize row;
    TZrSize column;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            transpose[row * 3 + column] = matrix[column * 3 + row];
        }
    }

    object = ZrMath_MakeMatrix3x3(context->state, transpose);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Determinant(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetFloat(context->state, result, zr_math_matrix3_det(matrix));
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Inverse(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    TZrFloat64 inverse[9];
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix) ||
        !zr_math_matrix3_inverse(matrix, inverse)) {
        return ZR_FALSE;
    }

    object = ZrMath_MakeMatrix3x3(context->state, inverse);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MulVector(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    ZrMathVector3 vector;
    SZrObject *other = ZR_NULL;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &vector)) {
        return ZR_FALSE;
    }

    object = ZrMath_MakeVector3(context->state,
                                matrix[0] * vector.x + matrix[1] * vector.y + matrix[2] * vector.z,
                                matrix[3] * vector.x + matrix[4] * vector.y + matrix[5] * vector.z,
                                matrix[6] * vector.x + matrix[7] * vector.y + matrix[8] * vector.z);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 lhs[9];
    TZrFloat64 rhs[9];
    TZrFloat64 product[9];
    SZrObject *other = ZR_NULL;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadMatrix3Object(context->state, other, rhs)) {
        return ZR_FALSE;
    }

    zr_math_matrix3_mul(lhs, rhs, product);
    object = ZrMath_MakeMatrix3x3(context->state, product);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *other = ZR_NULL;
    if (!ZrLib_CallContext_ReadObject(context, 0, &other)) {
        return ZR_FALSE;
    }
    if (ZrMath_ObjectTypeEquals(context->state, other, "Vector3")) {
        return ZrMath_Matrix3x3_MulVector(context, result);
    }
    return ZrMath_Matrix3x3_MulMatrix(context, result);
}

TZrBool ZrMath_Matrix3x3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }
    return ZrMath_MakeStringResult(context->state,
                                   result,
                                   "Matrix3x3([%g,%g,%g],[%g,%g,%g],[%g,%g,%g])",
                                   matrix[0],
                                   matrix[1],
                                   matrix[2],
                                   matrix[3],
                                   matrix[4],
                                   matrix[5],
                                   matrix[6],
                                   matrix[7],
                                   matrix[8]);
}
