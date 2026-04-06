//
// Internal helpers shared by zr.system.fs object/runtime translation units.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_INTERNAL_H
#define ZR_VM_LIB_SYSTEM_FS_INTERNAL_H

#include "zr_vm_lib_system/fs.h"

#include "zr_vm_core/raw_object.h"
#include "zr_vm_library/file.h"

#define ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD "__zr_fs_stream"
#define ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD "__zr_ffi_handleId"

typedef struct ZrSystemFsStreamData {
    TZrLibrary_File_Handle handle;
    TZrBool readable;
    TZrBool writable;
    TZrBool append;
    TZrBool closed;
    TZrBool finalized;
} ZrSystemFsStreamData;

void ZrSystem_Fs_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);
void ZrSystem_Fs_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);
void ZrSystem_Fs_WriteStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName, const TZrChar *value);
void ZrSystem_Fs_WriteNullField(SZrState *state, SZrObject *object, const TZrChar *fieldName);
void ZrSystem_Fs_WriteObjectField(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  SZrObject *fieldObject,
                                  EZrValueType valueType);
const SZrTypeValue *ZrSystem_Fs_GetFieldValue(SZrState *state, SZrObject *object, const TZrChar *fieldName);
const TZrChar *ZrSystem_Fs_GetStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName);
TZrInt64 ZrSystem_Fs_GetIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 defaultValue);
TZrBool ZrSystem_Fs_GetBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool defaultValue);
SZrObject *ZrSystem_Fs_SelfObject(const ZrLibCallContext *context);
SZrObject *ZrSystem_Fs_ResolveConstructTarget(ZrLibCallContext *context);
TZrBool ZrSystem_Fs_FinishObjectResult(SZrState *state, SZrTypeValue *result, SZrObject *object);
TZrBool ZrSystem_Fs_ReadStringArgument(const ZrLibCallContext *context, TZrSize index, const TZrChar **outText);
TZrBool ZrSystem_Fs_ReadOptionalStringArgument(const ZrLibCallContext *context,
                                               TZrSize index,
                                               const TZrChar *defaultValue,
                                               ZR_OUT TZrChar *buffer,
                                               TZrSize bufferSize);
TZrBool ZrSystem_Fs_ReadOptionalBoolArgument(const ZrLibCallContext *context,
                                             TZrSize index,
                                             TZrBool defaultValue,
                                             TZrBool *outValue);
TZrBool ZrSystem_Fs_ReadOptionalIntArgument(const ZrLibCallContext *context,
                                            TZrSize index,
                                            TZrInt64 defaultValue,
                                            TZrInt64 *outValue);
TZrBool ZrSystem_Fs_ReadArrayArgument(const ZrLibCallContext *context, TZrSize index, SZrObject **outArray);

TZrBool ZrSystem_Fs_RaiseIOException(SZrState *state, const TZrChar *format, ...);
TZrBool ZrSystem_Fs_RaiseErrnoIOException(SZrState *state, const TZrChar *action, const TZrChar *path);

SZrObject *ZrSystem_Fs_MakeInfoObject(SZrState *state, const SZrLibrary_File_Info *info);
TZrBool ZrSystem_Fs_PopulateEntryObject(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *originalPath,
                                        const TZrChar *fullPathOverride);
SZrObject *ZrSystem_Fs_NewEntryObject(SZrState *state,
                                      const TZrChar *typeName,
                                      const TZrChar *originalPath,
                                      const TZrChar *fullPathOverride);
TZrBool ZrSystem_Fs_RefreshEntryObject(SZrState *state,
                                       SZrObject *object,
                                       ZR_OUT SZrLibrary_File_Info *outInfo,
                                       ZR_OUT SZrObject **outInfoObject);

SZrObject *ZrSystem_Fs_NewStreamObject(SZrState *state,
                                       const TZrChar *path,
                                       const SZrLibrary_File_StreamOpenResult *openResult);
ZrSystemFsStreamData *ZrSystem_Fs_GetStreamData(SZrState *state, SZrObject *object);
TZrBool ZrSystem_Fs_StreamEnsureOpen(SZrState *state,
                                     SZrObject *object,
                                     ZrSystemFsStreamData **outData);
TZrBool ZrSystem_Fs_StreamSyncFields(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data);
TZrBool ZrSystem_Fs_StreamCloseInternal(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data);
void ZrSystem_Fs_StreamFinalize(SZrState *state, SZrRawObject *rawObject);

TZrBool ZrSystem_Fs_ReadBytesFromHandle(SZrState *state,
                                        TZrLibrary_File_Handle handle,
                                        TZrInt64 count,
                                        SZrTypeValue *result,
                                        ZR_OUT TZrInt64 *outReadCount);
TZrBool ZrSystem_Fs_ReadTextFromHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       TZrInt64 count,
                                       SZrTypeValue *result,
                                       ZR_OUT TZrInt64 *outReadCount);
TZrBool ZrSystem_Fs_WriteBytesToHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       SZrObject *array,
                                       ZR_OUT TZrInt64 *outWrittenCount);
TZrBool ZrSystem_Fs_WriteTextToHandle(SZrState *state,
                                      TZrLibrary_File_Handle handle,
                                      const TZrChar *text,
                                      ZR_OUT TZrInt64 *outWrittenCount);

#endif // ZR_VM_LIB_SYSTEM_FS_INTERNAL_H
