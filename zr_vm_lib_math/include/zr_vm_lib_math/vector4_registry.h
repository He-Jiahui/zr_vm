//
// Vector4 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H

#include "zr_vm_lib_math/vector4.h"

const ZrLibTypeDescriptor *ZrMath_Vector4Registry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_Vector4Registry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_Vector4Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR4_REGISTRY_H
