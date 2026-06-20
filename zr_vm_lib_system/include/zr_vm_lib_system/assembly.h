//
// zr.system.assembly native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_ASSEMBLY_H
#define ZR_VM_LIB_SYSTEM_ASSEMBLY_H

#include "zr_vm_lib_system/conf.h"

TZrBool ZrSystem_Assembly_ResourceExists(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Assembly_ReadResourceText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Assembly_ReadResourceBytes(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_ASSEMBLY_H
