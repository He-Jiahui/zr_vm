//
// zr.system.vm native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_VM_H
#define ZR_VM_LIB_SYSTEM_VM_H

#include "zr_vm_lib_system/conf.h"

TZrBool ZrSystem_Vm_LoadedModules(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Vm_State(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Vm_CallModuleExport(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_VM_H
