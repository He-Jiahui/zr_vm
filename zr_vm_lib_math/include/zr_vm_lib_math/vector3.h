//
// Vector3 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR3_H
#define ZR_VM_LIB_MATH_VECTOR3_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Vector3 的方法和 meta 回调由 registry 映射到 ZR 类型；新数值结果由 VM 管理，
 *  `context` 与 `result` 只在一次 native 调用期间有效。
 */
TZrBool ZrMath_Vector3_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 近零长度时返回零向量，其余情况返回新的单位向量。 */
TZrBool ZrMath_Vector3_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Dot(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Distance(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_Cross(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 比较长度平方以建立顺序；与逐分量比较或近似相等不同。 */
TZrBool ZrMath_Vector3_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Vector3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR3_H
