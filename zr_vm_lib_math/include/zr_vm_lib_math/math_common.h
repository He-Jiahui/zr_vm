//
// Shared helpers for zr.math split implementation.
//

#ifndef ZR_VM_LIB_MATH_COMMON_H
#define ZR_VM_LIB_MATH_COMMON_H

#include "zr_vm_lib_math/module.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

typedef struct ZrMathVector2 {
    TZrFloat64 x;
    TZrFloat64 y;
} ZrMathVector2;

typedef struct ZrMathVector3 {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
} ZrMathVector3;

typedef struct ZrMathVector4 {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
    TZrFloat64 w;
} ZrMathVector4;

typedef struct ZrMathQuaternion {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
    TZrFloat64 w;
} ZrMathQuaternion;

typedef struct ZrMathComplex {
    TZrFloat64 real;
    TZrFloat64 imag;
} ZrMathComplex;

typedef struct ZrMathTensorStorage {
    SZrObject *shape;
    SZrObject *data;
    TZrSize rank;
    TZrInt64 size;
} ZrMathTensorStorage;

TZrFloat64 ZrMath_AbsFloat(TZrFloat64 value);
TZrBool ZrMath_AlmostEqual(TZrFloat64 lhs, TZrFloat64 rhs, TZrFloat64 epsilon);
TZrFloat64 ZrMath_Dot(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrSize count);

TZrBool ZrMath_NumberFromValue(const SZrTypeValue *value, TZrFloat64 *outValue);
TZrBool ZrMath_IntFromValue(const SZrTypeValue *value, TZrInt64 *outValue);
TZrBool ZrMath_ReadFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 *outValue);
void ZrMath_WriteFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 value);
void ZrMath_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);
void ZrMath_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);

SZrObject *ZrMath_SelfObject(ZrLibCallContext *context);
TZrBool ZrMath_ObjectTypeEquals(SZrState *state, SZrObject *object, const TZrChar *typeName);
SZrObject *ZrMath_ResolveConstructTarget(ZrLibCallContext *context);
TZrBool ZrMath_FinishConstructObject(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object);
TZrBool ZrMath_ConstructFloatObject(ZrLibCallContext *context,
                                    SZrTypeValue *result,
                                    const TZrChar *const *fieldNames,
                                    const TZrFloat64 *fieldValues,
                                    TZrSize fieldCount);

SZrObject *ZrMath_MakeVector2(SZrState *state, TZrFloat64 x, TZrFloat64 y);
SZrObject *ZrMath_MakeVector3(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z);
SZrObject *ZrMath_MakeVector4(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
SZrObject *ZrMath_MakeQuaternion(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
SZrObject *ZrMath_MakeComplex(SZrState *state, TZrFloat64 real, TZrFloat64 imag);
SZrObject *ZrMath_MakeMatrix3x3(SZrState *state, const TZrFloat64 *values);
SZrObject *ZrMath_MakeMatrix4x4(SZrState *state, const TZrFloat64 *values);

TZrBool ZrMath_ReadVector2Object(SZrState *state, SZrObject *object, ZrMathVector2 *outValue);
TZrBool ZrMath_ReadVector3Object(SZrState *state, SZrObject *object, ZrMathVector3 *outValue);
TZrBool ZrMath_ReadVector4Object(SZrState *state, SZrObject *object, ZrMathVector4 *outValue);
TZrBool ZrMath_ReadQuaternionObject(SZrState *state, SZrObject *object, ZrMathQuaternion *outValue);
TZrBool ZrMath_ReadComplexObject(SZrState *state, SZrObject *object, ZrMathComplex *outValue);
TZrBool ZrMath_ReadMatrix3Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);
TZrBool ZrMath_ReadMatrix4Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);

TZrBool ZrMath_ArrayReadFloat(SZrState *state, SZrObject *array, TZrSize index, TZrFloat64 *outValue);
TZrBool ZrMath_ArrayReadInt(SZrState *state, SZrObject *array, TZrSize index, TZrInt64 *outValue);
TZrBool ZrMath_ArraySetValue(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value);

TZrBool ZrMath_TensorGetStorage(SZrState *state, SZrObject *tensor, ZrMathTensorStorage *outStorage);
TZrBool ZrMath_TensorPopulate(SZrState *state, SZrObject *tensor, SZrObject *shapeArray, SZrObject *dataArray);
SZrObject *ZrMath_TensorMake(SZrState *state, SZrObject *shapeArray, SZrObject *dataArray);
TZrBool ZrMath_TensorShapeEquals(SZrState *state, SZrObject *lhsShape, SZrObject *rhsShape);
TZrBool ZrMath_TensorComputeOffset(SZrState *state, SZrObject *shape, SZrObject *indices, TZrSize *outOffset);
TZrInt64 ZrMath_TensorTotalSize(SZrState *state, SZrObject *shapeArray);
SZrObject *ZrMath_TensorMakeZeroData(SZrState *state, TZrInt64 size);

TZrBool ZrMath_MakeStringResult(SZrState *state, SZrTypeValue *result, const TZrChar *format, ...);

#endif // ZR_VM_LIB_MATH_COMMON_H
