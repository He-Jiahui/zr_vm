//
// zr.system.process native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_PROCESS_H
#define ZR_VM_LIB_SYSTEM_PROCESS_H

#include "zr_vm_library.h"

TZrBool ZrSystem_Process_SleepMilliseconds(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Process_Exit(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_PROCESS_H
