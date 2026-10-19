#include "fs/fs_internal.h"
#include "zr_vm_core/exception.h"

#include <errno.h>
#include <string.h>

/* One-shot operations own the handle until close, including a VM Throw. */
typedef enum ZrSystemFsFileOperation {
    ZR_FS_FILE_READ_TEXT, ZR_FS_FILE_READ_BYTES, ZR_FS_FILE_WRITE_TEXT, ZR_FS_FILE_WRITE_BYTES
} ZrSystemFsFileOperation;

typedef struct ZrSystemFsFileIo {
    ZrSystemFsFileOperation operation;
    TZrLibrary_File_Handle handle;
    SZrTypeValue *result;
    const TZrChar *text;
    SZrObject *array;
    TZrInt64 *outWritten;
    TZrBool success;
    int operationErrno;
} ZrSystemFsFileIo;

static void system_fs_perform_file_io(SZrState *state, TZrPtr arguments) {
    ZrSystemFsFileIo *context = arguments;
    switch (context->operation) {
        case ZR_FS_FILE_READ_TEXT:
            context->success = ZrSystem_Fs_ReadTextFromHandle(state, context->handle, -1, context->result, ZR_NULL);
            break;
        case ZR_FS_FILE_READ_BYTES:
            context->success = ZrSystem_Fs_ReadBytesFromHandle(state, context->handle, -1, context->result, ZR_NULL);
            break;
        case ZR_FS_FILE_WRITE_TEXT:
            context->success = ZrSystem_Fs_WriteTextToHandle(state, context->handle, context->text, context->outWritten);
            break;
        case ZR_FS_FILE_WRITE_BYTES:
            context->success = ZrSystem_Fs_WriteBytesToHandle(state, context->handle, context->array, context->outWritten);
            break;
    }
    context->operationErrno = errno;
}

static TZrBool system_fs_run_file_io(SZrState *state, const TZrChar *path,
                                      const TZrChar *mode, ZrSystemFsFileIo *context) {
    SZrLibrary_File_StreamOpenResult opened;
    memset(&opened, 0, sizeof(opened));
    if (!ZrLibrary_File_OpenHandle((TZrNativeString)path, (TZrNativeString)mode, &opened)) {
        return ZR_FALSE;
    }
    context->handle = opened.handle;
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, system_fs_perform_file_io, context);
    TZrBool closed = ZrLibrary_File_CloseHandle(opened.handle);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
        return ZR_FALSE;
    }
    if (!context->success) {
        errno = context->operationErrno;
        return ZR_FALSE;
    }
    return closed;
}

static TZrBool system_fs_read_text_file(SZrState *state, const TZrChar *path, SZrTypeValue *result) {
    ZrSystemFsFileIo context = {0};
    context.operation = ZR_FS_FILE_READ_TEXT;
    context.result = result;
    return system_fs_run_file_io(state, path, "r", &context);
}

static TZrBool system_fs_read_bytes_file(SZrState *state, const TZrChar *path, SZrTypeValue *result) {
    ZrSystemFsFileIo context = {0};
    context.operation = ZR_FS_FILE_READ_BYTES;
    context.result = result;
    return system_fs_run_file_io(state, path, "rb", &context);
}

static TZrBool system_fs_write_text_file(SZrState *state, const TZrChar *path, const TZrChar *mode,
                                         const TZrChar *text, TZrInt64 *outWritten) {
    ZrSystemFsFileIo context = {0};
    context.operation = ZR_FS_FILE_WRITE_TEXT;
    context.text = text;
    context.outWritten = outWritten;
    return system_fs_run_file_io(state, path, mode, &context);
}

static TZrBool system_fs_write_bytes_file(SZrState *state, const TZrChar *path, const TZrChar *mode,
                                          SZrObject *array, TZrInt64 *outWritten) {
    ZrSystemFsFileIo context = {0};
    context.operation = ZR_FS_FILE_WRITE_BYTES;
    context.array = array;
    context.outWritten = outWritten;
    return system_fs_run_file_io(state, path, mode, &context);
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

TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    const TZrChar *fullPath = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath)) {
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
    if (self == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath)) {
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
        !ZrSystem_Fs_SelfFullPath(context->state, self, &fullPath) ||
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
 * 临时句柄由 system_fs_run_file_io 在成功、普通 false 和捕获 VM 异常后关闭一次；异常清理后按原 status 重抛。ReadTextFromHandle 按实际字节数发布并先释放其文本缓冲，不回滚读取位置。
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
 * 临时 handle 不转交 VM 对象；字节缓冲由 ReadBytesFromHandle 清理，handle 由本层清理，随后传播原异常。普通失败保持主要 I/O errno；成功后的 close 失败仍返回 false。
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
 * 一次性事务关闭临时 handle 后返回或传播原异常。文本仍按 strlen 写入，内嵌 NUL 后缀不写；需要二进制内容应使用 writeBytes。宿主短写失败可能留下已写前缀；w 打开可截断、a 打开可创建文件，这些副作用不回滚。
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
 * RAW_CANONICAL 的真实 Array_Get 物化可能抛出；WriteBytesToHandle 先释放 native bytes，本层再关闭临时 handle 后重抛原 status。普通 false 保留原 errno。所有元素在首次写入前校验，但 w/a 打开副作用和宿主部分写入不回滚；本组原生夹具不证明脚本数组 lowering 选择 RAW_CANONICAL。
 *
 * ## References
 * 由 ZrSystem_Fs_File_WriteBytes 和 File_AppendBytes 调用，实际写入委托 ZrSystem_Fs_WriteBytesToHandle。
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
 * @return 完整读取成功返回 ZR_TRUE；helper 的普通 I/O 失败抛 IOException。context/result 等普通校验失败返回 ZR_FALSE；缺必需 path 抛 ArityError，非 string path 由 ReadString 抛 TypeError。VM 异常在资源清理后按原 status 传播。
 *
 * ## Constraints and lifecycle
 * 共享 helper 关闭临时 handle 后返回或传播原异常；文本按实际字节数构造，保留内嵌 NUL。不把失败的字符串创建作为成功 null 结果发布。
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
 * 共享 helper 清理缓冲和临时 handle 后传播原异常。读取保留内嵌 NUL；普通成功后仍调用 RefreshEntryObject，其普通 false 仍被忽略，读取位置与宿主状态不回滚。
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
 * 数组归 VM 管理；共享 helper 在 VM 异常后先释放 C bytes，再关闭临时 handle，然后传播原 status。普通 refresh false 仍被忽略，不承诺对象快照或文件位置回滚；底层 Array_New 内部构造/GC 生命周期不在本组修复声明内。
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
 * 字节缓冲与临时 handle 在异常后分层清理；RAW 物化异常发生在首次写入前。宿主打开可截断目标，短写可留下前缀，均不回滚。普通 RefreshEntryObject false 仍被忽略；非法数组类型仍由 ReadArray 抛 TypeError。
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
 * 使用 mode a；共享缓冲与 handle 所有者各清理一次后传播原 status。首次写前校验所有 byte 元素；打开可创建文件，宿主部分写入不回滚。普通 RefreshEntryObject false 仍被忽略。
 *
 * ## References
 * fs_registry.c:154 绑定方法；调用 system_fs_write_bytes_file(mode a) 与 RefreshEntryObject。
 */
