//
// Built-in zr.math native module registration.
//

#ifndef ZR_VM_LIB_MATH_MODULE_H
#define ZR_VM_LIB_MATH_MODULE_H

#include "zr_vm_library.h"

ZR_API const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void);
ZR_API TZrBool ZrVmLibMath_Register(SZrGlobalState *global);

#endif // ZR_VM_LIB_MATH_MODULE_H
