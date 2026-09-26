//
// Matrix3x3 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_MATRIX3X3_H
#define ZR_VM_LIB_MATH_MATRIX3X3_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Matrix3x3 的 VM 回调表接口。分量按 row-major 存储，乘法解释为矩阵乘列向量；
 *  构造器接受零参数单位矩阵或完整九个分量，运算结果写入 result。
 */
TZrBool ZrMath_Matrix3x3_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_Identity(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_Transpose(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_Determinant(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 求逆并返回新实例；零或近零行列式使回调失败。 */
TZrBool ZrMath_Matrix3x3_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_MulVector(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief `*` 的分派入口：右对象原型名为 Vector3 时走向量乘法，其余按 Matrix3x3 读取。 */
TZrBool ZrMath_Matrix3x3_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Matrix3x3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_MATRIX3X3_H
