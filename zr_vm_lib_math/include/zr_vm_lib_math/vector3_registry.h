//
// Vector3 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H

#include "zr_vm_lib_math/vector3.h"

const ZrLibTypeDescriptor *ZrMath_Vector3Registry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_Vector3Registry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_Vector3Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR3_REGISTRY_H
