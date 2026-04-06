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

TZrBool ZrSystem_Fs_Entry_Constructor(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Entry_Exists(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Entry_Refresh(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_Create(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_AppendBytes(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_File_Delete(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrSystem_Fs_Folder_Create(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_Entries(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_Files(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_Folders(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_Glob(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Folder_Delete(ZrLibCallContext *context, SZrTypeValue *result);

TZrBool ZrSystem_Fs_Stream_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_Flush(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_Seek(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_SetLength(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrSystem_Fs_Stream_Close(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_FS_H
