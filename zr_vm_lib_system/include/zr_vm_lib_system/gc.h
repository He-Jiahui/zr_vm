//
// zr.system.gc native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_H
#define ZR_VM_LIB_SYSTEM_GC_H

#include "zr_vm_lib_system/conf.h"

ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Enable(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Disable(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetHeapLimit(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetBudget(ZrLibCallContext *context, SZrTypeValue *result);
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_GetStats(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_GC_H
