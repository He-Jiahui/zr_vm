//
// Scalar registry accessors.
//

#ifndef ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
#define ZR_VM_LIB_MATH_SCALAR_REGISTRY_H

#include "zr_vm_lib_math/scalar.h"

const ZrLibFunctionDescriptor *ZrMath_ScalarRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_ScalarRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
