//
// Vector2 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR2_H
#define ZR_VM_LIB_MATH_VECTOR2_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Vector2_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_Distance(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector2_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR2_H
