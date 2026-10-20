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

TZrBool ZrSystem_Fs_SelfFullPath(SZrState *state, SZrObject *object, const TZrChar **outPath) {
    if (outPath != ZR_NULL) {
        *outPath = ZR_NULL;
    }
    if (state == ZR_NULL || object == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }
    *outPath = ZrSystem_Fs_GetStringField(state, object, "fullPath");
    return *outPath != ZR_NULL;
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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

TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *copyObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
    if (self == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath)) {
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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

TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    const TZrChar *targetPath = ZR_NULL;
    TZrBool overwrite = ZR_FALSE;
    SZrObject *copyObject;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
 * callback 同步借用当前 ZrLibCallContext、receiver 与 VM 实参。descriptor arity 先检查必需参数；必需 string/array 错型由 Read API 抛 TypeError；可选 string 缺省时使用默认值，但显式错型仍抛 TypeError，可选 bool 错型由 helper 返回普通 false。context、结果槽及普通校验失败也可返回 false。VM 非局部 MemoryError 不自动遍历 C 栈；一次性 helper、字节缓冲和目录列表由各自 caller-owned TryRun 事务清理后传播原 status；File.open 仅在 owner 字段完整发布后移交句柄，非 NULL wrapper 软失败例外见 File_Open。WriteStringField 的 HASH_BUCKET 软失败可让未初始化的 SZrTypeValue 进入 setter，影响构造、元数据快照和目标 wrapper，结果未定义；共享根因见 fs_common.c 的本批记录。多个变更 callback 忽略 RefreshEntryObject 的普通 false，但非局部异常仍会退出 callback。changeCurrentDirectory 修改宿主进程共享 cwd；并行相对路径 I/O 由宿主协调。
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
 * @fn TZrBool ZrSystem_Fs_SelfFullPath(SZrState *state, SZrObject *object, const TZrChar **outPath)
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
 * fs_registry.c:135 绑定 File.open；调用 ZrSystem_Fs_SelfFullPath、ZrLibrary_File_OpenHandle、ZrSystem_Fs_NewStreamObject 和 FinishObjectResult。
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
