//
// Tensor native callbacks.
//

#ifndef ZR_VM_LIB_MATH_TENSOR_H
#define ZR_VM_LIB_MATH_TENSOR_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Tensor 回调由 tensor_registry 绑定给 VM。构造参数是 shape 数组与等长的数值 data 数组；
 *  运算采用 row-major 布局，失败时返回 ZR_FALSE 并由 native dispatcher 处理结果。
 */
/** @brief 复制输入数组并建立 Tensor 存储，避免调用者随后修改原数组影响实例。 */
TZrBool ZrMath_Tensor_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Clone(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 以相同元素数的新 shape 创建独立 Tensor；shape 和 data 均复制。 */
TZrBool ZrMath_Tensor_Reshape(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 原地更新数据，并把当前 Tensor 作为链式调用结果返回。 */
TZrBool ZrMath_Tensor_Fill(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按完整的多维 indices 数组读取一个数值。 */
TZrBool ZrMath_Tensor_Get(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按完整的多维 indices 数组原地写入一个数值。 */
TZrBool ZrMath_Tensor_Set(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sum(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Mean(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 只接受 rank=2 的 Tensor，返回转置后的新实例。 */
TZrBool ZrMath_Tensor_Transpose2D(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 两侧均须为二维且左列数等于右行数，返回矩阵积的新实例。 */
TZrBool ZrMath_Tensor_Matmul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Add(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MulScalar(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 返回数据数组的外层副本，供脚本读取而不暴露内部数组对象。 */
TZrBool ZrMath_Tensor_ToArray(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_TENSOR_H
