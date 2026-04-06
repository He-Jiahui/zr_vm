//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_FILE_H
#define ZR_VM_LIBRARY_FILE_H

#include <stdio.h>

#include "zr_vm_library/conf.h"

#define ZR_LIBRARY_FILE_BUFFER_SIZE BUFSIZ
#define ZR_LIBRARY_FILE_STREAM_MODE_MAX 8U
#define ZR_LIBRARY_FILE_INVALID_HANDLE (-1)

typedef FILE *TZrLibrary_File_Ptr;
typedef int TZrLibrary_File_Handle;

enum EZrLibrary_File_Exist {
    ZR_LIBRARY_FILE_NOT_EXIST = 0,
    ZR_LIBRARY_FILE_IS_FILE = 1,
    ZR_LIBRARY_FILE_IS_DIRECTORY = 2,
    ZR_LIBRARY_FILE_IS_OTHER = 3
};

typedef enum EZrLibrary_File_Exist EZrLibrary_File_Exist;

enum EZrLibrary_File_Mode {
    ZR_LIBRARY_FILE_MODE_READ = 0,
    ZR_LIBRARY_FILE_MODE_WRITE = 1,
    ZR_LIBRARY_FILE_MODE_APPEND = 2,
    ZR_LIBRARY_FILE_MODE_READ_WRITE = 3,
    ZR_LIBRARY_FILE_MODE_READ_APPEND = 4,
    ZR_LIBRARY_FILE_MODE_WRITE_APPEND = 5,
    ZR_LIBRARY_FILE_MODE_READ_WRITE_APPEND = 6
};

typedef enum EZrLibrary_File_Mode EZrLibrary_File_Mode;

struct ZR_STRUCT_ALIGN SZrLibrary_File_Reader {
    TZrSize size;
    TZrLibrary_File_Ptr file;
    TZrChar buffer[ZR_LIBRARY_FILE_BUFFER_SIZE];
};

typedef struct SZrLibrary_File_Reader SZrLibrary_File_Reader;

typedef struct SZrLibrary_File_Info {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar name[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar extension[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar parentPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrInt64 size;
    TZrInt64 modifiedMilliseconds;
    TZrInt64 createdMilliseconds;
    TZrInt64 accessedMilliseconds;
    EZrLibrary_File_Exist existence;
    TZrBool exists;
} SZrLibrary_File_Info;

typedef struct SZrLibrary_File_ListEntry {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;
} SZrLibrary_File_ListEntry;

typedef struct SZrLibrary_File_List {
    SZrLibrary_File_ListEntry *entries;
    TZrSize count;
    TZrSize capacity;
} SZrLibrary_File_List;

typedef struct SZrLibrary_File_StreamOpenResult {
    TZrLibrary_File_Handle handle;
    TZrBool readable;
    TZrBool writable;
    TZrBool append;
    TZrChar normalizedMode[ZR_LIBRARY_FILE_STREAM_MODE_MAX];
} SZrLibrary_File_StreamOpenResult;

ZR_LIBRARY_API EZrLibrary_File_Exist ZrLibrary_File_Exist(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_IsAbsolutePath(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_NormalizePath(TZrNativeString path,
                                                    ZR_OUT TZrNativeString normalizedPath,
                                                    TZrSize normalizedPathSize);

ZR_LIBRARY_API TZrBool ZrLibrary_File_GetDirectory(TZrNativeString path, ZR_OUT TZrNativeString directory);

ZR_LIBRARY_API TZrBool ZrLibrary_File_QueryInfo(TZrNativeString path, ZR_OUT SZrLibrary_File_Info *outInfo);

ZR_LIBRARY_API void ZrLibrary_File_PathJoin(const TZrChar *path1, const TZrChar *path2, ZR_OUT TZrNativeString result);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateDirectorySingle(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateDirectories(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateEmpty(TZrNativeString path, TZrBool recursively);

ZR_LIBRARY_API TZrBool ZrLibrary_File_Delete(TZrNativeString path, TZrBool recursively);

ZR_LIBRARY_API TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath,
                                           TZrNativeString targetPath,
                                           TZrBool overwrite);

ZR_LIBRARY_API TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath,
                                           TZrNativeString targetPath,
                                           TZrBool overwrite);

ZR_LIBRARY_API TZrBool ZrLibrary_File_ListDirectory(TZrNativeString path,
                                                    TZrBool recursively,
                                                    ZR_OUT SZrLibrary_File_List *outList);

ZR_LIBRARY_API TZrBool ZrLibrary_File_Glob(TZrNativeString path,
                                           TZrNativeString pattern,
                                           TZrBool recursively,
                                           ZR_OUT SZrLibrary_File_List *outList);

ZR_LIBRARY_API void ZrLibrary_File_List_Free(ZR_OUT SZrLibrary_File_List *list);

ZR_LIBRARY_API TZrBool ZrLibrary_File_OpenHandle(TZrNativeString path,
                                                 TZrNativeString mode,
                                                 ZR_OUT SZrLibrary_File_StreamOpenResult *outResult);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CloseHandle(TZrLibrary_File_Handle handle);

ZR_LIBRARY_API TZrBool ZrLibrary_File_ReadHandle(TZrLibrary_File_Handle handle,
                                                 void *buffer,
                                                 TZrSize requestedSize,
                                                 ZR_OUT TZrSize *outReadSize);

ZR_LIBRARY_API TZrBool ZrLibrary_File_WriteHandle(TZrLibrary_File_Handle handle,
                                                  const void *buffer,
                                                  TZrSize requestedSize,
                                                  ZR_OUT TZrSize *outWrittenSize);

ZR_LIBRARY_API TZrBool ZrLibrary_File_SeekHandle(TZrLibrary_File_Handle handle,
                                                 TZrInt64 offset,
                                                 int origin,
                                                 ZR_OUT TZrInt64 *outPosition);

ZR_LIBRARY_API TZrBool ZrLibrary_File_GetHandlePosition(TZrLibrary_File_Handle handle,
                                                        ZR_OUT TZrInt64 *outPosition);

ZR_LIBRARY_API TZrBool ZrLibrary_File_GetHandleLength(TZrLibrary_File_Handle handle,
                                                      ZR_OUT TZrInt64 *outLength);

ZR_LIBRARY_API TZrBool ZrLibrary_File_SetHandleLength(TZrLibrary_File_Handle handle, TZrInt64 length);

ZR_LIBRARY_API TZrBool ZrLibrary_File_FlushHandle(TZrLibrary_File_Handle handle);

ZR_LIBRARY_API TZrNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TZrNativeString path);

ZR_LIBRARY_API SZrLibrary_File_Reader *ZrLibrary_File_OpenRead(SZrGlobalState *global,
                                                               TZrNativeString path,
                                                               TZrBool isBinary);

ZR_LIBRARY_API void ZrLibrary_File_CloseRead(SZrGlobalState *global, SZrLibrary_File_Reader *reader);

ZR_LIBRARY_API TZrBool ZrLibrary_File_SourceLoadImplementation(SZrState *state,
                                                               TZrNativeString path,
                                                               TZrNativeString md5,
                                                               SZrIo *io);

ZR_LIBRARY_API TZrBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state,
                                                                  TZrPtr reader,
                                                                  ZR_OUT TZrSize *size);

ZR_LIBRARY_API void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader);

#endif // ZR_VM_LIBRARY_FILE_H
