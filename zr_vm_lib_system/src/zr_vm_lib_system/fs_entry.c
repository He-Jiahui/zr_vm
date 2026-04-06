//
// FileSystemEntry, File, Folder, and compatibility surface for zr.system.fs.
//

#include "fs_internal.h"

#include <errno.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <direct.h>
#else
#include <unistd.h>
#endif

static int system_fs_change_directory_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _chdir(path);
#else
    return chdir(path);
#endif
}

static TZrBool system_fs_self_full_path(SZrState *state, SZrObject *object, const TZrChar **outPath) {
    if (outPath != ZR_NULL) {
        *outPath = ZR_NULL;
    }
    if (state == ZR_NULL || object == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }
    *outPath = ZrSystem_Fs_GetStringField(state, object, "fullPath");
    return *outPath != ZR_NULL;
}

static TZrBool system_fs_read_text_file(SZrState *state, const TZrChar *path, SZrTypeValue *result) {
    SZrLibrary_File_StreamOpenResult openResult;
    TZrBool success;
    int savedErrno = 0;

    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)path, "r", &openResult)) {
        return ZR_FALSE;
    }

    success = ZrSystem_Fs_ReadTextFromHandle(state, openResult.handle, -1, result, ZR_NULL);
    savedErrno = errno;
    if (!ZrLibrary_File_CloseHandle(openResult.handle) && success) {
        return ZR_FALSE;
    }
    if (!success) {
        errno = savedErrno;
    }
    return success;
}

static TZrBool system_fs_read_bytes_file(SZrState *state, const TZrChar *path, SZrTypeValue *result) {
    SZrLibrary_File_StreamOpenResult openResult;
    TZrBool success;
    int savedErrno = 0;

    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)path, "rb", &openResult)) {
        return ZR_FALSE;
    }

    success = ZrSystem_Fs_ReadBytesFromHandle(state, openResult.handle, -1, result, ZR_NULL);
    savedErrno = errno;
    if (!ZrLibrary_File_CloseHandle(openResult.handle) && success) {
        return ZR_FALSE;
    }
    if (!success) {
        errno = savedErrno;
    }
    return success;
}

static TZrBool system_fs_write_text_file(SZrState *state,
                                         const TZrChar *path,
                                         const TZrChar *mode,
                                         const TZrChar *text,
                                         TZrInt64 *outWritten) {
    SZrLibrary_File_StreamOpenResult openResult;
    TZrBool success;
    int savedErrno = 0;

    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)path, (TZrNativeString)mode, &openResult)) {
        return ZR_FALSE;
    }

    success = ZrSystem_Fs_WriteTextToHandle(state, openResult.handle, text, outWritten);
    savedErrno = errno;
    if (!ZrLibrary_File_CloseHandle(openResult.handle) && success) {
        return ZR_FALSE;
    }
    if (!success) {
        errno = savedErrno;
    }
    return success;
}

static TZrBool system_fs_write_bytes_file(SZrState *state,
                                          const TZrChar *path,
                                          const TZrChar *mode,
                                          SZrObject *array,
                                          TZrInt64 *outWritten) {
    SZrLibrary_File_StreamOpenResult openResult;
    TZrBool success;
    int savedErrno = 0;

    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)path, (TZrNativeString)mode, &openResult)) {
        return ZR_FALSE;
    }

    success = ZrSystem_Fs_WriteBytesToHandle(state, openResult.handle, array, outWritten);
    savedErrno = errno;
    if (!ZrLibrary_File_CloseHandle(openResult.handle) && success) {
        return ZR_FALSE;
    }
    if (!success) {
        errno = savedErrno;
    }
    return success;
}

static TZrBool system_fs_make_entry_array(SZrState *state,
                                          const SZrLibrary_File_List *list,
                                          TZrBool filesOnly,
                                          TZrBool foldersOnly,
                                          SZrTypeValue *result) {
    SZrObject *array;
    TZrSize index;

    if (state == ZR_NULL || list == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < list->count; index++) {
        SZrObject *entryObject = ZR_NULL;
        SZrTypeValue entryValue;
        if (filesOnly && list->entries[index].existence != ZR_LIBRARY_FILE_IS_FILE) {
            continue;
        }
        if (foldersOnly && list->entries[index].existence != ZR_LIBRARY_FILE_IS_DIRECTORY) {
            continue;
        }

        entryObject = ZrSystem_Fs_NewEntryObject(state,
                                                 list->entries[index].existence == ZR_LIBRARY_FILE_IS_DIRECTORY
                                                         ? "Folder"
                                                         : (list->entries[index].existence == ZR_LIBRARY_FILE_IS_FILE
                                                                    ? "File"
                                                                    : "FileSystemEntry"),
                                                 list->entries[index].path,
                                                 list->entries[index].path);
        if (entryObject == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrLib_Value_SetObject(state, &entryValue, entryObject, ZR_VALUE_TYPE_OBJECT);
        if (!ZrLib_Array_PushValue(state, array, &entryValue)) {
            return ZR_FALSE;
        }
    }

    ZrLib_Value_SetObject(state, result, array, ZR_VALUE_TYPE_ARRAY);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(".", path, sizeof(path))) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetString(context->state, result, path);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, system_fs_change_directory_native(path) == 0);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, ZrLibrary_File_Exist((TZrNativeString)path) != ZR_LIBRARY_FILE_NOT_EXIST);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, ZrLibrary_File_Exist((TZrNativeString)path) == ZR_LIBRARY_FILE_IS_FILE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result,
                        ZrLibrary_File_Exist((TZrNativeString)path) == ZR_LIBRARY_FILE_IS_DIRECTORY);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_CreateDirectorySingle((TZrNativeString)path)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "createDirectory", path);
    }
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_CreateDirectories((TZrNativeString)path)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "createDirectories", path);
    }
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    EZrLibrary_File_Exist existence;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    existence = ZrLibrary_File_Exist((TZrNativeString)path);
    if (existence == ZR_LIBRARY_FILE_IS_DIRECTORY) {
        if (!ZrLibrary_File_Delete((TZrNativeString)path, ZR_FALSE)) {
            return ZrSystem_Fs_RaiseErrnoIOException(context->state, "removePath", path);
        }
    } else if (existence == ZR_LIBRARY_FILE_IS_FILE || existence == ZR_LIBRARY_FILE_IS_OTHER) {
        if (!ZrLibrary_File_Delete((TZrNativeString)path, ZR_FALSE)) {
            return ZrSystem_Fs_RaiseErrnoIOException(context->state, "removePath", path);
        }
    } else {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "removePath", path);
    }
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    if (!system_fs_read_text_file(context->state, path, result)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "readText", path);
    }
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    const TZrChar *text = ZR_NULL;
    TZrInt64 ignored = 0;
    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &path) ||
        !ZrSystem_Fs_ReadStringArgument(context, 1, &text)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_text_file(context->state, path, "w", text, &ignored)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "writeText", path);
    }
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    const TZrChar *text = ZR_NULL;
    TZrInt64 ignored = 0;
    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &path) ||
        !ZrSystem_Fs_ReadStringArgument(context, 1, &text)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_text_file(context->state, path, "a", text, &ignored)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "appendText", path);
    }
    ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    SZrLibrary_File_Info info;
    SZrObject *infoObject;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    memset(&info, 0, sizeof(info));
    if (!ZrLibrary_File_QueryInfo((TZrNativeString)path, &info)) {
        return ZR_FALSE;
    }
    infoObject = ZrSystem_Fs_MakeInfoObject(context->state, &info);
    if (infoObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, infoObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Entry_Constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    SZrObject *object;
    if (context == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_ReadStringArgument(context, 0, &path)) {
        return ZR_FALSE;
    }
    object = ZrSystem_Fs_ResolveConstructTarget(context);
    if (object == ZR_NULL || !ZrSystem_Fs_PopulateEntryObject(context->state, object, path, ZR_NULL)) {
        return ZR_FALSE;
    }
    return ZrSystem_Fs_FinishObjectResult(context->state, result, object);
}

TZrBool ZrSystem_Fs_Entry_Exists(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    SZrLibrary_File_Info info;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_RefreshEntryObject(context->state, self, &info, ZR_NULL)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, info.exists);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Entry_Refresh(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const SZrTypeValue *fileInfoValue;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL)) {
        return ZR_FALSE;
    }
    fileInfoValue = ZrSystem_Fs_GetFieldValue(context->state, self, "fileInfo");
    if (fileInfoValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *result = *fileInfoValue;
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    TZrChar modeBuffer[ZR_LIBRARY_FILE_STREAM_MODE_MAX];
    SZrLibrary_File_StreamOpenResult openResult;
    SZrObject *streamObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadOptionalStringArgument(context, 0, "r", modeBuffer, sizeof(modeBuffer))) {
        return ZR_FALSE;
    }
    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)fullPath, modeBuffer, &openResult)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "open", fullPath);
    }
    streamObject = ZrSystem_Fs_NewStreamObject(context->state, fullPath, &openResult);
    if (streamObject == ZR_NULL) {
        ZrLibrary_File_CloseHandle(openResult.handle);
        return ZR_FALSE;
    }
    return ZrSystem_Fs_FinishObjectResult(context->state, result, streamObject);
}

TZrBool ZrSystem_Fs_File_Create(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    TZrBool recursively = ZR_TRUE;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 0, ZR_TRUE, &recursively)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_CreateEmpty((TZrNativeString)fullPath, recursively)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "create", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL || !system_fs_self_full_path(context->state, self, &fullPath)) {
        return ZR_FALSE;
    }
    if (!system_fs_read_text_file(context->state, fullPath, result)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "readText", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_WriteText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *text = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &text)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_text_file(context->state, fullPath, "w", text, &written)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "writeText", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_AppendText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *text = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &text)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_text_file(context->state, fullPath, "a", text, &written)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "appendText", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL || !system_fs_self_full_path(context->state, self, &fullPath)) {
        return ZR_FALSE;
    }
    if (!system_fs_read_bytes_file(context->state, fullPath, result)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "readBytes", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    SZrObject *array = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadArrayArgument(context, 0, &array)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_bytes_file(context->state, fullPath, "w", array, &written)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "writeBytes", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_AppendBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    SZrObject *array = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadArrayArgument(context, 0, &array)) {
        return ZR_FALSE;
    }
    if (!system_fs_write_bytes_file(context->state, fullPath, "a", array, &written)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "appendBytes", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *copyObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &targetPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_FALSE, &overwrite)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Copy((TZrNativeString)fullPath, (TZrNativeString)targetPath, overwrite)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "copyTo", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    copyObject = ZrSystem_Fs_NewEntryObject(context->state, "File", targetPath, ZR_NULL);
    return copyObject != ZR_NULL && ZrSystem_Fs_FinishObjectResult(context->state, result, copyObject);
}

TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *movedObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &targetPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_FALSE, &overwrite)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Move((TZrNativeString)fullPath, (TZrNativeString)targetPath, overwrite)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "moveTo", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    movedObject = ZrSystem_Fs_NewEntryObject(context->state, "File", targetPath, ZR_NULL);
    return movedObject != ZR_NULL && ZrSystem_Fs_FinishObjectResult(context->state, result, movedObject);
}

TZrBool ZrSystem_Fs_File_Delete(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL || !system_fs_self_full_path(context->state, self, &fullPath)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Delete((TZrNativeString)fullPath, ZR_FALSE)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "delete", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Folder_Create(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    TZrBool recursively = ZR_TRUE;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 0, ZR_TRUE, &recursively)) {
        return ZR_FALSE;
    }
    if (!(recursively ? ZrLibrary_File_CreateDirectories((TZrNativeString)fullPath)
                      : ZrLibrary_File_CreateDirectorySingle((TZrNativeString)fullPath))) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "create", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool system_fs_folder_list_common(ZrLibCallContext *context,
                                            SZrTypeValue *result,
                                            TZrBool filesOnly,
                                            TZrBool foldersOnly) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    SZrLibrary_File_List list;
    TZrBool ok;
    memset(&list, 0, sizeof(list));
    if (self == ZR_NULL || result == ZR_NULL || !system_fs_self_full_path(context->state, self, &fullPath)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_ListDirectory((TZrNativeString)fullPath, ZR_FALSE, &list)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "entries", fullPath);
    }
    ok = system_fs_make_entry_array(context->state, &list, filesOnly, foldersOnly, result);
    ZrLibrary_File_List_Free(&list);
    return ok;
}

TZrBool ZrSystem_Fs_Folder_Entries(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_fs_folder_list_common(context, result, ZR_FALSE, ZR_FALSE);
}

TZrBool ZrSystem_Fs_Folder_Files(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_fs_folder_list_common(context, result, ZR_TRUE, ZR_FALSE);
}

TZrBool ZrSystem_Fs_Folder_Folders(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_fs_folder_list_common(context, result, ZR_FALSE, ZR_TRUE);
}

TZrBool ZrSystem_Fs_Folder_Glob(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *pattern = ZR_NULL;
    TZrBool recursively = ZR_TRUE;
    SZrLibrary_File_List list;
    TZrBool ok;
    memset(&list, 0, sizeof(list));
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &pattern) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_TRUE, &recursively)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Glob((TZrNativeString)fullPath, (TZrNativeString)pattern, recursively, &list)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "glob", fullPath);
    }
    ok = system_fs_make_entry_array(context->state, &list, ZR_FALSE, ZR_FALSE, result);
    ZrLibrary_File_List_Free(&list);
    return ok;
}

TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *copyObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &targetPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_FALSE, &overwrite)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Copy((TZrNativeString)fullPath, (TZrNativeString)targetPath, overwrite)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "copyTo", fullPath);
    }
    copyObject = ZrSystem_Fs_NewEntryObject(context->state, "Folder", targetPath, ZR_NULL);
    return copyObject != ZR_NULL && ZrSystem_Fs_FinishObjectResult(context->state, result, copyObject);
}

TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *movedObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &targetPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_FALSE, &overwrite)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Move((TZrNativeString)fullPath, (TZrNativeString)targetPath, overwrite)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "moveTo", fullPath);
    }
    movedObject = ZrSystem_Fs_NewEntryObject(context->state, "Folder", targetPath, ZR_NULL);
    return movedObject != ZR_NULL && ZrSystem_Fs_FinishObjectResult(context->state, result, movedObject);
}

TZrBool ZrSystem_Fs_Folder_Delete(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    TZrBool recursively = ZR_FALSE;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 0, ZR_FALSE, &recursively)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Delete((TZrNativeString)fullPath, recursively)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "delete", fullPath);
    }
    ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, ZR_NULL);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}
