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

/** @brief 源码 Io 会话的文件句柄与逐块读取缓冲区，由 OpenRead/CloseRead 管理。
 *  buffer 只是临时借用视图；下一次 SourceRead 后旧视图失效。
 */
struct ZR_STRUCT_ALIGN SZrLibrary_File_Reader {
    TZrSize size;
    TZrLibrary_File_Ptr file;
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
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

/** @brief 目录扫描结果，entries 由 ListDirectory/Glob 分配并由 List_Free 释放。 */
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

/** TODO: 本接口只读取 path，却声明为可写 TZrNativeString；内部 const 路径调用在
 *  Clang 上产生丢弃 const 的告警。需连同公开函数指针兼容性核定签名调整。 */
ZR_LIBRARY_API TZrBool ZrLibrary_File_IsAbsolutePath(TZrNativeString path);

/** @brief 将宿主路径转成当前平台的绝对规范路径，供 project、CLI 和文件原生模块共用。
 *  @pre normalizedPath 指向 normalizedPathSize 字节的可写缓冲区；相对路径以当前工作目录为基准。
 *  @return 路径或输出容量无效时返回 false；成功时写入以零结尾的路径。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_File_NormalizePath(TZrNativeString path,
                                                    ZR_OUT TZrNativeString normalizedPath,
                                                    TZrSize normalizedPathSize);

/** @brief 从规范路径取父目录，供项目文件和文件原生模块定位同级资源。
 *  @pre directory 至少具有 ZR_LIBRARY_MAX_PATH_LENGTH 字节容量。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_File_GetDirectory(TZrNativeString path, ZR_OUT TZrNativeString directory);

ZR_LIBRARY_API TZrBool ZrLibrary_File_QueryInfo(TZrNativeString path, ZR_OUT SZrLibrary_File_Info *outInfo);

/** @brief 给固定长度路径缓冲区拼接目录和子路径；绝对的 path2 覆盖 path1。
 *  @pre result 至少具有 ZR_LIBRARY_MAX_PATH_LENGTH 字节容量；调用方须检查结果是否满足其路径约束。
 */
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

/** @brief 将目录快照收集为按路径排序的列表，供 glob、递归复制及 CLI 扫描使用。
 *  @note 有效入参下遍历失败时 outList 由本函数清空；成功后调用方须调用 List_Free。
 */
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

/** @brief 读取配置或源码文本，并从 global 的原生分配器返回零结尾缓冲区。
 *  @pre global 有效；调用方须用同一 global 的 RawFreeWithType 释放返回值。
 */
ZR_LIBRARY_API TZrNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TZrNativeString path);

/** @brief 打开供 Project、AOT 或核心 Io 逐块消费的文件读取会话。
 *  @pre global 有效，path 指向现存普通文件。
 *  @return 成功返回由 global 分配的 reader；调用方须以同一 global 调用 CloseRead。
 */
ZR_LIBRARY_API SZrLibrary_File_Reader *ZrLibrary_File_OpenRead(SZrGlobalState *global,
                                                               TZrNativeString path,
                                                               TZrBool isBinary);

ZR_LIBRARY_API void ZrLibrary_File_CloseRead(SZrGlobalState *global, SZrLibrary_File_Reader *reader);

/** @brief 项目源码加载器的 Io 回调组；reader 在 Io 关闭前由同一 global 持有。
 *  @note Load 成功后，Core_Io_Init 将读取和关闭回调与 reader 一起交给核心。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_File_SourceLoadImplementation(SZrState *state,
                                                                TZrNativeString path,
                                                                TZrNativeString md5,
                                                                SZrIo *io);

/** @brief 向 Core Io 借出 reader 的下一块缓存；返回指针在下次读取或关闭前有效。 */
ZR_LIBRARY_API TZrBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state,
                                                                   TZrPtr reader,
                                                                   ZR_OUT TZrSize *size);

/** @brief Core Io 的关闭回调，使用创建 reader 时的 global 释放它。 */
ZR_LIBRARY_API void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader);

#endif // ZR_VM_LIBRARY_FILE_H

/**
 * @fn TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath,
 *                                 TZrNativeString targetPath,
 *                                 TZrBool overwrite)
 *
 * ## Summary
 * 将源路径复制到目标路径；根据源的实际文件系统类型选择文件内容复制或目录树递归复制。
 *
 * ## Parameters
 * @param[in] sourcePath 本次调用读取的 NUL 结尾路径；函数不保留该指针。
 * @param[in] targetPath 本次调用写入的 NUL 结尾路径；函数不保留该指针。
 * @param[in] overwrite 两规范化字符串不同时，为真允许先递归删除现存目标，为假要求目标不存在；源存在且字符串相等时，两种取值均成功空操作。
 *
 * ## Returns
 * @return 源存在且复制流程成功或规范化路径字符串相等时返回真；失败时返回假。失败可发生在目标已删除、
 * 父目录已创建或部分内容已写入之后，这些副作用不会回滚。文件 helper（含目录子文件）在 close 前确定成功且忽略两个 CloseHandle 结果，故 true 不证明关闭或持久化成功；清理也可能改变最终 errno。
 *
 * ## Constraints and lifecycle
 * 路径规范化是字符串处理，不承诺识别物理别名；只有规范化后的字符串完全相等才走空操作。
 * 分派依据源的实际 `stat` 类型，而非调用者包装类型。BUG: overwrite=true、目标为源的严格词法组件祖先、
 * 目标递归删除成功且期间无外部重建时，先移除源及目标树内其他内容，随后复制失败也不恢复它们。
 * BUG: 实际目录源的严格词法后代目标不存在或允许覆盖且建/列目录成功时，先建目标再枚举源会把新目标继续作为下一层源递归，未排除目标或检测深度/环，可留部分树而无可靠终止保证。上述关系只针对规范化输出，不涵盖 symlink/hardlink/reparse/mount 物理别名；外部并发改路径/cwd的稳定结果不在保证内。
 *
 * ## References
 * 实现及文件系统失败顺序见 `zr_vm_library/src/zr_vm_library/file.c:1214-1240`；
 * System.Fs 调用见 `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:529,663`。
 */

/**
 * @fn TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath,
 *                                 TZrNativeString targetPath,
 *                                 TZrBool overwrite)
 *
 * ## Summary
 * 将源路径移到目标路径；优先使用宿主 `rename`，失败后尝试复制再递归删除源。
 *
 * ## Parameters
 * @param[in] sourcePath 本次调用使用的 NUL 结尾源路径；函数不保留该指针。
 * @param[in] targetPath 本次调用使用的 NUL 结尾目标路径；函数不保留该指针。
 * @param[in] overwrite 为真时允许预先递归删除现存目标，为假时目标必须不存在。
 *
 * ## Returns
 * @return `rename` 成功或 Copy 与 Delete 回退都成功时返回真；否则返回假。回退失败可留下部分
 * 目标，删除源失败也可能留下已复制目标和部分源树。Copy 忽略文件 close 结果，返回 true 后本函数仍可继续删源，即使 close 失败；true 不证明 close/持久化成功。
 *
 * ## Constraints and lifecycle
 * 目标存在且 overwrite=true 时，会在源有效性确认及 rename 前尝试删除。BUG: 递归删除成功且无外部重建时，若源缺失、
 * 两规范化字符串相同，或目标是源的严格词法组件祖先目录，后续会因源缺失失败而原目标/源数据
 * 已删除。BUG: rename 失败后对实际目录源的严格词法后代目标执行 Copy(true)，可进入上述自递归路径并留部分树。任何 rename 失败都尝试 Copy(true)&&Delete(source,true)，不是仅跨文件系统；Copy false 不执行最后的源删除，但此前预删除可能已删源。规范化只处理字符串/组件，不检测物理别名；外部并发路径变化无原子/稳定保证。
 *
 * ## References
 * 实现见 `zr_vm_library/src/zr_vm_library/file.c:1242-1269`；System.Fs 调用见
 * `zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:552,682`。
 */
