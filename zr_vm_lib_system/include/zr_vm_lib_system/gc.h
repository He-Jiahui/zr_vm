//
// zr.system.gc native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_H
#define ZR_VM_LIB_SYSTEM_GC_H

#include "zr_vm_lib_system/conf.h"

TZrBool ZrSystem_Gc_Start(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Gc_Stop(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Gc_Step(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_GC_H
