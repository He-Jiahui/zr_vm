#ifndef ZR_VM_LIB_NETWORK_MODULE_H
#define ZR_VM_LIB_NETWORK_MODULE_H

#include "zr_vm_lib_network/conf.h"
#include "zr_vm_library/native_binding.h"

ZR_NETWORK_API const ZrLibModuleDescriptor *ZrVmLibNetwork_GetModuleDescriptor(void);
ZR_NETWORK_API TZrBool ZrVmLibNetwork_Register(SZrGlobalState *global);

#endif
