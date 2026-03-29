//
// Quaternion registry accessors.
//

#ifndef ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H
#define ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H

#include "zr_vm_lib_math/quaternion.h"

const ZrLibTypeDescriptor *ZrMath_QuaternionRegistry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_QuaternionRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_QuaternionRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H
