//
// Vector2 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H

#include "zr_vm_lib_math/vector2.h"

const ZrLibTypeDescriptor *ZrMath_Vector2Registry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_Vector2Registry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_Vector2Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H
