//
// zr.thread native module registration.
//

#ifndef ZR_VM_THREAD_MODULE_H
#define ZR_VM_THREAD_MODULE_H

#include "zr_vm_lib_thread/conf.h"

ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVmThread_GetModuleDescriptor(void);
ZR_VM_THREAD_API TZrBool ZrVmThread_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_THREAD_MODULE_H
