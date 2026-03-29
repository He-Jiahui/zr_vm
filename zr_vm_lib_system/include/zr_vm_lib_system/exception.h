//
// zr.system.exception native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_EXCEPTION_H
#define ZR_VM_LIB_SYSTEM_EXCEPTION_H

#include "zr_vm_library.h"

TZrBool ZrSystem_Exception_RegisterUnhandledException(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Exception_Constructor(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_EXCEPTION_H
