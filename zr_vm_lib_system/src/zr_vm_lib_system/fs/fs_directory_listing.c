#include "fs/fs_internal.h"
#include "fs_resource_roots_internal.h"
#include "zr_vm_core/exception.h"

#include <errno.h>
#include <string.h>

/* The native list remains owned by the caller while callbacks allocate. */
typedef struct ZrSystemFsEntryArray {
    const SZrLibrary_File_List *list;
    TZrBool filesOnly;
    TZrBool foldersOnly;
    SZrTypeValue *result;
    ZrSystemFsResourceRoots roots;
    TZrBool success;
} ZrSystemFsEntryArray;

static void system_fs_build_entry_array(SZrState *state, TZrPtr arguments) {
    ZrSystemFsEntryArray *context = arguments;
    SZrObject *array = ZrLib_Array_New(state);
    if (array == ZR_NULL) return;
    context->roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(array);
    for (TZrSize index = 0; index < context->list->count; ++index) {
        const SZrLibrary_File_ListEntry *entry = &context->list->entries[index];
        SZrTypeValue entryValue;
        if (context->filesOnly && entry->existence != ZR_LIBRARY_FILE_IS_FILE) continue;
        if (context->foldersOnly && entry->existence != ZR_LIBRARY_FILE_IS_DIRECTORY) continue;
        SZrObject *object = ZrSystem_Fs_NewEntryObject(state,
            entry->existence == ZR_LIBRARY_FILE_IS_DIRECTORY ? "Folder" :
            entry->existence == ZR_LIBRARY_FILE_IS_FILE ? "File" : "FileSystemEntry",
            entry->path, entry->path);
        if (object == ZR_NULL) return;
        context->roots.objects[1] = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        ZrLib_Value_SetObject(state, &entryValue, object, ZR_VALUE_TYPE_OBJECT);
        array = system_fs_resource_object(state, &context->roots, 0u);
        if (!ZrLib_Array_PushValue(state, array, &entryValue)) return;
        context->roots.objects[1] = ZR_NULL;
    }
    array = system_fs_resource_object(state, &context->roots, 0u);
    ZrLib_Value_SetObject(state, context->result, array, ZR_VALUE_TYPE_ARRAY);
    context->success = ZR_TRUE;
}

/* Consumes the successful native enumeration on every path, including Throw. */
static TZrBool system_fs_make_entry_array(SZrState *state,
                                          SZrLibrary_File_List *list,
                                          TZrBool filesOnly,
                                          TZrBool foldersOnly,
                                          SZrTypeValue *result) {
    ZrSystemFsEntryArray context = {0};
    if (state == ZR_NULL || list == ZR_NULL || result == ZR_NULL ||
        !system_fs_resource_roots_begin(state, &context.roots)) {
        ZrLibrary_File_List_Free(list);
        return ZR_FALSE;
    }
    context.list = list;
    context.filesOnly = filesOnly;
    context.foldersOnly = foldersOnly;
    context.result = result;
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, system_fs_build_entry_array, &context);
    int savedErrno = errno;
    ZrLibrary_File_List_Free(list);
    system_fs_resource_roots_end(state, &context.roots);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
        return ZR_FALSE;
    }
    errno = savedErrno;
    return context.success;
}

/* 三种直接子项视图共享一次平台枚举，File/Folder 筛选在物化脚本对象前完成。 */
static TZrBool system_fs_folder_list_common(ZrLibCallContext *context,
                                            SZrTypeValue *result,
                                            TZrBool filesOnly,
                                            TZrBool foldersOnly) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    SZrLibrary_File_List list;
    memset(&list, 0, sizeof(list));
    if (self == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_ListDirectory((TZrNativeString)fullPath, ZR_FALSE, &list)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "entries", fullPath);
    }
    return system_fs_make_entry_array(context->state, &list, filesOnly, foldersOnly, result);
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
    memset(&list, 0, sizeof(list));
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &pattern) ||
        !ZrSystem_Fs_ReadOptionalBoolArgument(context, 1, ZR_TRUE, &recursively)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_Glob((TZrNativeString)fullPath, (TZrNativeString)pattern, recursively, &list)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "glob", fullPath);
    }
    return system_fs_make_entry_array(context->state, &list, ZR_FALSE, ZR_FALSE, result);
}


/**
 * @fn TZrBool system_fs_make_entry_array(SZrState *state, SZrLibrary_File_List *list, TZrBool filesOnly, TZrBool foldersOnly, SZrTypeValue *result)
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
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。
 *
 * ## References
 * 仅由 system_fs_folder_list_common 与 ZrSystem_Fs_Folder_Glob 调用；新对象经 ZrSystem_Fs_NewEntryObject 创建，最后经 ZrLib_Array_PushValue 入数组。
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
 * @return ListDirectory 与数组物化成功返回 ZR_TRUE；普通输入/物化失败返回 ZR_FALSE，宿主枚举失败抛 IOException。VM 异常在释放原生列表后按原 status 传播。
 *
 * ## Constraints and lifecycle
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。
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
 * @return 列举与数组物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE，宿主枚举失败抛 IOException。VM 异常在释放列表后传播。
 *
 * ## Constraints and lifecycle
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。
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
 * @return 枚举和物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE；目录枚举错误抛 IOException。物化异常在列表清理后按原 status 传播。
 *
 * ## Constraints and lifecycle
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。
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
 * @return 列举和物化成功返回 ZR_TRUE；普通 receiver/数组校验失败返回 ZR_FALSE；宿主枚举错误抛 IOException。物化异常在列表清理后按原 status 传播。
 *
 * ## Constraints and lifecycle
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。
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
 * 成功取得的 native list 由 system_fs_make_entry_array 消费，成功、普通 false 和 VM 异常都释放一次。caller-owned AOT 根保护未发布数组及当前条目；每次分配后从 root slot 重取 array。Throw 恢复保留 caller 帧，清理列表后显式 Pop，再传播原 status。数组/条目内部构造和底层 Library pin 的独立 GC 风险未据此授予修复信用。 省略 recursively 的原默认 true 保持不变；本组夹具显式传 false，不提供省略参数的额外执行覆盖。
 *
 * ## References
 * fs_registry.c:177 绑定 glob；宿主匹配经 ZrLibrary_File_Glob，结果复用 system_fs_make_entry_array。
 */
