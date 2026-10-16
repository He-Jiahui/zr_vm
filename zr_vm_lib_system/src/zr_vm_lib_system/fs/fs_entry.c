//
// FileSystemEntry, File, Folder, and compatibility surface for zr.system.fs.
//

#include "fs/fs_internal.h"
#include "zr_vm_core/exception.h"

#include <errno.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <direct.h>
#else
#include <unistd.h>
#endif

/* 兼容 API 直接改变宿主进程工作目录，因此不同 VM/state 也共享这项副作用。 */
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

/* 一次性文件 API 自己拥有临时句柄；保存原 I/O errno，避免清理掩盖主要失败原因。 */
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

/* 平台目录列表在调用方转换完成后释放；返回数组中的每个条目独立持有路径快照。 */
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

    /* TODO: array 仅保存在 C 局部变量中，逐项 NewEntryObject 会分配 VM 对象。
     * 需用 GC 压力场景验证 Folder.entries/glob 构造期间的存活性和地址更新；
     * ZrLib_Array_PushValue 的 pin 仅覆盖单次 push，不能覆盖相邻条目构造。 */
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

/* TODO: path 是从 VM 字符串借用的原生指针；ResolveConstructTarget 和 PopulateEntryObject 都可分配。
 * 需在构造 File/Folder 时强制 GC 移动并验证路径；当前代码未在分配后重取 path。 */
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
    SZrObject *infoObject = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_RefreshEntryObject(context->state, self, ZR_NULL, &infoObject) ||
        infoObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, infoObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

/* 只有包装成功才把句柄所有权交给 FileStream；构造失败仍由此处关闭。 */
TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    TZrChar modeBuffer[ZR_LIBRARY_FILE_STREAM_MODE_MAX];
    SZrLibrary_File_StreamOpenResult openResult;
    SZrObject *streamObject;
    TZrBool handleTransferred = ZR_FALSE;
    EZrThreadStatus constructionStatus = ZR_THREAD_STATUS_FINE;
    if (self == ZR_NULL || result == ZR_NULL ||
        !system_fs_self_full_path(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadOptionalStringArgument(context, 0, "r", modeBuffer, sizeof(modeBuffer))) {
        return ZR_FALSE;
    }
    memset(&openResult, 0, sizeof(openResult));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)fullPath, modeBuffer, &openResult)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "open", fullPath);
    }
    /* TODO: fullPath 借自 self 的字符串；NewStreamObject 会先分配对象和字段再写 path。
     * 需以 GC 压力验证路径指针有效期，并核对包装失败时仍由本层关闭句柄。 */
    streamObject = ZrSystem_Fs_NewStreamObject(context->state, fullPath, &openResult,
                                              &handleTransferred, &constructionStatus);
    if (streamObject == ZR_NULL) {
        if (!handleTransferred) ZrLibrary_File_CloseHandle(openResult.handle);
        if (constructionStatus != ZR_THREAD_STATUS_FINE) {
            ZrCore_Exception_Throw(context->state, constructionStatus);
        }
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
    /* TODO: targetPath 借自 VM 实参，RefreshEntryObject 会分配快照对象。
     * 需在强制 GC 移动时验证随后创建的目标 File 路径仍有效。 */
    copyObject = ZrSystem_Fs_NewEntryObject(context->state, "File", targetPath, ZR_NULL);
    return copyObject != ZR_NULL && ZrSystem_Fs_FinishObjectResult(context->state, result, copyObject);
}

/* 调用者继续使用返回的新 File；self 仍描述原路径，以便 exists 观察移动后的消失状态。 */
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
    /* TODO: targetPath 借自 VM 实参，刷新源路径快照后可能已失效。
     * 需在强制 GC 移动时验证返回的目标 File 包装对象。 */
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

/* 三种直接子项视图共享一次平台枚举，File/Folder 筛选在物化脚本对象前完成。 */
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

/* TODO: 省略 recursively 当前按 true 处理；descriptor 只声明可选参数，不记录默认值。
 * 现有 test_system_fs_module 只显式传 true；需补脚本省略参数场景以核实公开契约。 */
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


/**
 * @file zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
 * ## Summary
 * 实现 zr.system.fs 的兼容模块函数、FileSystemEntry/File/Folder 回调，以及把一次性文件 I/O 与平台目录列表接入 VM 的内部 helper。公开 descriptor 在 fs_registry.c 中绑定这些 callback；FileStream 的状态和流式 I/O 由 fs_stream.c 负责。
 *
 * ## Constraints and lifecycle
 * callback 同步借用当前 ZrLibCallContext、receiver 与 VM 实参。descriptor arity 先检查必需参数；必需 string/array 错型由 Read API 抛 TypeError；可选 string 缺省时使用默认值，但显式错型仍抛 TypeError，可选 bool 错型由 helper 返回普通 false。context、结果槽及普通校验失败也可返回 false。VM 非局部 MemoryError 经 longjmp 越过 C 栈，不执行这些局部句柄、缓冲或列表的清理。一次性 helper 和目录列表只在普通返回路径清理；File.open 仅在 owner 字段完整发布后移交句柄，非 NULL wrapper 软失败例外见 File_Open。WriteStringField 的 HASH_BUCKET 软失败可让未初始化的 SZrTypeValue 进入 setter，影响构造、元数据快照和目标 wrapper，结果未定义；共享根因见 fs_common.c 的本批记录。多个变更 callback 忽略 RefreshEntryObject 的普通 false，但非局部异常仍会退出 callback。changeCurrentDirectory 修改宿主进程共享 cwd；并行相对路径 I/O 由宿主协调。
 *
 * ## References
 * callback 描述符见 zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c；共享字段、对象与异常 helper 见 fs_internal.h/fs_common.c；FileStream 交接见 fs_stream.c；native callback 入口见 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277。
 */

/**
 * @fn int system_fs_change_directory_native(const TZrChar *path)
 * ## Summary
 * 把平台差异收敛到一次 chdir/_chdir 调用，供兼容的 changeCurrentDirectory callback 使用。
 *
 * ## Parameters
 * @param[in] path 调用期间有效、以 NUL 结尾的宿主路径；不在 helper 内复制或保存。
 *
 * ## Returns
 * @return 直接返回平台 chdir/_chdir 的整数状态；零表示成功，非零表示失败，errno 留给同步调用方读取。
 *
 * ## Constraints and lifecycle
 * 修改的是进程级工作目录；不同 VM/state 间没有隔离。不得把 path 保留到 callback 返回之后。
 *
 * ## References
 * 仅由 ZrSystem_Fs_ChangeCurrentDirectory 调用；平台头文件分支位于本文件开头。
 */

/**
 * @fn TZrBool system_fs_self_full_path(SZrState *state, SZrObject *object, const TZrChar **outPath)
 * ## Summary
 * 从 File/Folder receiver 读取 fullPath 字段，避免各实例方法重复字段查找。
 *
 * ## Parameters
 * @param[in] state 当前 native callback 的 VM 状态，借用至字段读取完成。
 * @param[in] object 当前 FileSystemEntry 派生 receiver；本 helper 不持有它。
 * @param[out] outPath 必须非空；先置为 null，成功时返回 object 中的借用字符串指针。
 *
 * ## Returns
 * @return 只有 state、object、outPath 均有效且 fullPath 字段可读时返回 ZR_TRUE；其他情况返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * outPath 指向 VM 字符串，不转移所有权且不得跨可能触发 VM 分配的操作保存。调用方如在分配后还需该路径，应重新读取字段；相关回调窗口在各自 TODO 中记录。
 *
 * ## References
 * 由 File/Folder 的 open、create、read/write、copy/move、delete、列表和 glob callback 调用；字段读取委托 ZrSystem_Fs_GetStringField。
 */

/**
 * @fn TZrBool system_fs_read_text_file(SZrState *state, const TZrChar *path, SZrTypeValue *result)
 * ## Summary
 * 为模块级 readText 与 File.readText 提供一次性文本读取：本 helper 打开临时只读句柄、读取至 EOF，再关闭句柄。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态，读取文本可能在该状态中创建 VM 字符串。
 * @param[in] path 同步使用的 NUL 结尾宿主路径，不由 helper 保存。
 * @param[out] result 由调用方提供的 callback 结果槽；读取 helper 负责尝试写入文本值。
 *
 * ## Returns
 * @return 打开、读取和关闭均按本 helper 的成功规则完成时为 ZR_TRUE；打开/读取失败或成功读取后的关闭失败时为 ZR_FALSE，errno 保留主要 I/O 失败。
 *
 * ## Constraints and lifecycle
 * 临时句柄仅在普通返回路径关闭；关闭失败且读取成功会令整体失败。BUG: ReadTextFromHandle 的短串 HASH_BUCKET 软失败可能留下未写的 null 结果槽而仍报成功，精确条件沿用 fs_stream.c 的共享 ReadText BUG 记录。另在 ReadTextFromHandle:347 的 VM 字符串物化抛出非局部 MemoryError 时，会跳过 :351 的 free(text) 并越过本 helper :49 的 CloseHandle；dispatcher/longjmp 只恢复 VM root，不清理 C 局部资源。普通 false 路径仍执行显式清理。
 *
 * ## References
 * 直接调用者为 ZrSystem_Fs_ReadText 与 ZrSystem_Fs_File_ReadText；底层读文本由 ZrSystem_Fs_ReadTextFromHandle 完成。
 */

/**
 * @fn TZrBool system_fs_read_bytes_file(SZrState *state, const TZrChar *path, SZrTypeValue *result)
 * ## Summary
 * 为 File.readBytes 提供一次性二进制读取，并在普通路径关闭本次打开的句柄。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态，供字节数组物化使用。
 * @param[in] path 同步使用的 NUL 结尾宿主路径，不由 helper 保存。
 * @param[out] result 由 callback 提供的结果槽，读取 helper 将尝试写入 VM array。
 *
 * ## Returns
 * @return 打开、读取、VM 数组物化及关闭均普通成功时返回 ZR_TRUE；I/O 失败返回 ZR_FALSE 并保留主要 I/O errno。Array_New/PushValue 普通失败也返回 ZR_FALSE，此时不保证 errno 表示 I/O 错误；公开 callback 仍按当时 errno 抛 IOException。
 *
 * ## Constraints and lifecycle
 * 临时 handle 不转交给 VM 对象。BUG: ReadBytesFromHandle:290 的 Array_New 或 :299 的 Array_PushValue 若抛非局部 MemoryError，会越过 :292/:300/:305 的 free(bytes) 以及本 helper :70 的 CloseHandle；异常恢复不遍历 C 栈或注册 C 局部资源。Array_New 返回 null、PushValue 普通返回 false 等普通路径仍显式释放并关闭。
 *
 * ## References
 * 直接调用者为 ZrSystem_Fs_File_ReadBytes；底层缓冲读取与 VM array 构造由 ZrSystem_Fs_ReadBytesFromHandle 执行。
 */

/**
 * @fn TZrBool system_fs_write_text_file(SZrState *state, const TZrChar *path, const TZrChar *mode, const TZrChar *text, TZrInt64 *outWritten)
 * ## Summary
 * 封装模块和 File 的一次性文本覆盖/追加写入，统一处理临时句柄和 errno。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态，供文本写 helper 使用。
 * @param[in] path 同步使用的 NUL 结尾路径，不保存。
 * @param[in] mode 由调用者传入的宿主打开模式，当前为 w 或 a。
 * @param[in] text 本次写入的 NUL 结尾文本；调用期间借用。
 * @param[out] outWritten 必须指向有效计数槽，成功时接收底层写入的字节数。
 *
 * ## Returns
 * @return 全部文本写入且句柄关闭成功时返回 ZR_TRUE；打开/写入失败返回 ZR_FALSE 并恢复保存的 errno；写入成功但 close 失败也返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 句柄在普通返回路径关闭。WriteTextToHandle:411-438 只按 NUL 结尾文本计算 strlen 并调用 write_all，不做 VM 分配或抛错，因此此 helper 没有非局部 VM 清理缺口。嵌入 NUL 的文本只写首 NUL 前缀；任意字节内容应使用 writeBytes。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。一次性 helper 先以 w/a 打开文件再做写入；w 成功打开已可截断，a 可创建缺失文件，这些打开副作用也不因后续失败撤销。
 *
 * ## References
 * 由 ZrSystem_Fs_WriteText、AppendText、File_WriteText、File_AppendText 调用，实际写入委托 ZrSystem_Fs_WriteTextToHandle。
 */

/**
 * @fn TZrBool system_fs_write_bytes_file(SZrState *state, const TZrChar *path, const TZrChar *mode, SZrObject *array, TZrInt64 *outWritten)
 * ## Summary
 * 为 File.writeBytes/appendBytes 提供一次性字节写入，集中负责打开并关闭文件句柄。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态，供字节转换与写入 helper 使用。
 * @param[in] path 同步使用的 NUL 结尾路径，不保存。
 * @param[in] mode 由调用者选择的宿主模式，当前为 w 或 a。
 * @param[in] array VM byte array receiver/实参；在本次 callback 中借用，不转移所有权。
 * @param[out] outWritten 有效的输出计数槽，成功时接收写入字节数。
 *
 * ## Returns
 * @return 打开、写入和关闭依规则成功时为 ZR_TRUE；其他普通失败为 ZR_FALSE，并保留主要 I/O errno。
 *
 * ## Constraints and lifecycle
 * 临时句柄与 WriteBytesToHandle 的 C bytes 缓冲仅在普通返回路径清理。BUG: 仅对尚未物化的 RAW_CANONICAL 整数数组，native_binding_dispatch.c:1542 的 Array_Get 进入 Object_GetValue:2007；dirty materialization 经 object_super_array_internal.h:486 的 EnsurePairPool 到 hash_set.c:119 的 GcMalloc，memory.c:46 抛 MemoryError 时，会越过 fs_stream.c:403 的 free(bytes) 和本 helper:120 的 CloseHandle，native_binding_dispatch.c:1760 执行非局部退出。RAW_CANONICAL 可由 object_super_array.c:1870 的整数写入状态建立；编译器数组字面量 lowering compile_expression_values.c:539/553/580 使用 CREATE_ARRAY/SET_BY_INDEX，test_system_fs_module.c:753 只提供合法数组输入，不证明该 fixture 当前为 RAW_CANONICAL。execution_dispatch.c:5715 是可建立该状态的整数追加路径。普通 false 与非 RAW_CANONICAL 路径仍执行 free/close；本批无故障注入。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。数组元素在首次 WriteHandle 前全部校验；非法元素本身不会造成已写元素前缀，但此前 w/a 打开仍可截断或创建文件。
 *
 * ## References
 * 由 ZrSystem_Fs_File_WriteBytes 和 File_AppendBytes 调用，实际写入委托 ZrSystem_Fs_WriteBytesToHandle。
 */

/**
 * @fn TZrBool system_fs_make_entry_array(SZrState *state, const SZrLibrary_File_List *list, TZrBool filesOnly, TZrBool foldersOnly, SZrTypeValue *result)
 * ## Summary
 * 把平台枚举出的路径快照筛选并物化为 FileSystemEntry/File/Folder VM 对象数组，供 entries/files/folders/glob 共用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态；构造每个条目会分配 VM 对象。
 * @param[in] list 调用方持有的平台目录列表，仅在本次同步遍历中借用。
 * @param[in] filesOnly 非零时跳过非普通文件条目。
 * @param[in] foldersOnly 非零时跳过非目录条目；两个筛选均开启时只接受同时满足二者的条目。
 * @param[out] result 成功后写入 VM array；失败时不交付部分数组。
 *
 * ## Returns
 * @return 数组和所有保留条目均成功创建、push 后返回 ZR_TRUE；输入、构造或 push 失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * list 的存储归调用者，普通返回后必须释放；数组与当前 entryObject 暂存于 C 局部变量。BUG: Array_New、NewEntryObject/PopulateEntryObject 或 Array_PushValue 的 VM 分配若抛 MemoryError，会跳过 system_fs_folder_list_common:611 或 Folder_Glob:647 的 File_List_Free；exception_try_run.c:10-27 只恢复 VM root/GC scope，不遍历 abandoned C 栈，dispatcher 无 C-list cleanup 注册。NewEntryObject 还经共享 WriteStringField；HASH_BUCKET 软失败可能令未初始化 SZrTypeValue 进入 setter，属于 UB 而非稳定的缺字段对象，见本批共享字段 BUG 记录。TODO: 沿实际 native lane 核查局部数组与未入数组条目的参数保活、GC safepoint 后字符串存储稳定性及连续构造窗口；目前未证普通对象悬空或重定位。Array_PushValue 的单次 pin 不证明相邻构造期间持续存活。
 *
 * ## References
 * 仅由 system_fs_folder_list_common 与 ZrSystem_Fs_Folder_Glob 调用；新对象经 ZrSystem_Fs_NewEntryObject 创建，最后经 ZrLib_Array_PushValue 入数组。
 */

/**
 * @fn TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 为 zr.system.fs 的兼容函数 currentDirectory 规范化宿主当前工作目录并返回 VM string；该入口并不要求 File/Folder receiver。
 *
 * ## Parameters
 * @param[in] context 当前 native callback 上下文，无显式 VM 参数；借用至 callback 返回。
 * @param[out] result dispatcher 提供的结果槽，成功时应包含归一化路径字符串。
 *
 * ## Returns
 * @return 路径规范化成功时当前实现返回 ZR_TRUE；上下文/结果为空或 NormalizePath 失败时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 结果是 VM 管理的字符串。BUG: 对短且尚未驻留的规范化 cwd，若短串 HASH_BUCKET 扩容这一次软失败，void ZrLib_Value_SetString 不写结果槽，而 callback 仍返回 ZR_TRUE，调用脚本观察到预置 null；条件及 string-cache 链见 fs_stream batch 的共享 HASH_BUCKET 证据。此 API 读取的是进程工作目录，不是 VM 私有目录。
 *
 * ## References
 * 由 fs_registry.c:301 的 currentDirectory descriptor 绑定，并经 native_binding_invoke_callback_inline 派发；路径来自 ZrLibrary_File_NormalizePath(".")。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现兼容函数 changeCurrentDirectory：将必需 path 交给平台 chdir/_chdir，并把系统调用成败作为 VM bool 返回。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string 参数 path 的当前调用上下文；字符串只在同步 callback 内借用。
 * @param[out] result 接收系统操作成功/失败的 bool；不是 callback 自身的状态码。
 *
 * ## Returns
 * @return path 读取并写入 bool 时返回 ZR_TRUE；chdir 失败写入 false 仍返回 ZR_TRUE。context/result 普通无效返回 ZR_FALSE；缺必需 path 由 arity 检查抛 ArityError，错型由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * chdir/_chdir 改变全进程共享 cwd，影响所有 VM/state 后续相对路径解析；API 不提供同步或隔离，跨线程调用与相对路径 I/O 应由宿主协调。
 *
 * ## References
 * fs_registry.c:303 注册 callback；本函数调用 system_fs_change_directory_native，并依赖平台目录函数。
 */

/**
 * @fn TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 pathExists 查询，将宿主存在类型映射成 VM bool。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文；该路径只在同步查询期间借用。
 * @param[out] result 接收是否为已知文件、目录或其他存在对象的 bool。
 *
 * ## Returns
 * @return 查询完成时返回 ZR_TRUE；不存在写入普通 false，不抛 IOException。context/result 或其他普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 路径按宿主文件 API 的 NUL 结尾字符串解释；结果不保留 path。
 *
 * ## References
 * 由 fs_registry.c:305 的 pathExists descriptor 暴露；存在枚举由 ZrLibrary_File_Exist 提供。
 */

/**
 * @fn TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 isFile，判断路径当前是否对应普通文件。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文，路径按 callback 同步借用。
 * @param[out] result 接收比较结果；本函数不会把路径保存到 VM 对象。
 *
 * ## Returns
 * @return 类型查询完成时返回 ZR_TRUE，result 表示是否为 FILE。context/result 或普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 类型查询结果直接来自宿主 ZrLibrary_File_Exist；false 同时覆盖“不属于普通文件”的正常结果。
 *
 * ## References
 * fs_registry.c:307 将该函数绑定为模块级 isFile；共用 ZrLibrary_File_Exist。
 */

/**
 * @fn TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 isDirectory，判断宿主路径是否为目录。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文，路径按 callback 同步借用。
 * @param[out] result 接收是否等于 DIRECTORY 的 VM bool。
 *
 * ## Returns
 * @return 类型查询完成时返回 ZR_TRUE；context/result 或普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 这是类型谓词；普通 false 不表示 native callback 失败，也不抛 IOException。
 *
 * ## References
 * fs_registry.c:309 将 callback 绑定为 isDirectory；具体类型由 ZrLibrary_File_Exist 返回。
 */

/**
 * @fn TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 createDirectory，仅创建指定目录本身，不负责递归建立父路径。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文；path 仅在本次宿主调用中借用。
 * @param[out] result 操作成功时写入 true。
 *
 * ## Returns
 * @return 创建成功返回 ZR_TRUE；宿主创建失败抛 IOException。context/result 等普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 不将路径保存到 VM 对象；目录权限和存在性语义由宿主库决定。
 *
 * ## References
 * fs_registry.c:311 注册模块级 callback；调用 ZrLibrary_File_CreateDirectorySingle。
 */

/**
 * @fn TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 createDirectories，调用平台层递归创建路径链。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的同步调用上下文。
 * @param[out] result 成功时接收 true。
 *
 * ## Returns
 * @return 创建成功返回 ZR_TRUE；宿主操作失败抛 IOException。context/result 等普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 不缓存路径或状态；目录创建副作用在 callback 返回前发生。 递归创建不是事务：逐级 mkdir 后若后续层级或最终步骤失败，已创建的父目录不撤销；IOException 不保证路径树保持调用前状态。
 *
 * ## References
 * fs_registry.c:313 注册该兼容函数；宿主递归操作由 ZrLibrary_File_CreateDirectories 执行。
 */

/**
 * @fn TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现模块级 removePath，按宿主存在类型删除文件、other 项或空目录。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文，path 在同步删除期间借用。
 * @param[out] result 删除完成时写入 true。
 *
 * ## Returns
 * @return 删除成功返回 ZR_TRUE；不存在、未接受的宿主类型或删除失败抛 IOException。普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 对目录明确传递 recursively=false；删除是外部副作用，不可由 VM 槽回滚。
 *
 * ## References
 * fs_registry.c:315 暴露 removePath；存在分类和实际删除由 ZrLibrary_File_Exist/ZrLibrary_File_Delete 完成。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现兼容模块函数 readText(path)，通过一次性 helper 读取文本至 EOF。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文，path 只在本次同步 I/O 中借用。
 * @param[out] result 接收 VM string；不由 callback 自己分配 C 结果缓冲。
 *
 * ## Returns
 * @return 完整读取成功返回 ZR_TRUE；helper 的普通 I/O 失败抛 IOException。context/result 等普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。非局部 MemoryError 的 C 清理见 helper BUG。
 *
 * ## Constraints and lifecycle
 * 临时文件句柄在 helper 的普通返回路径关闭。BUG: 沿用 ReadTextFromHandle 的短串 HASH_BUCKET 单次扩容软失败路径时，helper 可报告成功但不写结果槽，故 callback 可返回 true/null；具体条件与调用链见 fs_stream 批次。嵌入 NUL 的文本语义由共享读取 helper 决定。
 *
 * ## References
 * fs_registry.c:317 注册 callback；调用 system_fs_read_text_file，再到 ZrSystem_Fs_ReadTextFromHandle。
 */

/**
 * @fn TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现兼容函数 writeText(path, text)，以覆盖模式一次写入文本。
 *
 * ## Parameters
 * @param[in] context 含 path 与 text 两个必需 string 参数；仅在本次同步 callback/helper 中借用。
 * @param[out] result 文件写入及关闭成功时接收 true。
 *
 * ## Returns
 * @return 写入成功返回 ZR_TRUE；I/O 失败抛 IOException。普通 context/result 校验失败返回 ZR_FALSE；缺必需 path/text 抛 ArityError，非 string 参数由 ReadString 抛 TypeError。文本按 C string 写入，内嵌 NUL 后缀被截断；任意字节用 writeBytes。
 *
 * ## Constraints and lifecycle
 * 写入字节数写入内部 ignored 槽，公开 API 只返回 bool；覆盖写入的外部副作用不回滚。临时句柄普通路径由 helper 关闭。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。一次性 helper 先以 w/a 打开文件再做写入；w 成功打开已可截断，a 可创建缺失文件，这些打开副作用也不因后续失败撤销。
 *
 * ## References
 * fs_registry.c:319 注册此兼容函数；内部调用 system_fs_write_text_file，使用 mode w。
 */

/**
 * @fn TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 实现兼容函数 appendText(path, text)，将文本追加到文件末尾。
 *
 * ## Parameters
 * @param[in] context 含两个必需 string 参数 path/text，按本次同步写入借用。
 * @param[out] result 成功写入并关闭后接收 true。
 *
 * ## Returns
 * @return 写入成功返回 ZR_TRUE；I/O 失败抛 IOException。普通 context/result 校验失败返回 ZR_FALSE；缺必需 path/text 抛 ArityError，非 string 参数由 ReadString 抛 TypeError。文本按 NUL 结尾 C string 处理；任意字节用 writeBytes。
 *
 * ## Constraints and lifecycle
 * 公共结果不暴露内部字节计数；mode 为 a，临时句柄由 system_fs_write_text_file 关闭。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。一次性 helper 先以 w/a 打开文件再做写入；w 成功打开已可截断，a 可创建缺失文件，这些打开副作用也不因后续失败撤销。
 *
 * ## References
 * fs_registry.c:321 绑定 callback；实际写入走 system_fs_write_text_file 与 ZrSystem_Fs_WriteTextToHandle。
 */

/**
 * @fn TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 查询任意 path 并创建独立 SystemFileInfo 元数据快照，不要求已有 File/Folder receiver。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的调用上下文；宿主查询完成后不保存 path。
 * @param[out] result 成功时接收由 VM 管理的 SystemFileInfo 对象。
 *
 * ## Returns
 * @return 查询与对象创建成功返回 ZR_TRUE；普通查询/校验失败返回 ZR_FALSE。缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。BUG: MakeInfoObject 经共享 WriteStringField；短串 HASH_BUCKET 软失败可使未初始化 SZrTypeValue 进入 setter，结果未定义，见本批共享字段 BUG 记录。
 *
 * ## Constraints and lifecycle
 * 快照只反映查询时点，不会与文件系统持续同步；result 中的 object 由 VM 生命周期管理。
 *
 * ## References
 * fs_registry.c:323 将函数注册为 getInfo；元数据对象由 ZrSystem_Fs_MakeInfoObject 创建。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Entry_Constructor(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 作为 FileSystemEntry/File/Folder 共用构造 callback，把一个路径初始化到 VM 选定的 receiver。
 *
 * ## Parameters
 * @param[in] context 含一个必需 string path 的构造上下文；callback 按 constructor 协议取得目标 receiver。
 * @param[out] result 成功时由 FinishObjectResult 交付当前构造对象。
 *
 * ## Returns
 * @return 目标解析、字段填充和 FinishObjectResult 成功返回 ZR_TRUE；普通 helper 失败返回 ZR_FALSE，VM 异常可非局部退出。缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * 构造不保证新建对象：ResolveConstructTarget 按 constructor 协议选定或复用 target。BUG: PopulateEntryObject 经 WriteStringField 写短串字段；HASH_BUCKET 单次扩容软失败会使未初始化 SZrTypeValue 进入 setter，行为未定义，不能断言稳定地产生缺字段对象；共享触发与 allocator 路径见本批字段发布 BUG 记录。TODO: path 是 VM string 的借用指针，target 解析及 PopulateEntryObject 会分配；native lane 的 pin/GC 地址窗口仍待核。
 *
 * ## References
 * fs_registry.c:78 将同一 callback 绑定到 FileSystemEntry/File/Folder 构造表；调用 ZrSystem_Fs_ResolveConstructTarget、PopulateEntryObject 和 FinishObjectResult。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Entry_Exists(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 刷新 receiver 的 FileSystemEntry 快照并返回当前路径存在性。
 *
 * ## Parameters
 * @param[in] context 普通实例方法的当前上下文；接收 FileSystemEntry 派生 self，无显式 VM 参数。
 * @param[out] result 查询成功后接收 info.exists。
 *
 * ## Returns
 * @return 刷新成功时返回 ZR_TRUE，result 是查询时点的存在性；路径不存在可作为正常 false 值。self/context/result 或 RefreshEntryObject 普通校验失败返回 ZR_FALSE。BUG: 刷新写入 SystemFileInfo 字符字段时共享 WriteStringField 的 HASH_BUCKET 软失败可能把未初始化 SZrTypeValue 交给 setter，结果未定义；见本批字段发布 BUG 记录。
 *
 * ## Constraints and lifecycle
 * 刷新会更新 receiver 的 fileInfo 快照；返回 bool 描述查询时点，不提供后续一致性保证。
 *
 * ## References
 * fs_registry.c:128 注册实例方法 exists；通过 ZrSystem_Fs_SelfObject 和 RefreshEntryObject 读取/更新 receiver。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Entry_Refresh(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 刷新 FileSystemEntry receiver 的元数据字段，并把新建的 SystemFileInfo 快照作为结果返回。
 *
 * ## Parameters
 * @param[in] context 含 FileSystemEntry 派生 self 的方法上下文，无显式 VM 参数。
 * @param[out] result 成功时接收 VM 管理的 metadata object。
 *
 * ## Returns
 * @return receiver 有效且刷新产生 infoObject 时返回 ZR_TRUE；普通校验/刷新失败返回 ZR_FALSE。BUG: RefreshEntryObject 的字符串字段写入共享 WriteStringField；HASH_BUCKET 软失败可使未初始化 SZrTypeValue 进入 setter，结果未定义，见本批字段发布 BUG 记录。
 *
 * ## Constraints and lifecycle
 * 返回对象和 self.fileInfo 是同一次刷新产生的 VM 快照，不是对宿主文件的长期引用。
 *
 * ## References
 * fs_registry.c:130 注册 refresh；由 ZrSystem_Fs_RefreshEntryObject 填充字段并创建 info。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 打开 File receiver 的 fullPath，并把成功打开的句柄包装成 FileStream 返回。
 *
 * ## Parameters
 * @param[in] context 含 File receiver 和可选 string mode 的方法上下文；省略时为 r，只同步借用首 NUL 前缀。8 字节 modeBuffer 包含终止 NUL，因此前缀最多 7 字节；更长时在 OpenHandle 前普通返回 ZR_FALSE。
 * @param[out] result 成功时接收 FileStream VM 对象。
 *
 * ## Returns
 * @return 打开及 wrapper 交付成功返回 ZR_TRUE；OpenHandle 的模式解析、宿主打开或 append 定位失败均转入 RaiseErrnoIOException（短而非法 mode 如空串/q 设 EINVAL）。receiver/result/fullPath 前置校验、mode 首 NUL 前缀超过 7 字节或 wrapper 普通构造失败返回 ZR_FALSE。省略 mode 用 r；显式非 string mode 由 ReadString 抛 TypeError。包装异常通过私有 outStatus 返回；本 callback 先关闭未转移句柄，再按原状态传播。
 *
 * ## Constraints and lifecycle
 * hidden owner 字段发布成功即由 FileStream 接管，即使后续构造失败也不重复关闭；提交前失败尝试 CloseHandle，忽略关闭返回值且不改捕获状态。关闭失败后的可重试性不在本组保证内。TODO: fullPath 来自 self 的 VM 字符串，在 NewStreamObject 分配和写字段期间仍被使用，GC/lane 地址有效期待核。 w/a/x 模式的宿主 open 成功后已可能截断或创建文件；之后 append seek 失败、wrapper 普通 NULL 或非局部 VM 异常均不恢复文件内容。未转移 owner 的失败只触发 CloseHandle 尝试，关闭句柄不等于回滚路径副作用。
 *
 * ## References
 * fs_registry.c:135 绑定 File.open；调用 system_fs_self_full_path、ZrLibrary_File_OpenHandle、ZrSystem_Fs_NewStreamObject 和 FinishObjectResult。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_Create(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * File 路径不存在时创建空文件；若已有普通文件则成功返回而不截断，再刷新 receiver 元数据。
 *
 * ## Parameters
 * @param[in] context 含 File self 与可选 bool recursively 的上下文；省略时为 true，仅用于按需创建父目录；错型由 optional-bool helper 普通返回 false。
 * @param[out] result 成功时写入 VM null。
 *
 * ## Returns
 * @return 创建操作成功且结果槽有效时返回 ZR_TRUE；宿主失败抛 IOException，普通 context/self/optional-bool 校验失败返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 先执行 CreateEmpty，再忽略 RefreshEntryObject 的普通 false；非局部 VM 异常仍会退出。CreateEmpty 对已存在普通文件不截断，只在不存在时创建空文件；recursively 只控制父目录。BUG: refresh 的字符串字段若遇共享 WriteStringField HASH_BUCKET 软失败，未初始化 SZrTypeValue 会进入 setter，结果未定义。TODO: 普通 refresh false 是否应影响公开成功/快照契约，见 Entry_Refresh 与本批共享字段 BUG 记录。 递归创建不是事务：逐级 mkdir 后若后续层级或最终步骤失败，已创建的父目录不撤销；IOException 不保证路径树保持调用前状态。 目标文件创建成功后若 CloseHandle 失败仍返回宿主 false，已创建文件同样保留；原有普通文件内容保持不变。
 *
 * ## References
 * fs_registry.c:138 注册 File.create；宿主创建由 ZrLibrary_File_CreateEmpty 执行。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 通过 File receiver 的路径一次性读取文本字节并构造 VM string，不校验 UTF-8；随后尝试刷新 receiver 元数据。
 *
 * ## Parameters
 * @param[in] context 含 File self 的无显式参数方法上下文。
 * @param[out] result 接收文本 VM value。
 *
 * ## Returns
 * @return 普通读取与 close 成功返回 ZR_TRUE；I/O 失败抛 IOException，receiver/context 普通无效返回 ZR_FALSE。BUG: ReadTextFromHandle 的非局部 VM 字符串物化异常会跳过 C 临时缓冲及本 helper 外层句柄清理；短串缓存软失败仍可 true/null，见对应共享 BUG 记录。
 *
 * ## Constraints and lifecycle
 * BUG: 与模块 readText 共用短串 HASH_BUCKET 软失败链，若 cache miss 的单次扩容失败且普通 close 与后续 refresh 返回，callback 可 true/null；详见 fs_stream 批次。非局部字符串物化异常会跳过 helper C 缓冲/句柄清理。普通 refresh false 被忽略且不会撤销读取。TODO: refresh false 的公开快照策略待核，见 Entry_Refresh。ReadTextFromHandle 按 C string 截断首 NUL。
 *
 * ## References
 * fs_registry.c:141 注册 File.readText；调用 system_fs_read_text_file 并在普通成功后调用 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_WriteText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 以覆盖模式写入 File receiver 路径并返回实际写入字节数。
 *
 * ## Parameters
 * @param[in] context 含 File self 与一个必需 string text 参数；两者在同步 callback 中借用。
 * @param[out] result 成功时接收 int64 字节数。
 *
 * ## Returns
 * @return 写入成功返回 ZR_TRUE；I/O 失败抛 IOException，普通 receiver/result 校验失败返回 ZR_FALSE；缺必需 text 抛 ArityError，非 string text 由 ReadString 抛 TypeError。WriteText 使用 NUL 结尾 C string，内嵌 NUL 后缀不写；任意字节使用 writeBytes。
 *
 * ## Constraints and lifecycle
 * 使用 mode w 覆盖写入；WriteText 按 strlen 处理，内嵌 NUL 后缀不写。成功后普通 RefreshEntryObject false 被忽略，写入不回滚，非局部异常仍退出；字符串字段软失败的共享 UB 见本批字段发布 BUG。TODO: 普通 refresh false 是否应影响公开快照策略，见 Entry_Refresh。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。一次性 helper 先以 w/a 打开文件再做写入；w 成功打开已可截断，a 可创建缺失文件，这些打开副作用也不因后续失败撤销。
 *
 * ## References
 * fs_registry.c:143 注册方法；写入经 system_fs_write_text_file(mode w) 和 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_AppendText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 以追加模式把文本写入 File receiver 路径，并返回写入字节数。
 *
 * ## Parameters
 * @param[in] context 含 File self 及一个必需 string text 参数，按同步写入借用。
 * @param[out] result 写入成功时接收 int64 字节数。
 *
 * ## Returns
 * @return 追加成功返回 ZR_TRUE；I/O 错误抛 IOException，普通 receiver/result 校验失败返回 ZR_FALSE；缺必需 text 抛 ArityError，非 string text 由 ReadString 抛 TypeError。文本按 NUL 结尾 C string 处理，内嵌 NUL 后缀不写；任意字节使用 appendBytes。
 *
 * ## Constraints and lifecycle
 * 使用 mode a；WriteText 按 strlen 处理，内嵌 NUL 后缀不写。成功后普通 RefreshEntryObject false 被忽略，追加的字节不回滚；非局部异常仍退出。共享字段 setter 的 HASH_BUCKET UB 见本批字段发布 BUG。TODO: 普通 refresh false 是否应影响公开快照策略，见 Entry_Refresh。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。一次性 helper 先以 w/a 打开文件再做写入；w 成功打开已可截断，a 可创建缺失文件，这些打开副作用也不因后续失败撤销。
 *
 * ## References
 * fs_registry.c:146 绑定 File.appendText；使用 system_fs_write_text_file(mode a) 和 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 从 File receiver 路径一次性读取二进制内容并交付 VM array。
 *
 * ## Parameters
 * @param[in] context 含 File self 的无显式参数调用上下文。
 * @param[out] result 成功时接收 GC 管理的 byte array。
 *
 * ## Returns
 * @return 读取/关闭成功返回 ZR_TRUE；receiver、result 或 fullPath 前置校验普通失败返回 ZR_FALSE。一次性 helper 的普通 false（包括宿主 I/O/close 失败及 Array_New/PushValue 普通失败）转入 RaiseErrnoIOException，按当时 errno 构造/抛出 IOException；非局部 MemoryError 见下方资源约束。
 *
 * ## Constraints and lifecycle
 * 数组归 VM 管理；ReadBytesFromHandle 的 C 缓冲和本 helper 临时句柄仅普通返回时清理。BUG: Array_New/PushValue 的非局部 MemoryError 会越过两处清理，见 system_fs_read_bytes_file BUG。普通 refresh false 被忽略且不回滚读取。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh；RefreshEntryObject 字段 setter 另受共享 HASH_BUCKET UB 影响。
 *
 * ## References
 * fs_registry.c:149 注册 File.readBytes；调用 system_fs_read_bytes_file 与 ZrSystem_Fs_RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 以覆盖模式把 File receiver 的路径写成 VM byte array 内容，返回写入字节数。
 *
 * ## Parameters
 * @param[in] context 含 File self 和一个必需 array 参数；array 在同步操作中借用。
 * @param[out] result 成功时接收 int64 字节数。
 *
 * ## Returns
 * @return 写入/关闭成功返回 ZR_TRUE；receiver、result 或 fullPath 前置校验普通失败返回 ZR_FALSE。一次性 helper 普通 false 转入 RaiseErrnoIOException，包括非整数或不在 0..255 的数组元素使 helper 设置 EINVAL 的内容校验失败；缺必需 array 抛 ArityError，非 array 由 ReadArray 抛 TypeError。RAW_CANONICAL 非局部物化异常的资源 BUG 另见 helper。
 *
 * ## Constraints and lifecycle
 * 使用 mode w 覆盖写入；普通 RefreshEntryObject false 被忽略，已写内容不回滚，非局部异常仍退出。RAW_CANONICAL Array_Get 物化资源 BUG 见共享 helper；刷新字段的 WriteStringField UB 见本批字段发布 BUG。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。数组元素在首次 WriteHandle 前全部校验；非法元素本身不会造成已写元素前缀，但此前 w/a 打开仍可截断或创建文件。
 *
 * ## References
 * fs_registry.c:151 注册方法；写入经 system_fs_write_bytes_file(mode w) 和 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_AppendBytes(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 以追加模式把 VM byte array 内容写到 File receiver 路径末尾。
 *
 * ## Parameters
 * @param[in] context 含 File self 和一个必需 array 参数，该 array 不被本 callback 持有。
 * @param[out] result 成功时接收 int64 字节数。
 *
 * ## Returns
 * @return 追加/关闭成功返回 ZR_TRUE；receiver、result 或 fullPath 前置校验普通失败返回 ZR_FALSE。一次性 helper 普通 false 转入 RaiseErrnoIOException，包括非整数或不在 0..255 的数组元素使 helper 设置 EINVAL 的内容校验失败；缺必需 array 抛 ArityError，非 array 由 ReadArray 抛 TypeError。RAW_CANONICAL 非局部物化异常的资源 BUG 另见 helper。
 *
 * ## Constraints and lifecycle
 * 使用 mode a 追加；普通 RefreshEntryObject false 被忽略，写入不回滚，非局部异常仍退出。RAW_CANONICAL Array_Get 物化资源 BUG 见共享 helper；刷新字段的 WriteStringField UB 见本批字段发布 BUG。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh。 宿主短写循环后续报错或零进度 EIO 时，失败前可能已写出前缀；IOException 或普通 false 不回滚这些字节。数组元素在首次 WriteHandle 前全部校验；非法元素本身不会造成已写元素前缀，但此前 w/a 打开仍可截断或创建文件。
 *
 * ## References
 * fs_registry.c:154 绑定方法；调用 system_fs_write_bytes_file(mode a) 与 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 File 路径复制到必需目标路径，刷新源 receiver，并返回目标 File 包装对象。
 *
 * ## Parameters
 * @param[in] context 含 File self、必需 string targetPath 和可选 bool overwrite（缺省 false）；目标字符串来自 VM 实参。
 * @param[out] result 成功包装后接收新 File 对象。
 *
 * ## Returns
 * @return 复制与目标 wrapper 交付成功返回 ZR_TRUE；宿主复制失败抛 IOException，普通 receiver/result/optional-bool 校验或包装失败返回 ZR_FALSE。缺必需 targetPath 抛 ArityError，非 string 抛 TypeError；optional bool 错型普通 false。
 *
 * ## Constraints and lifecycle
 * 复制完成后调用 RefreshEntryObject，但忽略其普通 false，随后仍构造目标 wrapper；副本不会回滚，源 receiver 的 fileInfo 可能仍旧。非局部异常会退出 callback。目标 NewEntryObject 也经共享 WriteStringField，HASH_BUCKET 软失败可导致未定义结果，见本批字段发布 BUG。TODO: targetPath 跨 Refresh/constructor VM 分配的 pin/GC 地址有效期仍待核查。 宿主复制本身失败也不承诺原子性或回滚：overwrite 可先删除旧目标，随后读写或递归子项失败会保留已建目标、已写前缀或部分目录树。 覆盖删除目标发生在源打开之前，父目录建立发生在目标打开之前；后续失败可使旧目标已消失、目标仍不存在或只留下父目录，均不回滚。实际复制普通文件内容的分支忽略源与目标 CloseHandle 的返回值（file.c:825–832），因而成功不保证 close 成功。 BUG: overwrite=true 且现存目标是源路径的严格祖先目录时，递归 Delete(target) 会把源与该祖先树中的兄弟项一并删除；限定删除成功且无外部重建，随后 Copy 打开/列举源或 Move 的 rename/Copy 失败，callback 抛 IOException。目录 Copy 还可能先重建空目标再因源缺失失败（file.c:772–783/:847–853/:1259–1269）。 File/Folder wrapper 名称不校验实际源类型；Copy 在 file.c:1228–1239 按 stat 结果分派。因此实际源为目录的 File wrapper 也受“目标为源子目录时先建后枚举、不断纳入新目标”的递归 BUG 影响；Move 仅在 rename 失败后的 Copy 回退适用此递归路径。路径关系按本次 NormalizePath 输出的组件边界判断，不提供别名解析或并发文件系统原子保证。
 *
 * ## References
 * fs_registry.c:157 注册 File.copyTo；实际复制由 ZrLibrary_File_Copy 执行，目标对象由 ZrSystem_Fs_NewEntryObject 构造。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 File 从 receiver 路径移动到 targetPath；尝试刷新原 receiver，再返回目标 File wrapper。
 *
 * ## Parameters
 * @param[in] context 含 File self、必需 string targetPath 和可选 bool overwrite（缺省 false）。
 * @param[out] result 成功时接收指向目标路径的新 File 对象；原 self 仍绑定旧路径。
 *
 * ## Returns
 * @return move 与目标 wrapper 交付成功返回 ZR_TRUE；宿主失败抛 IOException，普通 receiver/result/optional-bool 校验或包装失败返回 ZR_FALSE。缺必需 targetPath 抛 ArityError，非 string 抛 TypeError；optional bool 错型普通 false。
 *
 * ## Constraints and lifecycle
 * move 完成后调用 RefreshEntryObject 并忽略其普通 false，再构造目标 wrapper；原 receiver 仍绑定旧路径，fileInfo 可能保留旧快照，宿主移动不会回滚。非局部异常会退出 callback。目标 NewEntryObject 的共享 WriteStringField HASH_BUCKET 失败可导致未定义结果，见本批字段发布 BUG。TODO: targetPath 跨 refresh 与构造分配的 pin/GC 地址窗口仍待核。 移动并非原子事务：overwrite 可先删除旧目标；rename 失败后按 Copy(overwrite=true) 再 Delete(source) 回退，回退 copy 失败不执行末尾源删除（不撤销此前 overwrite 删除）却可留下部分目标，copy 成功但源删除失败也不恢复目标，可能两边都有内容。 覆盖删除目标发生在源打开之前，父目录建立发生在目标打开之前；后续失败可使旧目标已消失、目标仍不存在或只留下父目录，均不回滚。仅回退到实际复制普通文件内容的分支忽略源与目标 CloseHandle 的返回值（file.c:825–832），因而成功不保证 close 成功。 rename 成功分支不经过该复制 helper。BUG: overwrite=true 且 NormalizePath 后 source/target 为同一现存路径时，Move:1259–1261 先递归删除目标即源；删除成功且无外部重建后，rename:1264 与 Copy:1228–1231 因源不存在失败，callback 抛 IOException，源已丢失。此结论包含相同路径输入，不推断所有文件系统别名等价。 BUG: overwrite=true 且现存目标是源路径的严格祖先目录时，递归 Delete(target) 会把源与该祖先树中的兄弟项一并删除；限定删除成功且无外部重建，随后 Copy 打开/列举源或 Move 的 rename/Copy 失败，callback 抛 IOException。目录 Copy 还可能先重建空目标再因源缺失失败（file.c:772–783/:847–853/:1259–1269）。 File/Folder wrapper 名称不校验实际源类型；Copy 在 file.c:1228–1239 按 stat 结果分派。因此实际源为目录的 File wrapper 也受“目标为源子目录时先建后枚举、不断纳入新目标”的递归 BUG 影响；Move 仅在 rename 失败后的 Copy 回退适用此递归路径。路径关系按本次 NormalizePath 输出的组件边界判断，不提供别名解析或并发文件系统原子保证。 Move 在删除 overwrite 目标前也不预验源存在；源本已缺失时仍可能先删现存目标，再返回失败。
 *
 * ## References
 * fs_registry.c:160 绑定 File.moveTo；调用 ZrLibrary_File_Move、RefreshEntryObject 与 ZrSystem_Fs_NewEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_File_Delete(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 删除 File receiver 指向的单个文件并尝试刷新它原有的元数据快照。
 *
 * ## Parameters
 * @param[in] context 含 File self 的无显式参数方法上下文。
 * @param[out] result 删除成功时写 VM null。
 *
 * ## Returns
 * @return 删除成功返回 ZR_TRUE；宿主失败抛 IOException，receiver/context 普通无效返回 ZR_FALSE。成功后普通 refresh false 被忽略，非局部异常仍退出。
 *
 * ## Constraints and lifecycle
 * 删除单个文件（recursively=false）；随后 RefreshEntryObject 的普通 false 被忽略，删除不回滚，非局部异常仍退出。refresh 写入的字符串字段受共享 WriteStringField HASH_BUCKET UB 影响，见本批字段发布 BUG。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh。
 *
 * ## References
 * fs_registry.c:163 注册 File.delete；实际调用 ZrLibrary_File_Delete 和 RefreshEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Create(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 按 recursively 选择单级或递归创建 Folder receiver 对应目录，并刷新其快照。
 *
 * ## Parameters
 * @param[in] context 含 Folder self 和可选 bool recursively；缺省值当前为 true。
 * @param[out] result 创建成功后写 VM null。
 *
 * ## Returns
 * @return 创建成功返回 ZR_TRUE；宿主失败抛 IOException，普通 receiver/result/optional-bool 错型校验返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 目录创建后调用 RefreshEntryObject，但忽略普通 false；目录仍报告成功且快照可能旧，非局部异常仍退出。refresh 的字符串字段受共享 WriteStringField HASH_BUCKET UB 影响，见本批字段发布 BUG。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh。 递归创建不是事务：逐级 mkdir 后若后续层级或最终步骤失败，已创建的父目录不撤销；IOException 不保证路径树保持调用前状态。 recursively=false 时只创建本级，不执行该逐级循环。
 *
 * ## References
 * fs_registry.c:168 注册 Folder.create；选择 ZrLibrary_File_CreateDirectories 或 CreateDirectorySingle。
 */

/**
 * @fn TZrBool system_fs_folder_list_common(ZrLibCallContext *context, SZrTypeValue *result, TZrBool filesOnly, TZrBool foldersOnly)
 * ## Summary
 * 统一执行 Folder 的直接子项枚举，按类型筛选后调用 system_fs_make_entry_array。
 *
 * ## Parameters
 * @param[in] context 含 Folder receiver 的方法上下文。
 * @param[out] result 成功时接收 VM array。
 * @param[in] filesOnly 传给数组物化层的普通文件筛选标志。
 * @param[in] foldersOnly 传给数组物化层的目录筛选标志。
 *
 * ## Returns
 * @return ListDirectory 与数组物化成功返回 ZR_TRUE；普通输入/物化失败返回 ZR_FALSE，宿主枚举失败抛 IOException。非局部分配异常会跳过 C 列表清理；wrapper 字段还受共享发布 BUG 影响。
 *
 * ## Constraints and lifecycle
 * list 在栈上初始化，ListDirectory 普通失败会清除部分结果；普通物化返回后调用 File_List_Free。BUG: NewEntryObject/Array_PushValue 的 VM 分配若抛 MemoryError，longjmp 跳过本函数 :611 的 File_List_Free；exception_try_run.c:10-27 不遍历 abandoned C 栈，dispatcher 仅恢复 VM root，不注册 C-list cleanup。NewEntryObject 的字符串发布还受共享 WriteStringField HASH_BUCKET UB 影响。
 *
 * ## References
 * 被 Folder_Entries、Folder_Files、Folder_Folders 三个 wrapper 调用；调用 system_fs_self_full_path、ZrLibrary_File_ListDirectory、system_fs_make_entry_array。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Entries(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 返回 Folder 直接子项的 FileSystemEntry/File/Folder 包装数组，不递归遍历子树。
 *
 * ## Parameters
 * @param[in] context 含 Folder self 的无显式参数方法上下文。
 * @param[out] result 成功时接收 VM 管理的 array。
 *
 * ## Returns
 * @return 列举与数组物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE，宿主枚举失败抛 IOException。非局部分配异常会跳过列表清理；每项构造还受共享字段发布 BUG 影响，见 system_fs_folder_list_common。
 *
 * ## Constraints and lifecycle
 * 三个列表 wrapper 通过 common helper 创建数组；本函数筛选标志均为 false。
 *
 * ## References
 * fs_registry.c:171 绑定 entries；转调 system_fs_folder_list_common(context, result, false, false)。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Files(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 返回 Folder 的直接普通文件子项包装数组。
 *
 * ## Parameters
 * @param[in] context 含 Folder self 的无显式参数方法上下文。
 * @param[out] result 成功时接收 VM 管理的 array。
 *
 * ## Returns
 * @return 枚举和物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE；目录枚举错误抛 IOException。物化异常跳过列表清理且字段发布可能触发共享 UB，见 system_fs_folder_list_common BUG。
 *
 * ## Constraints and lifecycle
 * 仅直接子项；筛选发生在 VM 对象构造之前，避免为排除项创建 wrapper。
 *
 * ## References
 * fs_registry.c:173 绑定 files；转调 system_fs_folder_list_common(context, result, true, false)。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Folders(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 返回 Folder 的直接子目录包装数组。
 *
 * ## Parameters
 * @param[in] context 含 Folder self 的无显式参数方法上下文。
 * @param[out] result 成功时接收 VM 管理的 array。
 *
 * ## Returns
 * @return 列举和物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE；宿主枚举错误抛 IOException。物化异常跳过列表清理且字段发布可能触发共享 UB，见 system_fs_folder_list_common BUG。
 *
 * ## Constraints and lifecycle
 * 筛选目录条目后才逐项创建对象；不递归扫描。
 *
 * ## References
 * fs_registry.c:175 绑定 folders；转调 system_fs_folder_list_common(context, result, false, true)。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Glob(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 在 Folder 的子树范围按 basename/path pattern 匹配条目，并返回匹配 wrapper 数组。
 *
 * ## Parameters
 * @param[in] context 含 Folder self、必需 string pattern 及可选 bool recursively 的调用上下文；缺省值当前为 true。
 * @param[out] result 成功时接收匹配项 VM array。
 *
 * ## Returns
 * @return Glob 与数组物化成功返回 ZR_TRUE；宿主枚举/匹配失败抛 IOException，普通 receiver/result/optional-bool 校验失败返回 ZR_FALSE。缺必需 pattern 抛 ArityError，非 string pattern 抛 TypeError；optional bool 错型普通 false。
 *
 * ## Constraints and lifecycle
 * pattern 和 self 路径仅同步借用；普通返回时 File_List_Free 释放列表。BUG: Glob 成功取得 list 后，若 system_fs_make_entry_array 的 VM 分配抛 MemoryError，longjmp 会跳过 :647 的 File_List_Free；无 C-list cleanup 注册。每项 NewEntryObject 还受共享 WriteStringField HASH_BUCKET UB 影响。省略 recursively 的公开默认已由 :639–641 传入 ZR_TRUE 及 fs_common.c:323–325 的省略参数处理确认是 true；descriptor 只限制 1..2 元。现有 runtime 用例显式传 true，不提供省略参数的额外执行覆盖。局部 root 风险见 system_fs_make_entry_array。
 *
 * ## References
 * fs_registry.c:177 绑定 glob；宿主匹配经 ZrLibrary_File_Glob，结果复用 system_fs_make_entry_array。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 复制 Folder 目录树到目标路径，并返回新 Folder wrapper。
 *
 * ## Parameters
 * @param[in] context 含 Folder self、必需 string targetPath 和可选 bool overwrite（当前缺省 false）。
 * @param[out] result 成功时接收目标 Folder 对象。
 *
 * ## Returns
 * @return 复制与目标 wrapper 交付成功返回 ZR_TRUE；宿主失败抛 IOException，普通 receiver/result/optional-bool 校验或包装失败返回 ZR_FALSE。缺必需 targetPath 抛 ArityError，非 string 抛 TypeError；optional bool 错型普通 false。
 *
 * ## Constraints and lifecycle
 * copy 完成后才构造目标 wrapper；构造失败不撤销副本。Folder 版本不刷新原 receiver。目标 NewEntryObject 经共享 WriteStringField，HASH_BUCKET 软失败可导致未定义结果，见本批字段发布 BUG。TODO: targetPath 跨 NewEntryObject 分配窗口的 pin/GC 稳定性仍待核。 宿主复制本身失败也不承诺原子性或回滚：overwrite 可先删除旧目标，随后读写或递归子项失败会保留已建目标、已写前缀或部分目录树。 覆盖删除目标发生在源打开之前，父目录建立发生在目标打开之前；后续失败可使旧目标已消失、目标仍不存在或只留下父目录，均不回滚。实际复制普通文件内容的分支忽略源与目标 CloseHandle 的返回值（file.c:825–832），因而成功不保证 close 成功。 BUG: 目标不得是源目录自身的子目录；例如现存 source/a 复制到尚不存在的 source/a/b，helper:851 先建 b 再于 :852 枚举 a，:865–866 又把新建 b 复制到 b/b，形成递归增长并可留下嵌套部分树；file_join_into 的截断误判成功使终止点不能承诺为路径拼接失败。Move 在 rename 失败后回退 Copy(true) 时同样可达。 BUG: overwrite=true 且现存目标是源路径的严格祖先目录时，递归 Delete(target) 会把源与该祖先树中的兄弟项一并删除；限定删除成功且无外部重建，随后 Copy 打开/列举源或 Move 的 rename/Copy 失败，callback 抛 IOException。目录 Copy 还可能先重建空目标再因源缺失失败（file.c:772–783/:847–853/:1259–1269）。 File/Folder wrapper 名称不校验实际源类型；Copy 在 file.c:1228–1239 按 stat 结果分派。因此实际源为目录的 File wrapper 也受“目标为源子目录时先建后枚举、不断纳入新目标”的递归 BUG 影响；Move 仅在 rename 失败后的 Copy 回退适用此递归路径。路径关系按本次 NormalizePath 输出的组件边界判断，不提供别名解析或并发文件系统原子保证。
 *
 * ## References
 * fs_registry.c:180 注册 Folder.copyTo；调用 ZrLibrary_File_Copy 与 ZrSystem_Fs_NewEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 移动 Folder 子树并返回指向目标路径的新 Folder wrapper。
 *
 * ## Parameters
 * @param[in] context 含 Folder self、必需 string targetPath 与可选 bool overwrite（当前缺省 false）。
 * @param[out] result 成功时接收目标 Folder 对象。
 *
 * ## Returns
 * @return move 与目标 wrapper 交付成功返回 ZR_TRUE；宿主失败抛 IOException，普通 receiver/result/optional-bool 校验或包装失败返回 ZR_FALSE。缺必需 targetPath 抛 ArityError，非 string 抛 TypeError；optional bool 错型普通 false。
 *
 * ## Constraints and lifecycle
 * 移动成功后旧路径可能不存在；Folder_MoveTo 不调用 RefreshEntryObject，原 self.fileInfo 保持旧快照。NewEntryObject 失败不回滚宿主移动；目标对象的 WriteStringField 软失败可能产生未定义结果，见本批共享字段 BUG。TODO: targetPath 跨 NewEntryObject 分配窗口的 pin/GC 地址仍待核。 移动并非原子事务：overwrite 可先删除旧目标；rename 失败后按 Copy(overwrite=true) 再 Delete(source) 回退，回退 copy 失败不执行末尾源删除（不撤销此前 overwrite 删除）却可留下部分目标，copy 成功但源删除失败也不恢复目标，可能两边都有内容。 覆盖删除目标发生在源打开之前，父目录建立发生在目标打开之前；后续失败可使旧目标已消失、目标仍不存在或只留下父目录，均不回滚。仅回退到实际复制普通文件内容的分支忽略源与目标 CloseHandle 的返回值（file.c:825–832），因而成功不保证 close 成功。 rename 成功分支不经过该复制 helper。BUG: overwrite=true 且 NormalizePath 后 source/target 为同一现存路径时，Move:1259–1261 先递归删除目标即源；删除成功且无外部重建后，rename:1264 与 Copy:1228–1231 因源不存在失败，callback 抛 IOException，源已丢失。此结论包含相同路径输入，不推断所有文件系统别名等价。 BUG: 目标不得是源目录自身的子目录；例如现存 source/a 复制到尚不存在的 source/a/b，helper:851 先建 b 再于 :852 枚举 a，:865–866 又把新建 b 复制到 b/b，形成递归增长并可留下嵌套部分树；file_join_into 的截断误判成功使终止点不能承诺为路径拼接失败。Move 在 rename 失败后回退 Copy(true) 时同样可达。 BUG: overwrite=true 且现存目标是源路径的严格祖先目录时，递归 Delete(target) 会把源与该祖先树中的兄弟项一并删除；限定删除成功且无外部重建，随后 Copy 打开/列举源或 Move 的 rename/Copy 失败，callback 抛 IOException。目录 Copy 还可能先重建空目标再因源缺失失败（file.c:772–783/:847–853/:1259–1269）。 File/Folder wrapper 名称不校验实际源类型；Copy 在 file.c:1228–1239 按 stat 结果分派。因此实际源为目录的 File wrapper 也受“目标为源子目录时先建后枚举、不断纳入新目标”的递归 BUG 影响；Move 仅在 rename 失败后的 Copy 回退适用此递归路径。路径关系按本次 NormalizePath 输出的组件边界判断，不提供别名解析或并发文件系统原子保证。 Move 在删除 overwrite 目标前也不预验源存在；源本已缺失时仍可能先删现存目标，再返回失败。
 *
 * ## References
 * fs_registry.c:183 注册 Folder.moveTo；调用 ZrLibrary_File_Move 和 ZrSystem_Fs_NewEntryObject。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Folder_Delete(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 删除 Folder receiver 指向的目录，是否递归由可选参数控制，并尝试刷新原对象快照。
 *
 * ## Parameters
 * @param[in] context 含 Folder self 及可选 bool recursively；省略时当前为 false。
 * @param[out] result 删除完成后写 VM null。
 *
 * ## Returns
 * @return 删除成功返回 ZR_TRUE；宿主删除失败抛 IOException；参数或 receiver 无效返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor 实际为 0..1 参数，省略 recursively 时默认 false；错误可选 bool 类型由 helper 普通返回 false。删除后 RefreshEntryObject 普通 false 被忽略，副作用不回滚，非局部异常仍退出。refresh 字符字段受共享 WriteStringField HASH_BUCKET UB 影响，见本批字段发布 BUG。TODO: 普通 refresh false 的公开快照策略见 Entry_Refresh。 recursively=true 时逐项删除子树，后续子项或最终 rmdir 失败会返回 false/IOException，但此前已删除项不恢复；recursively=false 只尝试删除空目录。
 *
 * ## References
 * fs_registry.c:186 注册 Folder.delete；调用 ZrLibrary_File_Delete 并尝试刷新 receiver。
 */
