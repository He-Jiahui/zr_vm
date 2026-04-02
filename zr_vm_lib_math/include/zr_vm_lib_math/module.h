//
// Built-in zr.math native module registration.
//

#ifndef ZR_VM_LIB_MATH_MODULE_H
#define ZR_VM_LIB_MATH_MODULE_H

#include "zr_vm_library.h"

ZR_API const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void);
ZR_API TZrBool ZrVmLibMath_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_MATH_MODULE_H
