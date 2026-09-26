//
// Matrix4x4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_MATRIX4X4_H
#define ZR_VM_LIB_MATH_MATRIX4X4_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Matrix4x4 的 VM 回调表接口。分量按 row-major 存储，使用列向量；平移项位于
 *  m03、m13、m23，旋转工厂接收弧度。构造器接受零参数或完整十六个分量。
 */
TZrBool ZrMath_Matrix4x4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Identity(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Transpose(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Determinant(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 求逆并返回新实例；主元未超过固定阈值时回调失败。 */
TZrBool ZrMath_Matrix4x4_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MulVector(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Translation(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_Scale(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationX(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationY(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_RotationZ(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief `*` 的分派入口：确切 Vector4 原型走向量乘法，其余按 Matrix4x4 读取。 */
TZrBool ZrMath_Matrix4x4_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix4x4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_MATRIX4X4_H
