//
// Quaternion native callbacks.
//

#ifndef ZR_VM_LIB_MATH_QUATERNION_H
#define ZR_VM_LIB_MATH_QUATERNION_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Quaternion 的 VM 回调表接口。乘法采用 Hamilton 积；零长度归一化或求逆采用
 *  单位四元数回退。数值回调读取对象字段，不维持输入为单位四元数的约束。
 */
TZrBool ZrMath_Quaternion_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_Mul(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 单位端点时按短球面弧插值；调用方若需要单位结果须自行核实输入和结果范数。 */
TZrBool ZrMath_Quaternion_Slerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief VM 比较协议按模长平方排序，不按四个分量或旋转等价关系排序。 */
TZrBool ZrMath_Quaternion_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Quaternion_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_QUATERNION_H
