//
// Built-in zr.system native module registration.
//

#ifndef ZR_VM_LIB_SYSTEM_MODULE_H
#define ZR_VM_LIB_SYSTEM_MODULE_H

#include "zr_vm_lib_system/conf.h"

ZR_API const ZrLibModuleDescriptor *ZrVmLibSystem_GetModuleDescriptor(void);
ZR_API TZrBool ZrVmLibSystem_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_SYSTEM_MODULE_H
