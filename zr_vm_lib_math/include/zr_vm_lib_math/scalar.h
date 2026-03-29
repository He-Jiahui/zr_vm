//
// Scalar math callbacks.
//

#ifndef ZR_VM_LIB_MATH_SCALAR_H
#define ZR_VM_LIB_MATH_SCALAR_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Scalar_Abs(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Min(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Max(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Clamp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Sqrt(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Rsqrt(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Pow(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Exp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Log(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Sin(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Cos(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Tan(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Asin(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Acos(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Atan(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Atan2(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Floor(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Ceil(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Round(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Sign(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Degrees(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_Radians(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Scalar_AlmostEqual(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_InvokeCallback(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_SCALAR_H
