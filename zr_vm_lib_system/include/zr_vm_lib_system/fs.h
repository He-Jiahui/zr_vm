//
// zr.system.fs native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_H
#define ZR_VM_LIB_SYSTEM_FS_H

#include "zr_vm_lib_system/conf.h"

TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_FS_H
