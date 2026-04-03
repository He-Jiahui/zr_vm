//
// zr.system.console native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_CONSOLE_H
#define ZR_VM_LIB_SYSTEM_CONSOLE_H

#include "zr_vm_lib_system/conf.h"

TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_CONSOLE_H
