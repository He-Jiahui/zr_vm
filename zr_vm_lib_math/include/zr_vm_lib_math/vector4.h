//
// Vector4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR4_H
#define ZR_VM_LIB_MATH_VECTOR4_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Vector4 回调由 registry 暴露给 VM；每个操作读取 receiver 快照并把新值写入结果 slot，
 *  不将临时 C 分量结构暴露给调用者。
 */
TZrBool ZrMath_Vector4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 近零长度时返回零向量，其余情况返回新的单位向量。 */
TZrBool ZrMath_Vector4_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Distance(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 依长度平方提供比较协议所需的顺序，不逐分量比较。 */
TZrBool ZrMath_Vector4_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR4_H
