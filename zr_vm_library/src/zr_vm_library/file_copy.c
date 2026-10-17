/* Copy/move validate source and lexical component overlap before changing targets. */
#include "file_path_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#if defined(ZR_PLATFORM_WIN)
#include <io.h>
#include <sys/stat.h>
#define ZR_FILE_NATIVE_OPEN _open
#else
#include <sys/stat.h>
#include <unistd.h>
#define ZR_FILE_NATIVE_OPEN open
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static TZrBool file_path_is_strict_ancestor(const TZrChar *parent, const TZrChar *child) {
    TZrSize length = strlen(parent);
    return length > 0 && strlen(child) > length && strncmp(parent, child, length) == 0 &&
           (file_is_separator(parent[length - 1]) || file_is_separator(child[length]));
}

static TZrBool file_copy_paths_are_disjoint(const TZrChar *source, const TZrChar *target,
                                           EZrLibrary_File_Exist sourceType) {
    if (file_path_is_strict_ancestor(target, source) ||
        (sourceType == ZR_LIBRARY_FILE_IS_DIRECTORY && file_path_is_strict_ancestor(source, target))) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
static TZrBool file_copy_file_contents(const TZrChar *sourcePath,
                                       const TZrChar *targetPath,
                                       TZrBool overwrite) {
    TZrLibrary_File_Handle sourceHandle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    TZrLibrary_File_Handle targetHandle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    char buffer[65536];
    TZrBool success = ZR_FALSE;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete((TZrNativeString)targetPath, ZR_TRUE)) {
        return ZR_FALSE;
    }

    {
        SZrLibrary_File_StreamOpenResult sourceOpen;
        TZrChar parentPath[ZR_LIBRARY_MAX_PATH_LENGTH];
        memset(&sourceOpen, 0, sizeof(sourceOpen));
        if (!ZrLibrary_File_OpenHandle((TZrNativeString)sourcePath, "rb", &sourceOpen)) {
            return ZR_FALSE;
        }
        sourceHandle = sourceOpen.handle;

        if (ZrLibrary_File_GetDirectory((TZrNativeString)targetPath, parentPath) &&
            parentPath[0] != '\0' &&
            ZrLibrary_File_Exist(parentPath) == ZR_LIBRARY_FILE_NOT_EXIST &&
            !ZrLibrary_File_CreateDirectories(parentPath)) {
            goto cleanup;
        }

#if defined(ZR_PLATFORM_WIN)
        targetHandle = ZR_FILE_NATIVE_OPEN(targetPath,
                                           (overwrite ? (O_WRONLY | O_CREAT | O_TRUNC | O_BINARY)
                                                      : (O_WRONLY | O_CREAT | O_EXCL | O_BINARY)),
                                           _S_IREAD | _S_IWRITE);
#else
        targetHandle = ZR_FILE_NATIVE_OPEN(targetPath,
                                           (overwrite ? (O_WRONLY | O_CREAT | O_TRUNC | O_BINARY)
                                                      : (O_WRONLY | O_CREAT | O_EXCL | O_BINARY)),
                                           0666);
#endif
    }
    if (targetHandle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        goto cleanup;
    }

    for (;;) {
        TZrSize readSize = 0;
        TZrSize writtenSize = 0;
        if (!ZrLibrary_File_ReadHandle(sourceHandle, buffer, sizeof(buffer), &readSize)) {
            goto cleanup;
        }
        if (readSize == 0) {
            break;
        }
        if (!ZrLibrary_File_WriteHandle(targetHandle, buffer, readSize, &writtenSize) || writtenSize != readSize) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (sourceHandle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        ZrLibrary_File_CloseHandle(sourceHandle);
    }
    if (targetHandle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        ZrLibrary_File_CloseHandle(targetHandle);
    }
    return success;
}

static TZrBool file_copy_directory_recursive(const TZrChar *sourcePath,
                                             const TZrChar *targetPath,
                                             TZrBool overwrite) {
    SZrLibrary_File_List children;
    TZrSize index;
    TZrBool success = ZR_FALSE;

    memset(&children, 0, sizeof(children));
    if (!overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete((TZrNativeString)targetPath, ZR_TRUE)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_CreateDirectories((TZrNativeString)targetPath) ||
        !ZrLibrary_File_ListDirectory((TZrNativeString)sourcePath, ZR_FALSE, &children)) {
        return ZR_FALSE;
    }

    success = ZR_TRUE;
    for (index = 0; index < children.count; index++) {
        const TZrChar *childName = file_basename_ptr(children.entries[index].path);
        TZrChar targetChildPath[ZR_LIBRARY_MAX_PATH_LENGTH];
        if (!file_join_into(targetPath, childName, targetChildPath, sizeof(targetChildPath))) {
            success = ZR_FALSE;
            break;
        }

        if (children.entries[index].existence == ZR_LIBRARY_FILE_IS_DIRECTORY) {
            if (!file_copy_directory_recursive(children.entries[index].path, targetChildPath, overwrite)) {
                success = ZR_FALSE;
                break;
            }
        } else {
            if (!file_copy_file_contents(children.entries[index].path, targetChildPath, overwrite)) {
                success = ZR_FALSE;
                break;
            }
        }
    }

    ZrLibrary_File_List_Free(&children);
    return success;
}

TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite) {
    TZrChar normalizedSource[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTarget[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(sourcePath, normalizedSource, sizeof(normalizedSource)) ||
        !ZrLibrary_File_NormalizePath(targetPath, normalizedTarget, sizeof(normalizedTarget))) {
        return ZR_FALSE;
    }

    existence = ZrLibrary_File_Exist(normalizedSource);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (file_string_equals(normalizedSource, normalizedTarget)) {
        return ZR_TRUE;
    }
    if (!file_copy_paths_are_disjoint(normalizedSource, normalizedTarget, existence)) {
        return ZR_FALSE;
    }

    return existence == ZR_LIBRARY_FILE_IS_DIRECTORY
                   ? file_copy_directory_recursive(normalizedSource, normalizedTarget, overwrite)
                   : file_copy_file_contents(normalizedSource, normalizedTarget, overwrite);
}

TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite) {
    TZrChar normalizedSource[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTarget[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(sourcePath, normalizedSource, sizeof(normalizedSource)) ||
        !ZrLibrary_File_NormalizePath(targetPath, normalizedTarget, sizeof(normalizedTarget))) {
        return ZR_FALSE;
    }

    existence = ZrLibrary_File_Exist(normalizedSource);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (file_string_equals(normalizedSource, normalizedTarget)) {
        return ZR_TRUE;
    }
    if (!file_copy_paths_are_disjoint(normalizedSource, normalizedTarget, existence)) {
        return ZR_FALSE;
    }

    if (!overwrite && ZrLibrary_File_Exist(normalizedTarget) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist(normalizedTarget) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete(normalizedTarget, ZR_TRUE)) {
        return ZR_FALSE;
    }

    if (rename(normalizedSource, normalizedTarget) == 0) {
        return ZR_TRUE;
    }

    return ZrLibrary_File_Copy(normalizedSource, normalizedTarget, ZR_TRUE) &&
           ZrLibrary_File_Delete(normalizedSource, ZR_TRUE);
}

/**
 * @fn static TZrBool file_path_is_strict_ancestor(const TZrChar *parent, const TZrChar *child)
 * @brief 在已规范化的路径上判定严格词法组件祖先，避免把 source-other 误判为 source 的后代。
 *
 * ## Summary
 * 在已规范化的路径上判定严格词法组件祖先，避免把 source-other 误判为 source 的后代。
 * 仅由路径重叠门禁同步调用，不访问文件系统。
 *
 * ## Parameters
 * @param[in] parent 可读且 NUL 结尾的候选祖先文本，调用期间保持稳定；本 helper 不接收 NULL。
 * @param[in] child 可读且 NUL 结尾的候选后代文本，调用期间保持稳定；与 parent 文本相同不满足严格祖先。
 *
 * ## Returns
 * @return 仅当 parent 非空、child 更长、字节前缀相同且边界位于分隔符时返回真；其它可读文本组合返回假。
 *
 * ## Constraints and lifecycle
 * 使用 strlen/strncmp 逐字节比较。分隔符可以在 parent 末尾或 child 的前缀后一位；大小写、硬链接、符号链接和并发重命名不由本判定解析。只借用两文本，不保存地址、不分配或转移资源；调用方负责稳定输入，不提供线程间锁。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/file_copy.c:20
 * zr_vm_library/src/zr_vm_library/file_copy.c:21
 * zr_vm_library/src/zr_vm_library/file_copy.c:22
 * zr_vm_library/src/zr_vm_library/file_copy.c:27
 * zr_vm_library/src/zr_vm_library/file_copy.c:28
 * zr_vm_library/src/zr_vm_library/file_path.c:13
 * tests/library/test_file_path_operations.c:99
 * zr_vm_library/src/zr_vm_library/file_copy.c:19
 */

/**
 * @fn static TZrBool file_copy_paths_are_disjoint(const TZrChar *source, const TZrChar *target, EZrLibrary_File_Exist sourceType)
 * @brief 在公开 Copy/Move 可能删除目标前，拒绝目标包含源或目录源包含目标的危险词法组件关系。
 *
 * ## Summary
 * 在公开 Copy/Move 可能删除目标前，拒绝目标包含源或目录源包含目标的危险词法组件关系。
 * 公开入口已完成规范化、源存在性检查和相同文本成功空操作之后调用。
 *
 * ## Parameters
 * @param[in] source 调用期间有效、NUL 结尾的规范化源路径；调用方已判定源存在。
 * @param[in] target 调用期间有效、NUL 结尾的规范化目标路径；相同规范化文本已由上层处理。
 * @param[in] sourceType 源存在性快照；只有 DIRECTORY 启用源为目标祖先的第二项拒绝。
 *
 * ## Returns
 * @return 危险组件关系返回假并设置 errno=EINVAL；未命中这两项拒绝返回真，不代表物理对象不相交或后续 I/O 必然成功。
 *
 * ## Constraints and lifecycle
 * 不修改文件系统、路径文本或保存指针。不检查 NULL、真实文件身份、访问权限、链接环或外部并发；必须与当前公开入口的顺序配合，不能独立代替源存在性和同路径处理。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/file_copy.c:27
 * zr_vm_library/src/zr_vm_library/file_copy.c:28
 * zr_vm_library/src/zr_vm_library/file_copy.c:29
 * zr_vm_library/src/zr_vm_library/file_copy.c:175
 * zr_vm_library/src/zr_vm_library/file_copy.c:180
 * zr_vm_library/src/zr_vm_library/file_copy.c:183
 * zr_vm_library/src/zr_vm_library/file_copy.c:206
 * zr_vm_library/src/zr_vm_library/file_copy.c:211
 * zr_vm_library/src/zr_vm_library/file_copy.c:214
 * tests/library/test_file_path_operations.c:81
 * tests/library/test_file_path_operations.c:83
 * zr_vm_library/src/zr_vm_library/file_copy.c:25
 */

/**
 * @fn static TZrBool file_copy_file_contents(const TZrChar *sourcePath, const TZrChar *targetPath, TZrBool overwrite)
 * @brief 为公开复制和目录递归复制的非目录项，同步把源内容分块写入目标，并在取得句柄后统一尝试关闭。
 *
 * ## Summary
 * 为公开复制和目录递归复制的非目录项，同步把源内容分块写入目标，并在取得句柄后统一尝试关闭。
 * 当前调用链已先验证公开源与目标的危险词法关系；本私有 helper 自身不重新做这项检查。
 *
 * ## Parameters
 * @param[in] sourcePath 调用期间稳定的 NUL 结尾源路径；本函数检查 NULL 并以 rb 打开，实际可读性由宿主决定。
 * @param[in] targetPath 调用期间稳定的 NUL 结尾目标路径；覆盖模式可递归删除原目标，之后可能创建父目录和新文件。
 * @param[in] overwrite 假时拒绝已存在目标并用 O_EXCL；真时先删除检测到的目标，再用 O_TRUNC 创建/打开。
 *
 * ## Returns
 * @return 读到零字节 EOF 且此前每次写入数量等于读取数量时返回真；参数、目标冲突、删除、开源、建父目录、开目标、读取或写入失败返回假。返回真不包含 CloseHandle 成功或落盘持久化保证。
 *
 * ## Constraints and lifecycle
 * 使用 65536 字节栈缓冲，每次最多一次读和一次写，短写直接失败，不重试余量。源打开前覆盖删除可能已发生；GetDirectory 失败时不会进入建父目录分支，仍继续尝试打开目标。成功取得的源/目标描述符各尝试关闭一次，关闭返回值被忽略，也不承诺保留最早 errno。失败不回滚已删目标、父目录或部分数据；无元数据复制、链接身份保护或文件系统事务。参数只借用至同步返回，句柄不向调用方交付。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/file_copy.c:47
 * zr_vm_library/src/zr_vm_library/file_copy.c:51
 * zr_vm_library/src/zr_vm_library/file_copy.c:60
 * zr_vm_library/src/zr_vm_library/file_copy.c:65
 * zr_vm_library/src/zr_vm_library/file_copy.c:73
 * zr_vm_library/src/zr_vm_library/file_copy.c:78
 * zr_vm_library/src/zr_vm_library/file_copy.c:91
 * zr_vm_library/src/zr_vm_library/file_copy.c:94
 * zr_vm_library/src/zr_vm_library/file_copy.c:97
 * zr_vm_library/src/zr_vm_library/file_copy.c:105
 * zr_vm_library/src/zr_vm_library/file_copy.c:108
 * zr_vm_library/src/zr_vm_library/file_copy.c:150
 * zr_vm_library/src/zr_vm_library/file_copy.c:189
 * zr_vm_library/src/zr_vm_library/file.c:714
 * zr_vm_library/src/zr_vm_library/file.c:759
 * zr_vm_library/src/zr_vm_library/file.c:772
 * zr_vm_library/src/zr_vm_library/file.c:812
 * tests/library/test_file_path_operations.c:112
 * zr_vm_library/src/zr_vm_library/file_copy.c:34
 */

/**
 * @fn static TZrBool file_copy_directory_recursive(const TZrChar *sourcePath, const TZrChar *targetPath, TZrBool overwrite)
 * @brief 在目标目录建立后取得源的直接子项快照，按项递归复制目录或复制非目录内容，并释放快照池。
 *
 * ## Summary
 * 在目标目录建立后取得源的直接子项快照，按项递归复制目录或复制非目录内容，并释放快照池。
 * 公开入口已做顶层危险词法重叠检查；递归子调用在父调用的有效子项快照和局部目标文本上同步执行。
 *
 * ## Parameters
 * @param[in] sourcePath 调用期间有效的规范化目录源文本；实际枚举由 ListDirectory 校验，递归 child 地址借自父快照。
 * @param[in] targetPath 调用期间有效的目标文本；目标可能先被递归删除并重建，不与源树安全关系检查分离使用。
 * @param[in] overwrite 假时拒绝现存目标；真时先尝试删除现存目标，再创建目录，递归子项沿用此值。
 *
 * ## Returns
 * @return 建目录、列举及所有子项复制成功返回真；首个失败返回假。成功枚举后的普通完成和循环失败都会 List_Free，ListDirectory 自身失败时已清空其输出。
 *
 * ## Constraints and lifecycle
 * children 从零初始化；CreateDirectories 成功后 ListDirectory(FALSE) 取得仅直接子项，basename 指针借自该列表，targetChildPath 是每层栈缓冲。file_join_into 截断/错误返回假时停止，不继续使用截断路径。递归返回前列表保持有效，之后释放 entries，局部路径和列表地址不逃逸。已建目录和已复制项不回滚；没有深度上限、物理链接环检测或全树原子快照。无显式文件系统同步；非目录项交给文件 helper，不证明为普通文件。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/file_copy.c:121
 * zr_vm_library/src/zr_vm_library/file_copy.c:122
 * zr_vm_library/src/zr_vm_library/file_copy.c:126
 * zr_vm_library/src/zr_vm_library/file_copy.c:130
 * zr_vm_library/src/zr_vm_library/file_copy.c:131
 * zr_vm_library/src/zr_vm_library/file_copy.c:137
 * zr_vm_library/src/zr_vm_library/file_copy.c:139
 * zr_vm_library/src/zr_vm_library/file_copy.c:145
 * zr_vm_library/src/zr_vm_library/file_copy.c:150
 * zr_vm_library/src/zr_vm_library/file_copy.c:157
 * zr_vm_library/src/zr_vm_library/file_copy.c:188
 * zr_vm_library/src/zr_vm_library/file.c:625
 * zr_vm_library/src/zr_vm_library/file.c:635
 * zr_vm_library/src/zr_vm_library/file.c:643
 * zr_vm_library/src/zr_vm_library/file.c:703
 * zr_vm_library/src/zr_vm_library/file_path.c:284
 * zr_vm_library/src/zr_vm_library/file_path.c:301
 * tests/library/test_file_path_operations.c:99
 * zr_vm_library/src/zr_vm_library/file_copy.c:114
 */

/**
 * @fn TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite)
 * @brief 为 Library 与 System.Fs 提供同步文件或目录复制入口，在目标可被修改之前完成源和规范化词法关系检查。
 *
 * ## Summary
 * 为 Library 与 System.Fs 提供同步文件或目录复制入口，在目标可被修改之前完成源和规范化词法关系检查。
 * 用于实际宿主路径；VM 对象封装由 System.Fs 调用方完成，本接口不创建或持有 VM 对象。
 *
 * ## Parameters
 * @param[in] sourcePath 只读借用的 NUL 结尾宿主源路径；NULL 被拒绝，相对路径按规范化时的进程工作目录解释。
 * @param[in] targetPath 只读借用的 NUL 结尾宿主目标路径；NULL 被拒绝，调用方在路径读取期间保持文本稳定。
 * @param[in] overwrite 控制不同目标的存在冲突与覆盖删除；存在源和相同规范化文本的空操作不受此值影响。
 *
 * ## Returns
 * @return 源存在且两规范化文本相同，或分派的复制 helper 成功时返回真；NULL、规范化失败、源不可判为存在、危险组件重叠或后续 I/O 失败返回假。源检查返回 NOT_EXIST 时设置 ENOENT，NULL/危险重叠设置 EINVAL，其他失败的 errno 由下层及清理决定。
 *
 * ## Constraints and lifecycle
 * 顺序为两路径规范化、源存在性、同文本空操作、组件重叠门禁、实际源类型分派。仅 DIRECTORY 走树复制，其它存在类型走内容 helper。不存在源不会先删目标。该检查不解析物理别名、大小写同物或并发变化，不保证源可读、目标保留、复制原子性、close 成功、权限/时间戳复制或持久化。失败可能留下已删旧目标、父目录或部分树。两参数只在本次调用期间借用，不保存地址、不接管调用方内存；并发操作须由调用方协调，当前接口没有路径锁。
 *
 * ## References
 * zr_vm_library/src/zr_vm_library/file_copy.c:166
 * zr_vm_library/src/zr_vm_library/file_copy.c:170
 * zr_vm_library/src/zr_vm_library/file_copy.c:175
 * zr_vm_library/src/zr_vm_library/file_copy.c:180
 * zr_vm_library/src/zr_vm_library/file_copy.c:183
 * zr_vm_library/src/zr_vm_library/file_copy.c:187
 * zr_vm_library/src/zr_vm_library/file_copy.c:231
 * zr_vm_library/include/zr_vm_library/file.h:125
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:250
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:327
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:157
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:180
 * tests/library/test_file_path_operations.c:82
 * tests/library/test_file_path_operations.c:99
 * tests/library/test_file_path_operations.c:109
 * tests/library/test_file_path_operations.c:112
 * zr_vm_library/src/zr_vm_library/file_copy.c:161
 */

/**
 * @fn TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite)
 * @brief 在源与危险词法重叠前置校验后优先 rename；任何 rename 失败再尝试复制并删除源，为 System.Fs 移动包装提供宿主操作。
 *
 * ## Summary
 * 在源与危险词法重叠前置校验后优先 rename；任何 rename 失败再尝试复制并删除源，为 System.Fs 移动包装提供宿主操作。
 * 需要迁移实际文件/目录时使用；移动后原路径与 VM 快照的刷新由上层负责。
 *
 * ## Parameters
 * @param[in] sourcePath 调用期间稳定的 NUL 结尾源路径，仅读取借用；NULL 或规范化/存在性失败在覆盖删除前返回。
 * @param[in] targetPath 调用期间稳定的 NUL 结尾目标路径，仅读取借用；覆盖路径可能被递归删除。
 * @param[in] overwrite 不同目标已存在时，假返回 EEXIST，真在 rename 之前删除；同规范化文本的现存源两种值均成功空操作。
 *
 * ## Returns
 * @return 存在源的同文本空操作、rename 成功，或 Copy(TRUE) 与随后 Delete(source,TRUE) 都成功时返回真；其它情况为假。Copy 返回假时 && 短路跳过末尾源删除，Copy 真但删源失败时仍为假。
 *
 * ## Constraints and lifecycle
 * 源存在性及组件关系校验先于目标删除，已修复的缺源/同文本/祖先路径旧失败顺序不得再作为当前事实。rename 的任意失败原因都进入回退，并非仅跨卷错误；没有目标恢复事务。覆盖删除失败可已部分改树；复制失败可留部分目标；复制成功而删源失败可留两边内容或部分源树。复制 helper 忽略 close 错误，因此回退成功不证明每次关闭或持久化。参数仅同步借用，不保存、无 VM/GC 所有权转移；外部并发、物理别名和链接环不由词法门禁保护。
 *
 * ## References
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
 * zr_vm_library/include/zr_vm_library/file.h:133
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:273
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:346
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:160
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c:183
 * tests/library/test_file_path_operations.c:82
 * tests/library/test_file_path_operations.c:91
 * tests/library/test_file_path_operations.c:105
 * tests/library/test_file_path_operations.c:115
 * zr_vm_library/src/zr_vm_library/file_copy.c:192
 */
