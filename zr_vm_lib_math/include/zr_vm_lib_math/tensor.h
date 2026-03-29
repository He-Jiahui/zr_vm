//
// Tensor native callbacks.
//

#ifndef ZR_VM_LIB_MATH_TENSOR_H
#define ZR_VM_LIB_MATH_TENSOR_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Tensor_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Clone(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Reshape(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Fill(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Get(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Set(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sum(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Mean(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Transpose2D(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Matmul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Add(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MulScalar(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_ToArray(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_TENSOR_H
