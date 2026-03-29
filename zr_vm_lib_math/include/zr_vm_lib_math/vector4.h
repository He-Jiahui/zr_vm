//
// Vector4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR4_H
#define ZR_VM_LIB_MATH_VECTOR4_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Vector4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Distance(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR4_H
