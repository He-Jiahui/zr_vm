//
// Tensor registry accessors.
//

#ifndef ZR_VM_LIB_MATH_TENSOR_REGISTRY_H
#define ZR_VM_LIB_MATH_TENSOR_REGISTRY_H

#include "zr_vm_lib_math/tensor.h"

const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_TensorRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_TensorRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_TENSOR_REGISTRY_H
