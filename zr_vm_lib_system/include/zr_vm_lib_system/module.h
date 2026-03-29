//
// Built-in zr.system native module registration.
//

#ifndef ZR_VM_LIB_SYSTEM_MODULE_H
#define ZR_VM_LIB_SYSTEM_MODULE_H

#include "zr_vm_library.h"

ZR_API const ZrLibModuleDescriptor *ZrVmLibSystem_GetModuleDescriptor(void);
ZR_API TZrBool ZrVmLibSystem_Register(SZrGlobalState *global);

#endif // ZR_VM_LIB_SYSTEM_MODULE_H
