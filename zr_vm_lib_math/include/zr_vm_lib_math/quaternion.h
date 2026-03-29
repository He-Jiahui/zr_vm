//
// Quaternion native callbacks.
//

#ifndef ZR_VM_LIB_MATH_QUATERNION_H
#define ZR_VM_LIB_MATH_QUATERNION_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Quaternion_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Mul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Slerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_QUATERNION_H
