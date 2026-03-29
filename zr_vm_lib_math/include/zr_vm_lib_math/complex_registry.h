//
// Complex registry accessors.
//

#ifndef ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
#define ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H

#include "zr_vm_lib_math/complex.h"

const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
