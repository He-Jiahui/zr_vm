/* Real host-file APIs. The caller registers and later cleans each fixture root. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zr_vm_library/file.h"

static int failures;
static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

static int join(const char *root, const char *leaf, char *out) {
    int count = snprintf(out, ZR_LIBRARY_MAX_PATH_LENGTH, "%s/%s", root, leaf);
    return count >= 0 && (size_t)count < ZR_LIBRARY_MAX_PATH_LENGTH;
}

static int write_file(char *path, const char *text) {
    FILE *stream = fopen(path, "wb");
    size_t length = strlen(text);
    int result;
    if (stream == NULL) return 0;
    result = fwrite(text, 1, length, stream) == length;
    if (fclose(stream) != 0) result = 0;
    return result;
}

static int matches(char *path, const char *text) {
    char buffer[64];
    FILE *stream = fopen(path, "rb");
    size_t count;
    int result;
    if (stream == NULL) return 0;
    count = fread(buffer, 1, sizeof(buffer), stream);
    result = !ferror(stream) && count == strlen(text) && memcmp(buffer, text, count) == 0;
    if (fclose(stream) != 0) result = 0;
    return result;
}

static void path_case(const char *name) {
    char output[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (strcmp(name, "join_overflow") == 0) {
        char left[ZR_LIBRARY_MAX_PATH_LENGTH], right[ZR_LIBRARY_MAX_PATH_LENGTH];
        memset(left, 'a', sizeof(left) - 1); left[sizeof(left) - 1] = '\0';
        memset(right, 'b', sizeof(right) - 1); right[sizeof(right) - 1] = '\0';
        strcpy(output, "sentinel"); errno = 0;
        ZrLibrary_File_PathJoin(left, right, output);
        check(output[0] == '\0' && errno == ENAMETOOLONG,
              "join rejects truncation and clears its void output");
    } else if (strcmp(name, "join_control") == 0) {
        ZrLibrary_File_PathJoin("alpha", "beta", output);
        check(strcmp(output, "alpha/beta") == 0 || strcmp(output, "alpha\\beta") == 0,
              "ordinary path join is complete");
    }
#ifdef _WIN32
    else if (strcmp(name, "unc_root") == 0) {
        check(ZrLibrary_File_NormalizePath("\\\\server\\share\\folder\\..\\leaf", output, sizeof(output)),
              "UNC path normalizes without host filesystem access");
        check(strcmp(output, "\\\\server\\share\\leaf") == 0, "UNC server/share root keeps two separators");
    } else if (strcmp(name, "drive_relative") == 0) {
        check(!ZrLibrary_File_IsAbsolutePath("E:relative"), "drive-relative path is not absolute");
        check(!ZrLibrary_File_NormalizePath("E:relative", output, sizeof(output)),
              "unsupported drive-relative path is rejected rather than changed to a root path");
    }
#endif
}

static void filesystem_case(const char *name, char *root) {
    char source[ZR_LIBRARY_MAX_PATH_LENGTH], target[ZR_LIBRARY_MAX_PATH_LENGTH];
    char sourceFile[ZR_LIBRARY_MAX_PATH_LENGTH], siblingFile[ZR_LIBRARY_MAX_PATH_LENGTH];
    int success;
    check(ZrLibrary_File_Exist(root) == ZR_LIBRARY_FILE_NOT_EXIST, "registered fixture root is new");
    if (ZrLibrary_File_Exist(root) != ZR_LIBRARY_FILE_NOT_EXIST) return;
    check(ZrLibrary_File_CreateDirectories(root), "create only the registered fixture root");
    if (strcmp(name, "copy_ancestor") == 0 || strcmp(name, "move_ancestor") == 0) {
        if (!join(root, "ancestor/source", source) || !join(root, "ancestor", target) ||
            !join(source, "payload.txt", sourceFile) || !join(target, "sibling.txt", siblingFile)) return;
        check(ZrLibrary_File_CreateDirectories(source), "create source directory under target ancestor");
        check(write_file(sourceFile, "source payload") && write_file(siblingFile, "sibling payload"), "create owned source and sibling files");
        success = strcmp(name, "copy_ancestor") == 0
                ? ZrLibrary_File_Copy(source, target, ZR_TRUE) : ZrLibrary_File_Move(source, target, ZR_TRUE);
        check(!success, "overlapping ancestor target is rejected");
        check(matches(sourceFile, "source payload"), "ancestor rejection preserves the source");
        check(matches(siblingFile, "sibling payload"), "ancestor rejection preserves sibling data");
        return;
    }
    if (!join(root, "source.txt", source) || !join(root, "target.txt", target)) return;
    if (strcmp(name, "move_missing_source") == 0) {
        check(write_file(target, "target payload"), "create an existing overwrite target");
        check(!ZrLibrary_File_Move(source, target, ZR_TRUE), "missing source move fails");
        check(matches(target, "target payload"), "missing source does not delete the existing target");
        return;
    }
    if (strcmp(name, "copy_component_prefix_control") == 0) {
        if (!join(root, "source", source) || !join(root, "source-other", target) ||
            !join(source, "payload.txt", sourceFile) || !join(target, "payload.txt", siblingFile)) return;
        check(ZrLibrary_File_CreateDirectories(source) && write_file(sourceFile, "source payload"), "create source with component-prefix sibling");
        check(ZrLibrary_File_Copy(source, target, ZR_FALSE), "component-prefix sibling remains a valid copy target");
        check(matches(sourceFile, "source payload") && matches(siblingFile, "source payload"), "sibling copy preserves complete data");
        return;
    }
    check(write_file(source, "source payload"), "create owned source data");
    if (strcmp(name, "move_self") == 0 || strcmp(name, "move_self_no_overwrite") == 0) {
        success = ZrLibrary_File_Move(source, source, strcmp(name, "move_self") == 0);
        check(success, "same normalized source and target move is a successful no-op");
        check(matches(source, "source payload"), "same-path move preserves source contents");
    } else if (strcmp(name, "copy_self_control") == 0) {
        check(ZrLibrary_File_Copy(source, source, ZR_TRUE), "same-path copy remains a successful no-op");
        check(matches(source, "source payload"), "same-path copy preserves source contents");
    } else if (strcmp(name, "copy_sibling_control") == 0) {
        check(ZrLibrary_File_Copy(source, target, ZR_FALSE), "ordinary copy succeeds");
        check(matches(source, "source payload") && matches(target, "source payload"), "ordinary copy preserves both complete files");
    } else if (strcmp(name, "move_sibling_control") == 0) {
        check(ZrLibrary_File_Move(source, target, ZR_FALSE), "ordinary move succeeds");
        check(ZrLibrary_File_Exist(source) == ZR_LIBRARY_FILE_NOT_EXIST && matches(target, "source payload"), "ordinary move publishes the complete target");
    }
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    if (strncmp(argv[1], "join_", 5) == 0 || strcmp(argv[1], "unc_root") == 0 || strcmp(argv[1], "drive_relative") == 0) path_case(argv[1]);
    else filesystem_case(argv[1], argv[2]);
    return failures ? 1 : 0;
}

/**
 * @var static int failures
 * @brief 为本测试进程累计 check 的失败数，并供 main 的最终退出码使用。
 *
 * ## Summary
 * 为本测试进程累计 check 的失败数，并供 main 的最终退出码使用。
 * 静态存储在进程启动时零初始化，注册场景通常各启动独立进程。
 *
 * ## Constraints and lifecycle
 * 仅 check 在条件为假时递增，main 读值；没有显式重置、原子操作或并发保护。当前单线程、一次 main 的协议不把它当可并发或可重入测试框架。状态不拥有路径或文件，进程结束不负责回收宿主夹具。
 *
 * ## References
 * tests/library/test_file_path_operations.c:8
 * tests/library/test_file_path_operations.c:11
 * tests/library/test_file_path_operations.c:124
 * tests/cmake/file-path-operations-tests.cmake:29
 * tests/cmake/file-path-operations-tests.cmake:30
 */

/**
 * @fn static void check(int condition, const char *message)
 * @brief 输出单条 PASS/FAIL 文本并累计失败，保留当前场景后续检查的执行机会。
 *
 * ## Summary
 * 输出单条 PASS/FAIL 文本并累计失败，保留当前场景后续检查的执行机会。
 * 由两个场景分派 helper 调用；它是普通 C 记账函数，不是 Unity 或 assert。
 *
 * ## Parameters
 * @param[in] condition 已由调用方求值的整数条件，非零记 PASS，零记 FAIL。
 * @param[in] message 本次 fprintf 期间有效且 NUL 结尾的说明文本，只借用不保存。
 *
 * ## Returns
 * 无返回值；每次尝试向 stderr 写一行，假条件使 failures 加一，随后正常返回。
 *
 * ## Constraints and lifecycle
 * 不 longjmp、不 exit/abort，不中断调用方流程，也不检查 fprintf 成功。调用表达式内部的 &&/|| 仍按 C 规则短路；后续检查可能观察到前置创建失败后的状态。仅单线程使用 failures，不负责句柄、内存或夹具清理。
 *
 * ## References
 * tests/library/test_file_path_operations.c:10
 * tests/library/test_file_path_operations.c:11
 * tests/library/test_file_path_operations.c:49
 * tests/library/test_file_path_operations.c:80
 * tests/library/test_file_path_operations.c:83
 * tests/library/test_file_path_operations.c:116
 * tests/library/test_file_path_operations.c:124
 * tests/library/test_file_path_operations.c:9
 */

/**
 * @fn static int join(const char *root, const char *leaf, char *out)
 * @brief 为测试夹具组合根路径和相对叶名，用显式格式化返回值阻止继续使用截断路径。
 *
 * ## Summary
 * 为测试夹具组合根路径和相对叶名，用显式格式化返回值阻止继续使用截断路径。
 * filesystem_case 在访问当前场景的实际文件前调用。
 *
 * ## Parameters
 * @param[in] root 有效 NUL 结尾的夹具根文本，只借用至返回。
 * @param[in] leaf 有效 NUL 结尾的相对子项文本，当前调用提供固定测试字符串。
 * @param[out] out 至少 ZR_LIBRARY_MAX_PATH_LENGTH 字节的可写独立缓冲区，不与输入重叠。
 *
 * ## Returns
 * @return snprintf 返回非负且小于固定容量时为真，否则为假；失败可能已写入截断文本，本 helper 不清空它。
 *
 * ## Constraints and lifecycle
 * 总是使用 / 拼接，不访问文件系统、不分配或保存指针。调用方用 || 短路检测失败并直接返回；这些早退自身不增加 failures，因此不能把 main 的零退出概括为每个分支都运行。
 *
 * ## References
 * tests/library/test_file_path_operations.c:15
 * tests/library/test_file_path_operations.c:16
 * tests/library/test_file_path_operations.c:77
 * tests/library/test_file_path_operations.c:78
 * tests/library/test_file_path_operations.c:88
 * tests/library/test_file_path_operations.c:96
 * tests/library/test_file_path_operations.c:97
 * tests/library/test_file_path_operations.c:14
 */

/**
 * @fn static int write_file(char *path, const char *text)
 * @brief 为当前宿主文件场景创建或截断二进制文本夹具，并将写入长度和关闭结果合并为布尔判据。
 *
 * ## Summary
 * 为当前宿主文件场景创建或截断二进制文本夹具，并将写入长度和关闭结果合并为布尔判据。
 * 只有外部已登记的测试根内路径可作为正常测试输入，普通调用不自动注册路径。
 *
 * ## Parameters
 * @param[in] path fopen 期间有效的 NUL 结尾宿主路径，wb 可创建或截断目标。
 * @param[in] text 有效 NUL 结尾文本，借用至长度计算和 fwrite 完成，当前输入为固定短 payload。
 *
 * ## Returns
 * @return 打开、完整 fwrite 和 fclose 均成功时返回真；打开失败立即假，已打开流的写入或关闭失败返回假。
 *
 * ## Constraints and lifecycle
 * 本函数在 fopen 后读取 strlen(text)，所以参数有效性是调用前提而非 NULL 诊断。已取得 FILE* 在普通完成路径始终 fclose 一次，不向调用者转移；失败不撤销已建文件/部分内容。内部无 Unity 跳转，调用方 check 接收该结果。
 *
 * ## References
 * tests/library/test_file_path_operations.c:20
 * tests/library/test_file_path_operations.c:21
 * tests/library/test_file_path_operations.c:23
 * tests/library/test_file_path_operations.c:24
 * tests/library/test_file_path_operations.c:25
 * tests/library/test_file_path_operations.c:80
 * tests/library/test_file_path_operations.c:90
 * tests/library/test_file_path_operations.c:98
 * tests/library/test_file_path_operations.c:103
 * tests/library/test_file_path_operations.c:19
 */

/**
 * @fn static int matches(char *path, const char *text)
 * @brief 用至多 64 字节的宿主读取结果核对本组短 payload，给 copy/move 后的保留和目标内容检查提供判据。
 *
 * ## Summary
 * 用至多 64 字节的宿主读取结果核对本组短 payload，给 copy/move 后的保留和目标内容检查提供判据。
 * 当前固定 payload 短于缓冲区；不能把此 helper 当任意长度的完整文件比较器。
 *
 * ## Parameters
 * @param[in] path 读取期间有效的 NUL 结尾文件路径，只借用。
 * @param[in] text 读取及比较期间有效的 NUL 结尾预期文本；当前用例为短字符串。
 *
 * ## Returns
 * @return 打开成功、一次 fread 无错误、读取数等于预期长度、对应字节相等且 fclose 成功时为真，否则假。
 *
 * ## Constraints and lifecycle
 * 最多读取 64 字节，不追加 NUL，不进行第二次 EOF 检查；预期长度等于 64 时不能排除文件还有后续字节，更长预期也不能成功匹配。长度相等后才 memcmp，逻辑 && 短路。打开后普通完成路径 fclose 一次；不分配或交付流，不删除路径。
 *
 * ## References
 * tests/library/test_file_path_operations.c:30
 * tests/library/test_file_path_operations.c:31
 * tests/library/test_file_path_operations.c:35
 * tests/library/test_file_path_operations.c:36
 * tests/library/test_file_path_operations.c:37
 * tests/library/test_file_path_operations.c:84
 * tests/library/test_file_path_operations.c:92
 * tests/library/test_file_path_operations.c:100
 * tests/library/test_file_path_operations.c:116
 * tests/library/test_file_path_operations.c:29
 */

/**
 * @fn static void path_case(const char *name)
 * @brief 分派不需要创建实际文件的路径字符串场景，核 join 截断/普通结果以及 Windows 路径语法。
 *
 * ## Summary
 * 分派不需要创建实际文件的路径字符串场景，核 join 截断/普通结果以及 Windows 路径语法。
 * main 对 join_ 前缀或 Windows 两个注册名称分派到此函数；未识别名称不自动生成失败。
 *
 * ## Parameters
 * @param[in] name 调用期间有效的 NUL 结尾场景名；注册名为 join_overflow、join_control，以及 _WIN32 下的 unc_root、drive_relative。
 *
 * ## Returns
 * 无返回值；相关 check 更新进程 failures 并输出判据，返回 main 后由计数决定退出码。
 *
 * ## Constraints and lifecycle
 * join_overflow 检查 void PathJoin 失败后首字节清零和 ENAMETOOLONG；join_control 接受斜线或反斜线完整结果。Windows UNC 仅检验规范化文本及不越过 server/share 根，盘符相对输入只检验 IsAbsolute/Normalize 拒绝；不访问 UNC 网络、不验证物理文件存在。_WIN32 分支未编入时没有这两项检查；局部数组只活到本调用返回。
 *
 * ## References
 * tests/library/test_file_path_operations.c:43
 * tests/library/test_file_path_operations.c:48
 * tests/library/test_file_path_operations.c:49
 * tests/library/test_file_path_operations.c:52
 * tests/library/test_file_path_operations.c:53
 * tests/library/test_file_path_operations.c:56
 * tests/library/test_file_path_operations.c:58
 * tests/library/test_file_path_operations.c:60
 * tests/library/test_file_path_operations.c:62
 * tests/library/test_file_path_operations.c:63
 * tests/library/test_file_path_operations.c:122
 * tests/cmake/file-path-operations-tests.cmake:23
 * tests/cmake/file-path-operations-tests.cmake:25
 * tests/library/test_file_path_operations.c:41
 */

/**
 * @fn static void filesystem_case(const char *name, char *root)
 * @brief 在外部登记并负责回收的独立根内构造实际宿主文件，分派九个 Copy/Move 场景并记账有限判据。
 *
 * ## Summary
 * 在外部登记并负责回收的独立根内构造实际宿主文件，分派九个 Copy/Move 场景并记账有限判据。
 * main 对非纯路径场景名调用；每个进程要求根此前不存在，不自行生成唯一根或执行最终清理。
 *
 * ## Parameters
 * @param[in] name 本次调用期间有效的注册场景名；未知名称不构成完整有效测试协议。
 * @param[in] root 有效 NUL 结尾的专用夹具根路径，仅借用；必须由外部登记、确认归属并在运行后按明确指纹回收。
 *
 * ## Returns
 * 无返回值；check 累计失败后继续，已存在根会先记失败再返回，join 失败的直接早退本身不记失败。
 *
 * ## Constraints and lifecycle
 * 创建根与子项、覆盖/移动均有真实磁盘副作用，函数未删除夹具。祖先目标两例核拒绝且短源/兄弟内容仍匹配；缺源 Move 核原目标内容；同路径 Move 两种 overwrite 和 Copy(true) 核成功/源保留；普通复制/移动核有限存在性与短内容；source-other 对照核组件边界。&& 会跳过后一写入或匹配；准备 check 失败不会阻止后续 API。没有目录后代拒绝、rename 失败回退、强制短写/OOM/close、物理别名、并发和权限的专项断言。
 *
 * ## References
 * tests/library/test_file_path_operations.c:73
 * tests/library/test_file_path_operations.c:74
 * tests/library/test_file_path_operations.c:75
 * tests/library/test_file_path_operations.c:77
 * tests/library/test_file_path_operations.c:80
 * tests/library/test_file_path_operations.c:81
 * tests/library/test_file_path_operations.c:83
 * tests/library/test_file_path_operations.c:84
 * tests/library/test_file_path_operations.c:85
 * tests/library/test_file_path_operations.c:88
 * tests/library/test_file_path_operations.c:90
 * tests/library/test_file_path_operations.c:91
 * tests/library/test_file_path_operations.c:92
 * tests/library/test_file_path_operations.c:99
 * tests/library/test_file_path_operations.c:103
 * tests/library/test_file_path_operations.c:105
 * tests/library/test_file_path_operations.c:109
 * tests/library/test_file_path_operations.c:112
 * tests/library/test_file_path_operations.c:115
 * tests/library/test_file_path_operations.c:116
 * tests/library/test_file_path_operations.c:123
 * tests/cmake/file-path-operations-tests.cmake:21
 * tests/cmake/file-path-operations-tests.cmake:30
 * tests/library/test_file_path_operations.c:69
 */

/**
 * @fn int main(int argc, char **argv)
 * @brief 作为 CMake 注册测试的普通 C 进程入口，消费场景名和夹具根，完成一次场景后用累计失败数交付退出状态。
 *
 * ## Summary
 * 作为 CMake 注册测试的普通 C 进程入口，消费场景名和夹具根，完成一次场景后用累计失败数交付退出状态。
 * 要求两个命令行参数；外层 CTest/驱动负责注册、超时、文件归属和清理。
 *
 * ## Parameters
 * @param[in] argc 必须为 3，包含程序名、场景名和根路径；其它值直接返回 2。
 * @param[in] argv argc==3 时 argv[1]/argv[2] 为有效 NUL 结尾文本，借用至 main 返回；路径字符串场景不使用根创建文件。
 *
 * ## Returns
 * @return 参数数量错误为 2；场景返回后 failures 非零为 1，否则为 0。零退出不证明未知名称或 join 早退实际覆盖所有断言。
 *
 * ## Constraints and lifecycle
 * join_ 前缀或 unc_root/drive_relative 走 path_case，其余走 filesystem_case；Windows 场景编译条件在子 helper，注册条件在 CMake WIN32。普通 check 没有 Unity/assert 异常控制流，stderr 的 PASS 文本是每条现场判据而非全套注册运行证明。当前 CMake 为 11 个可移植名称、Windows 再加 2 个；各项独立根和 TIMEOUT=30 由外层提供，本程序不自动清理或复位 failures。
 *
 * ## References
 * tests/library/test_file_path_operations.c:121
 * tests/library/test_file_path_operations.c:122
 * tests/library/test_file_path_operations.c:123
 * tests/library/test_file_path_operations.c:124
 * tests/cmake/file-path-operations-tests.cmake:7
 * tests/cmake/file-path-operations-tests.cmake:23
 * tests/cmake/file-path-operations-tests.cmake:25
 * tests/cmake/file-path-operations-tests.cmake:29
 * tests/cmake/file-path-operations-tests.cmake:30
 * tests/cmake/file-path-operations-tests.cmake:31
 * tests/library/test_file_path_operations.c:120
 */
