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
 *  @note Windows 保留 UNC server/share 根；不支持的 C:relative 盘符相对路径返回 false。
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
 *  @pre result 至少具有 ZR_LIBRARY_MAX_PATH_LENGTH 字节容量，且不与输入重叠。
 *  @note 失败清空 result；过长时设置 errno=ENAMETOOLONG，不返回截断路径。
 */
ZR_LIBRARY_API void ZrLibrary_File_PathJoin(const TZrChar *path1, const TZrChar *path2, ZR_OUT TZrNativeString result);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateDirectorySingle(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateDirectories(TZrNativeString path);

ZR_LIBRARY_API TZrBool ZrLibrary_File_CreateEmpty(TZrNativeString path, TZrBool recursively);

ZR_LIBRARY_API TZrBool ZrLibrary_File_Delete(TZrNativeString path, TZrBool recursively);

/** @brief 复制实际文件或目录；先验证源与规范化路径关系，再修改目标。
 *  @return 源存在且两规范化路径相同为成功空操作；目标为源祖先或目录源的后代时返回 false。
 *  @note 普通复制失败可保留部分目标，不提供物理别名检测、并发路径事务或持久化保证。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath,
                                           TZrNativeString targetPath,
                                           TZrBool overwrite);

/** @brief 优先 rename，失败后复制并删除源；源存在性与重叠检查在覆盖删除前完成。
 *  @return 源存在且两规范化路径相同为成功空操作，与 overwrite 无关；危险组件重叠返回 false。
 *  @note 回退失败可留下部分目标；关闭失败与物理路径别名不在本组验证范围。
 */
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
 * @fn ZR_LIBRARY_API TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite);
 *
 * ## Summary
 * 同步复制 sourcePath 所指的实际文件或目录；在允许覆盖删除目标之前校验源存在性和规范化路径的组件关系。
 *
 * ## Parameters
 * @param[in] sourcePath 本次同步调用借用的、可读且以 NUL 结尾的源路径；函数不接管、不释放也不改写调用者字符串。
 * @param[in] targetPath 同样在整个调用期间保持有效的目标路径；允许与源路径字符串相同，但指针非 NULL 不等于内存有效性证明。
 * @param[in] overwrite 为 false 时拒绝已有的不同目标；为 true 时允许先递归删除已有目标。源存在且规范化字符串相同的空操作不受该值影响。
 *
 * ## Returns
 * @return ZR_TRUE 表示当前路径检查和复制流程返回成功，包括存在源的同路径空操作；ZR_FALSE 表示参数、规范化、源检查或复制步骤拒绝/失败，目标及父目录可能已被修改。
 *
 * ## Constraints and lifecycle
 * 先拒绝 NULL，再分别写入固定容量的栈上规范化缓冲区；随后检查规范化源的实际存在性，缺失源在修改目标前失败。不是对任意非 NULL 指针、可读长度或物理路径身份的认证。
 * 只有源已存在时才以规范化字符串的 strcmp 相等认定成功空操作；词法严格组件祖先判断使用字符前缀及分隔符边界。目标为源的严格祖先，或源为实际目录且目标为其严格后代时，在覆盖删除前以 EINVAL 拒绝。
 * 这里不证明大小写别名、硬链接/符号链接或其他物理别名关系；其他线程/进程可在检查与使用之间改变路径，调用方不能把这些检查当作文件系统事务或并发锁。
 * 宿主 errno 不是统一的结构化诊断对象，成功不负责清空旧 errno；并非每个内部失败路径都重新设置 errno，不能仅由它证明最初原因。该同步实现可能阻塞；无原子提交或持久化保证。
 * 存在的不同目标在 overwrite=true 时可先被删除；复制文件还可能先创建父目录，再打开目标。源打开、目标打开、读、写或递归步骤失败均不回滚之前的删除和创建，目标可能残缺。
 * 实际目录由目录复制路径递归，其他存在类型走文件内容路径；不按上层 File/Folder 包装名称强制类型。目录列表由内部获取/释放，打开的句柄沿普通路径关闭；关闭返回值被忽略，ZR_TRUE 不证明关闭成功。
 *
 * ## References
 * zr_vm_library/include/zr_vm_library/file.h:121
 * zr_vm_library/include/zr_vm_library/file.h:125
 * zr_vm_library/include/zr_vm_library/file.h:127
 * zr_vm_library/src/zr_vm_library/file_copy.c:19
 * zr_vm_library/src/zr_vm_library/file_copy.c:21
 * zr_vm_library/src/zr_vm_library/file_copy.c:25
 * zr_vm_library/src/zr_vm_library/file_copy.c:27
 * zr_vm_library/src/zr_vm_library/file_copy.c:28
 * zr_vm_library/src/zr_vm_library/file_copy.c:34
 * zr_vm_library/src/zr_vm_library/file_copy.c:47
 * zr_vm_library/src/zr_vm_library/file_copy.c:51
 * zr_vm_library/src/zr_vm_library/file_copy.c:60
 * zr_vm_library/src/zr_vm_library/file_copy.c:65
 * zr_vm_library/src/zr_vm_library/file_copy.c:73
 * zr_vm_library/src/zr_vm_library/file_copy.c:91
 * zr_vm_library/src/zr_vm_library/file_copy.c:97
 * zr_vm_library/src/zr_vm_library/file_copy.c:105
 * zr_vm_library/src/zr_vm_library/file_copy.c:109
 * zr_vm_library/src/zr_vm_library/file_copy.c:114
 * zr_vm_library/src/zr_vm_library/file_copy.c:130
 * zr_vm_library/src/zr_vm_library/file_copy.c:131
 * zr_vm_library/src/zr_vm_library/file_copy.c:145
 * zr_vm_library/src/zr_vm_library/file_copy.c:150
 * zr_vm_library/src/zr_vm_library/file_copy.c:157
 * zr_vm_library/src/zr_vm_library/file_copy.c:161
 * zr_vm_library/src/zr_vm_library/file_copy.c:166
 * zr_vm_library/src/zr_vm_library/file_copy.c:170
 * zr_vm_library/src/zr_vm_library/file_copy.c:175
 * zr_vm_library/src/zr_vm_library/file_copy.c:180
 * zr_vm_library/src/zr_vm_library/file_copy.c:183
 * zr_vm_library/src/zr_vm_library/file_copy.c:187
 * zr_vm_library/src/zr_vm_library/file_copy.c:231
 * zr_vm_library/src/zr_vm_library/file_path.c:13
 * zr_vm_library/src/zr_vm_library/file_path.c:17
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:250
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:327
 * tests/library/test_file_path_operations.c:76
 * tests/library/test_file_path_operations.c:82
 * tests/library/test_file_path_operations.c:95
 * tests/library/test_file_path_operations.c:99
 * tests/library/test_file_path_operations.c:104
 * tests/library/test_file_path_operations.c:109
 * tests/library/test_file_path_operations.c:112
 * tests/cmake/file-path-operations-tests.cmake:23
 * tests/cmake/file-path-operations-tests.cmake:25
 * tests/cmake/file-path-operations-tests.cmake:29
 */

/**
 * @fn ZR_LIBRARY_API TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite);
 *
 * ## Summary
 * 同步移动 sourcePath 所指的实际条目；先校验源与路径关系，再尝试 rename，任何 rename 失败均进入复制后删除源的回退。
 *
 * ## Parameters
 * @param[in] sourcePath 本次同步调用借用的、可读且 NUL 结尾的源路径；成功移动后原路径可能不存在，字符串存储仍归调用者。
 * @param[in] targetPath 整个调用期间保持有效的借用目标字符串；函数不取得其所有权，也不为调用者保存指针。
 * @param[in] overwrite false 拒绝已有的不同目标，true 允许在 rename 前删除它；源存在且规范化字符串相同时两种取值均成功空操作。
 *
 * ## Returns
 * @return ZR_TRUE 表示同路径空操作、rename 成功，或 Copy(true) 与随后的源 Delete 均返回成功；ZR_FALSE 可发生于回退复制或删除源之后，不能据此认定两边均未变化。
 *
 * ## Constraints and lifecycle
 * 先拒绝 NULL，再分别写入固定容量的栈上规范化缓冲区；随后检查规范化源的实际存在性，缺失源在修改目标前失败。不是对任意非 NULL 指针、可读长度或物理路径身份的认证。
 * 只有源已存在时才以规范化字符串的 strcmp 相等认定成功空操作；词法严格组件祖先判断使用字符前缀及分隔符边界。目标为源的严格祖先，或源为实际目录且目标为其严格后代时，在覆盖删除前以 EINVAL 拒绝。
 * 这里不证明大小写别名、硬链接/符号链接或其他物理别名关系；其他线程/进程可在检查与使用之间改变路径，调用方不能把这些检查当作文件系统事务或并发锁。
 * 宿主 errno 不是统一的结构化诊断对象，成功不负责清空旧 errno；并非每个内部失败路径都重新设置 errno，不能仅由它证明最初原因。该同步实现可能阻塞；无原子提交或持久化保证。
 * 对不同目标，overwrite=false 且目标已存在时拒绝；overwrite=true 可先递归删除已有目标。随后 rename 失败不区分错误种类，执行 Copy(normalizedSource, normalizedTarget, ZR_TRUE) && Delete(normalizedSource, ZR_TRUE)，并非仅 EXDEV 回退。
 * Copy 返回 false 时短路跳过最后的源删除，但目标可已残缺；Copy 成功而源删除失败可留下两份或部分源树。复制路径忽略关闭错误，不能由回退成功推导数据已持久化。函数不回滚已完成的宿主操作。
 *
 * ## References
 * zr_vm_library/include/zr_vm_library/file.h:129
 * zr_vm_library/include/zr_vm_library/file.h:133
 * zr_vm_library/include/zr_vm_library/file.h:135
 * zr_vm_library/src/zr_vm_library/file_copy.c:19
 * zr_vm_library/src/zr_vm_library/file_copy.c:25
 * zr_vm_library/src/zr_vm_library/file_copy.c:192
 * zr_vm_library/src/zr_vm_library/file_copy.c:197
 * zr_vm_library/src/zr_vm_library/file_copy.c:201
 * zr_vm_library/src/zr_vm_library/file_copy.c:206
 * zr_vm_library/src/zr_vm_library/file_copy.c:211
 * zr_vm_library/src/zr_vm_library/file_copy.c:214
 * zr_vm_library/src/zr_vm_library/file_copy.c:218
 * zr_vm_library/src/zr_vm_library/file_copy.c:222
 * zr_vm_library/src/zr_vm_library/file_copy.c:227
 * zr_vm_library/src/zr_vm_library/file_copy.c:231
 * zr_vm_library/src/zr_vm_library/file_copy.c:232
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:273
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:346
 * tests/library/test_file_path_operations.c:82
 * tests/library/test_file_path_operations.c:89
 * tests/library/test_file_path_operations.c:91
 * tests/library/test_file_path_operations.c:104
 * tests/library/test_file_path_operations.c:105
 * tests/library/test_file_path_operations.c:115
 * tests/cmake/file-path-operations-tests.cmake:23
 * tests/cmake/file-path-operations-tests.cmake:25
 * tests/cmake/file-path-operations-tests.cmake:29
 */
