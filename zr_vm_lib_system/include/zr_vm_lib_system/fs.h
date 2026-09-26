//
// zr.system.fs native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_FS_H
#define ZR_VM_LIB_SYSTEM_FS_H

#include "zr_vm_lib_system/conf.h"

/** @brief 以下回调由 zr.system.fs 的 native descriptor 调度；返回值表示 native 调用是否成功，业务结果写入 result。 */
/** @brief 取得进程工作目录的规范化绝对路径；目录变化会影响随后使用相对路径的文件调用。 */
TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 改变整个宿主进程的工作目录，供兼容脚本控制相对路径解析。 */
TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 探测路径是否存在；不存在以 bool 返回，不作为 I/O 异常。 */
TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 探测路径当前是否是普通文件。 */
TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 探测路径当前是否是目录。 */
TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 兼容入口：仅创建单级目录，失败经 IOException 传播。 */
TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 兼容入口：按需创建父目录，失败经 IOException 传播。 */
TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 删除文件或空目录；递归删除只由 Folder.delete 的显式参数提供。 */
TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 兼容入口：一次性读取文件内容，由辅助函数负责短期句柄的关闭。 */
TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 兼容入口：覆盖文件文本，成功时返回 bool。 */
TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 兼容入口：追加文件文本，成功时返回 bool。 */
TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 返回独立的 SystemFileInfo 快照，调用方不能把它当作实时文件状态。 */
TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 初始化 FileSystemEntry、File 或 Folder 的路径与初始元数据；构造目标由 native 调度提供。 */
TZrBool ZrSystem_Fs_Entry_Constructor(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 重查磁盘并返回当前存在性，同时更新包装对象的 fileInfo。 */
TZrBool ZrSystem_Fs_Entry_Exists(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 重查磁盘、更新 fileInfo，并将本次 SystemFileInfo 快照返回给脚本。 */
TZrBool ZrSystem_Fs_Entry_Refresh(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 打开 File 路径并把原生句柄的所有权交给 FileStream；调用方应 close 或 using。 */
TZrBool ZrSystem_Fs_File_Open(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 创建尚不存在的文件；可选参数决定是否创建父目录。 */
TZrBool ZrSystem_Fs_File_Create(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 在一次调用内打开、读取和关闭句柄，不返回流资源。 */
TZrBool ZrSystem_Fs_File_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 覆盖文件文本，返回写入字节数并尝试刷新路径元数据。 */
TZrBool ZrSystem_Fs_File_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 追加文件文本，返回写入字节数并尝试刷新路径元数据。 */
TZrBool ZrSystem_Fs_File_AppendText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 一次性读取文件字节为 0..255 整数数组；句柄在返回前关闭。 */
TZrBool ZrSystem_Fs_File_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 覆盖写入 0..255 整数数组，返回写入字节数。 */
TZrBool ZrSystem_Fs_File_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 追加写入 0..255 整数数组，返回写入字节数。 */
TZrBool ZrSystem_Fs_File_AppendBytes(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 复制文件并返回指向目标路径的新 File；原包装对象仍指向源路径。 */
TZrBool ZrSystem_Fs_File_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 移动文件并返回指向目标路径的新 File；原包装对象不会改绑目标。 */
TZrBool ZrSystem_Fs_File_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 删除文件路径，并尝试刷新原包装对象的存在性快照。 */
TZrBool ZrSystem_Fs_File_Delete(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 创建 Folder 对应目录，可选参数控制父目录创建。 */
TZrBool ZrSystem_Fs_Folder_Create(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 列举直接子项并按类型构造路径包装对象。 */
TZrBool ZrSystem_Fs_Folder_Entries(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 从直接子项中筛选文件包装对象。 */
TZrBool ZrSystem_Fs_Folder_Files(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 从直接子项中筛选目录包装对象。 */
TZrBool ZrSystem_Fs_Folder_Folders(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按通配模式列举子项；省略 recursively 时当前实现会递归。 */
TZrBool ZrSystem_Fs_Folder_Glob(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 复制目录树并返回指向目标路径的新 Folder。 */
TZrBool ZrSystem_Fs_Folder_CopyTo(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 移动目录树并返回新 Folder；原包装对象不改绑。 */
TZrBool ZrSystem_Fs_Folder_MoveTo(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 删除目录；仅在显式指定 recursively 时遍历子项。 */
TZrBool ZrSystem_Fs_Folder_Delete(ZrLibCallContext *context, SZrTypeValue *result);

/** @brief 从当前流位置读取最多 count 个字节；省略 count 时读到 EOF。 */
TZrBool ZrSystem_Fs_Stream_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 从当前流位置读取文本；count 是字节数，可能与 UTF-8 字符边界不同。 */
TZrBool ZrSystem_Fs_Stream_ReadText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 向可写流写入字节数组，返回实际写入字节数。 */
TZrBool ZrSystem_Fs_Stream_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 向可写流写入文本，返回实际写入字节数。 */
TZrBool ZrSystem_Fs_Stream_WriteText(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将打开句柄的数据同步到宿主，失败经 IOException 传播。 */
TZrBool ZrSystem_Fs_Stream_Flush(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 按 begin/current/end 原点移动流位置并返回新字节位置。 */
TZrBool ZrSystem_Fs_Stream_Seek(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 调整可写流长度；负数长度由平台文件层拒绝。 */
TZrBool ZrSystem_Fs_Stream_SetLength(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 显式关闭持有的句柄；同一入口也供 using 的 @close 调用。 */
TZrBool ZrSystem_Fs_Stream_Close(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_FS_H
