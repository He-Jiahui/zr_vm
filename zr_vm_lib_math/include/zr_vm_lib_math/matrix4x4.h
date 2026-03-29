//
// Matrix4x4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_MATRIX4X4_H
#define ZR_VM_LIB_MATH_MATRIX4X4_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Matrix4x4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Identity(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Transpose(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Determinant(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MulVector(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Translation(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Scale(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationX(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationY(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationZ(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_MATRIX4X4_H
