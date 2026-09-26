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
