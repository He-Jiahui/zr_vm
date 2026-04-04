//
// Built-in zr.task native module registration.
//

#ifndef ZR_VM_TASK_MODULE_H
#define ZR_VM_TASK_MODULE_H

#include "zr_vm_task/conf.h"

ZR_VM_TASK_API const ZrLibModuleDescriptor *ZrVmTask_GetModuleDescriptor(void);
ZR_VM_TASK_API TZrBool ZrVmTask_Register(SZrGlobalState *global);

#if defined(ZR_LIBRARY_TYPE_SHARED)
ZR_VM_TASK_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif // ZR_VM_TASK_MODULE_H
