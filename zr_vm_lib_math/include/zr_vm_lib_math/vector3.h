//
// Vector3 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR3_H
#define ZR_VM_LIB_MATH_VECTOR3_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Vector3_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Distance(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Cross(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR3_H
