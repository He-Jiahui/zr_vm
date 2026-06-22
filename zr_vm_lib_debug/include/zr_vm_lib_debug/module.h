//
// Built-in script-level debug native module registration.
//

#ifndef ZR_VM_LIB_DEBUG_MODULE_H
#define ZR_VM_LIB_DEBUG_MODULE_H

#include "zr_vm_lib_debug/conf.h"

#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"

ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVmLibDebug_GetModuleDescriptor(void);
ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVmLibDebug_GetSandboxedModuleDescriptor(void);
ZR_DEBUG_API TZrBool ZrVmLibDebug_Register(SZrGlobalState *global);
ZR_DEBUG_API TZrBool ZrVmLibDebug_RegisterSandboxed(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_DEBUG_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_LIB_DEBUG_MODULE_H
