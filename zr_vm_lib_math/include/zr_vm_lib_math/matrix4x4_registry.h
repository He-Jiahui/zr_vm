//
// Matrix4x4 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H
#define ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H

#include "zr_vm_lib_math/matrix4x4.h"

const ZrLibTypeDescriptor *ZrMath_Matrix4x4Registry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_Matrix4x4Registry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_Matrix4x4Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H
