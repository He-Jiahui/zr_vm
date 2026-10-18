/* Real filesystem/stdio failures; production copy code is linked from Library. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#include <fcntl.h>
#include <io.h>
#else
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "native_binding/native_binding_internal.h"
#include "native_binding/native_binding_plugin_shadow_io.h"
#include "zr_vm_core/global.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s (errno=%d)\n", __LINE__, #condition, errno); \
        return 1; \
    } \
} while (0)

static TZrPtr test_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                            TZrSize newSize, TZrInt64 flag) {
    (void)data;
    (void)oldSize;
    (void)flag;
    if (newSize == 0) {
        free(old);
        return ZR_NULL;
    }
    return realloc(old, newSize);
}

/* Closing the reader guarantees a real flush error without disk quotas or hooks. */
static FILE *broken_pipe_writer(void) {
    int descriptor;
    FILE *stream;
#ifdef _WIN32
    HANDLE reader, writer;
    if (!CreatePipe(&reader, &writer, ZR_NULL, 0)) return ZR_NULL;
    CloseHandle(reader);
    descriptor = _open_osfhandle((intptr_t)writer, _O_BINARY);
    if (descriptor == -1) {
        CloseHandle(writer);
        return ZR_NULL;
    }
    stream = _fdopen(descriptor, "wb");
    if (stream == ZR_NULL) _close(descriptor);
#else
    int descriptors[2];
    if (pipe(descriptors) != 0) return ZR_NULL;
    close(descriptors[0]);
    descriptor = descriptors[1];
    stream = fdopen(descriptor, "wb");
    if (stream == ZR_NULL) close(descriptor);
#endif
    return stream;
}

static int test_target_close_failure(const char *workspace) {
    static const char payload[] = "small buffered plugin payload";
    char buffer[4096];
    char sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    FILE *source;
    FILE *target;
    TZrBool copied;
    int length;

    /* Establish that fwrite succeeds and fclose itself really fails here. */
    target = broken_pipe_writer();
    CHECK(target != ZR_NULL);
    CHECK(setvbuf(target, buffer, _IOFBF, sizeof(buffer)) == 0);
    CHECK(fwrite(payload, 1, sizeof(payload) - 1, target) == sizeof(payload) - 1);
    CHECK(fclose(target) == EOF);
    fprintf(stderr, "preflight: buffered write succeeded, broken-pipe fclose returned EOF\n");

    length = snprintf(sourcePath, sizeof(sourcePath), "%s/close-source.bin", workspace);
    CHECK(length > 0 && (size_t)length < sizeof(sourcePath));
    source = fopen(sourcePath, "wb");
    CHECK(source != ZR_NULL);
    CHECK(fwrite(payload, 1, sizeof(payload) - 1, source) == sizeof(payload) - 1);
    CHECK(fclose(source) == 0);
    source = fopen(sourcePath, "rb");
    CHECK(source != ZR_NULL);
    target = broken_pipe_writer();
    if (target == ZR_NULL) {
        fclose(source);
        remove(sourcePath);
        return 1;
    }
    CHECK(setvbuf(target, buffer, _IOFBF, sizeof(buffer)) == 0);
    copied = native_registry_copy_plugin_streams(source, target);
    CHECK(remove(sourcePath) == 0);
    CHECK(!copied);
    return 0;
}

/* Dedicated per-case directory; no other session's cache entries are touched. */
static int shadow_file_count(const char *directory, size_t *count) {
    *count = 0;
#ifdef _WIN32
    char pattern[ZR_LIBRARY_MAX_PATH_LENGTH];
    WIN32_FIND_DATAA entry;
    HANDLE search;
    int length = snprintf(pattern, sizeof(pattern), "%s/zrvm_shadow_*", directory);
    if (length <= 0 || (size_t)length >= sizeof(pattern)) return 0;
    search = FindFirstFileA(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++*count;
    } while (FindNextFileA(search, &entry));
    {
        DWORD error = GetLastError();
        FindClose(search);
        return error == ERROR_NO_MORE_FILES;
    }
#else
    struct dirent *entry;
    DIR *directoryStream = opendir(directory);
    if (directoryStream == ZR_NULL) return 0;
    errno = 0;
    while ((entry = readdir(directoryStream)) != ZR_NULL) {
        if (strncmp(entry->d_name, "zrvm_shadow_", 12) == 0) ++*count;
    }
    {
        int error = errno;
        closedir(directoryStream);
        return error == 0;
    }
#endif
}

static int test_open_failure_removes_shadow(const char *workspace) {
    static const char invalidImage[] = "This is deliberately not a PE/ELF dynamic library.\n";
    char sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    char cachePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    char observed[sizeof(invalidImage)] = {0};
    SZrGlobalState *global;
    ZrLibrary_NativeRegistryState *registry;
    FILE *file;
    size_t before, after;
    TZrSize modules, handles;
    int length, failed = 0;

    length = snprintf(sourcePath, sizeof(sourcePath), "%s/invalid-plugin.bin", workspace);
    CHECK(length > 0 && (size_t)length < sizeof(sourcePath));
    length = snprintf(cachePath, sizeof(cachePath), "%s/zr_vm_native_plugin_cache", workspace);
    CHECK(length > 0 && (size_t)length < sizeof(cachePath));
    CHECK(shadow_file_count(cachePath, &before));
    file = fopen(sourcePath, "wb");
    CHECK(file != ZR_NULL);
    CHECK(fwrite(invalidImage, 1, sizeof(invalidImage) - 1, file) == sizeof(invalidImage) - 1);
    CHECK(fclose(file) == 0);
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 1, ZR_NULL);
    CHECK(global != ZR_NULL);
    CHECK(ZrLibrary_NativeRegistry_Attach(global));
    registry = native_registry_get(global);
    modules = registry->moduleRecords.length;
    handles = registry->pluginHandles.length;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (native_registry_load_plugin_descriptor(global->mainThreadState, registry,
                                                   sourcePath, "zr.shadow.invalid") != ZR_NULL ||
            registry->moduleRecords.length != modules || registry->pluginHandles.length != handles ||
            ZrLibrary_NativeRegistry_FindModule(global, "zr.shadow.invalid") != ZR_NULL) {
            failed = 1;
        }
    }
    if (!shadow_file_count(cachePath, &after)) {
        failed = 1;
    } else {
        fprintf(stderr, "shadow entries before=%zu after=%zu\n", before, after);
        if (after != before) failed = 1;
    }
    file = fopen(sourcePath, "rb");
    if (file == ZR_NULL) {
        failed = 1;
    } else {
        if (fread(observed, 1, sizeof(invalidImage) - 1, file) != sizeof(invalidImage) - 1 ||
            memcmp(observed, invalidImage, sizeof(invalidImage) - 1) != 0) failed = 1;
        if (fclose(file) != 0) failed = 1;
    }
    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    if (remove(sourcePath) != 0) failed = 1;
    return failed;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    if (argc != 3) return 2;
    if (strcmp(argv[1], "open_failure") == 0) return test_open_failure_removes_shadow(argv[2]);
    if (strcmp(argv[1], "target_close_failure") == 0) return test_target_close_failure(argv[2]);
    return 2;
}

/**
 * @def CHECK(condition)
 *
 * ## Summary
 * 在普通 int 返回值测试 helper 内检查准备条件或有限期望，记录行号/表达式/errno 后把当前 helper 直接返回为失败。
 *
 * ## Parameters
 * @param[in] condition 可用作 C 条件的表达式，实际只求值一次；表达式字符串仅用于诊断。
 *
 * ## Returns
 * 宏没有表达式返回值；条件为真继续当前函数，条件为假输出诊断并从当前 int 函数 return 1。
 *
 * ## Constraints and lifecycle
 * 这是普通 main/helper 协议，不是 Unity/CMocka 断言、异常恢复或清理块。早期返回会跳过之后的 fclose/remove/registry/global 释放；调用点必须只在能理解此失败边界的 int 函数内使用。errno 只反映诊断当时值，成功不保证清零。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:25
 * - tests/library/test_native_plugin_shadow_io.c:27
 * - tests/library/test_native_plugin_shadow_io.c:28
 * - tests/library/test_native_plugin_shadow_io.c:81
 * - tests/library/test_native_plugin_shadow_io.c:101
 * - tests/library/test_native_plugin_shadow_io.c:165
 */

/**
 * @fn static TZrPtr test_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize, TZrSize newSize, TZrInt64 flag)
 *
 * ## Summary
 * 给无效插件装载夹具提供与 Core global 兼容的 CRT 分配回调，供 GlobalState 生命周期间接调用。
 *
 * ## Parameters
 * @param[in] data 回调上下文，本夹具忽略，不拥有其资源。
 * @param[in,out] old NULL 或与 CRT realloc/free 兼容的旧块，newSize 为零时消费并释放。
 * @param[in] oldSize 本夹具忽略的旧尺寸描述；不据此检查内存来源。
 * @param[in] newSize 零请求释放，非零请求 realloc 所需字节。
 * @param[in] flag Core 传入分类标记，本夹具忽略。
 *
 * ## Returns
 * @return 释放请求返回 NULL；其他请求返回 realloc 的结果。非零请求失败时 NULL 不释放旧块，仍由上层持有。
 *
 * ## Constraints and lifecycle
 * 不注入 OOM、统计 live bytes 或证明所有内存来源兼容；调用方须遵守 Core allocator 协议。回调借入的块只在本次调用中操作，全局对象正常释放时由上层继续回调。测试 CHECK 早退不保证执行最终 GlobalState_Free。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:32
 * - tests/library/test_native_plugin_shadow_io.c:37
 * - tests/library/test_native_plugin_shadow_io.c:41
 * - tests/library/test_native_plugin_shadow_io.c:164
 * - tests/library/test_native_plugin_shadow_io.c:193
 */

/**
 * @fn static FILE *broken_pipe_writer(void)
 *
 * ## Summary
 * 制造读端已关闭的可写管道流，使小块缓冲写可以成功而后续刷新/关闭报告真实 OS I/O 失败。
 *
 * ## Parameters
 * 无调用参数。
 *
 * ## Returns
 * @return 成功返回由调用方独占并负责 fclose 的 FILE*；创建、句柄转描述符或 fdopen 失败返回 NULL，并关闭该失败路径已取得的写端句柄/描述符。
 *
 * ## Constraints and lifecycle
 * Windows 依次转移 HANDLE→CRT descriptor→FILE 所有权，失败分别 CloseHandle/_close；POSIX 先关闭读端，fdopen 失败关闭写描述符。CloseHandle/close 清理调用的错误不再单独汇总。POSIX 执行写之前 main 应忽略 SIGPIPE，否则进程可能被信号终止。返回流不是已发生 I/O 错误的证明，场景还要用 fwrite/fclose 的原断言建立失败时机。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:45
 * - tests/library/test_native_plugin_shadow_io.c:48
 * - tests/library/test_native_plugin_shadow_io.c:50
 * - tests/library/test_native_plugin_shadow_io.c:52
 * - tests/library/test_native_plugin_shadow_io.c:57
 * - tests/library/test_native_plugin_shadow_io.c:61
 * - tests/library/test_native_plugin_shadow_io.c:64
 * - tests/library/test_native_plugin_shadow_io.c:67
 * - tests/library/test_native_plugin_shadow_io.c:205
 */

/**
 * @fn static int test_target_close_failure(const char *workspace)
 *
 * ## Summary
 * 先证明真实断管流的小块 fwrite 成功而 fclose 失败，再验证生产 owning-stream helper 将该关闭错误报告为 FALSE。
 *
 * ## Parameters
 * @param[in] workspace 独占、已存在且可写的场景目录；用于固定 close-source.bin，字符串借用至返回。
 *
 * ## Returns
 * @return 原 CHECK 条件与最后 copied==FALSE 断言均满足时返回 0；检查失败或第二次断管流准备失败返回 1。stderr 有刻意的 preflight 诊断，不应要求空流。
 *
 * ## Constraints and lifecycle
 * 使用栈上缓冲区完成两次 setvbuf，只在流关闭之后才允许它离开作用域。第一次断管流由本测试关闭；第二次 source/target 的关闭责任在 helper 调用时转移，返回后不再次 fclose。正常路径移除 source 文件；CHECK 早退可跳过本地关闭/移除，没有 tearDown。只证明此有限 target-close 失败及布尔结果，不直接测源读错误、短写、源关闭失败、目标 remove 或所有设备。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:70
 * - tests/library/test_native_plugin_shadow_io.c:71
 * - tests/library/test_native_plugin_shadow_io.c:80
 * - tests/library/test_native_plugin_shadow_io.c:82
 * - tests/library/test_native_plugin_shadow_io.c:83
 * - tests/library/test_native_plugin_shadow_io.c:84
 * - tests/library/test_native_plugin_shadow_io.c:85
 * - tests/library/test_native_plugin_shadow_io.c:87
 * - tests/library/test_native_plugin_shadow_io.c:95
 * - tests/library/test_native_plugin_shadow_io.c:101
 * - tests/library/test_native_plugin_shadow_io.c:102
 * - tests/library/test_native_plugin_shadow_io.c:103
 * - tests/library/test_native_plugin_shadow_io.c:104
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:69
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:73
 */

/**
 * @fn static int shadow_file_count(const char *directory, size_t *count)
 *
 * ## Summary
 * 观察自有缓存目录中的影子命名前缀条目数量，用于前后比较，不拥有或清理这些条目。
 *
 * ## Parameters
 * @param[in] directory 稳定 NUL 结尾、已准备的独占缓存目录；调用期间不得被并发修改。
 * @param[out] count 非 NULL、独立可写的计数槽；进入时置零，枚举失败时可能保留部分计数。
 *
 * ## Returns
 * @return 枚举协议正常结束返回 1；路径格式、打开或枚举错误返回 0。Windows 的无匹配 ERROR_FILE_NOT_FOUND 是成功的零计数。
 *
 * ## Constraints and lifecycle
 * Windows 用 FindFirst/Next 并排除目录；POSIX 仅按名字前缀计数，包含同前缀目录，两分支不是通用同等文件类型验证。已取得的搜索句柄/目录流会关闭，但关闭返回值未单独验证。结果只是这个瞬时目录观察，不认证内容、注册表状态或不存在外来同名项；调用方使用独占场景目录。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:109
 * - tests/library/test_native_plugin_shadow_io.c:110
 * - tests/library/test_native_plugin_shadow_io.c:115
 * - tests/library/test_native_plugin_shadow_io.c:118
 * - tests/library/test_native_plugin_shadow_io.c:120
 * - tests/library/test_native_plugin_shadow_io.c:124
 * - tests/library/test_native_plugin_shadow_io.c:129
 * - tests/library/test_native_plugin_shadow_io.c:133
 * - tests/library/test_native_plugin_shadow_io.c:137
 * - tests/library/test_native_plugin_shadow_io.c:159
 * - tests/library/test_native_plugin_shadow_io.c:178
 */

/**
 * @fn static int test_open_failure_removes_shadow(const char *workspace)
 *
 * ## Summary
 * 以非动态库的固定文件连续三次走真实 descriptor 装载失败路径，检查影子条目数量、注册表长度和源内容的有限保持。
 *
 * ## Parameters
 * @param[in] workspace 已存在、独占且可写的目录，含预建 zr_vm_native_plugin_cache 子目录；应与平台临时根环境指向同一场景根。
 *
 * ## Returns
 * @return 所有准备 CHECK、三次失败期望、前后计数与源字节比较及最后清理成功时返回 0；任一显式观察失败返回非零。stderr 可输出前后计数。
 *
 * ## Constraints and lifecycle
 * 构造固定非 PE/ELF 文本不等于覆盖成功插件、ABI/导出名/模块身份拒绝、OOM 或每条装载清理分支。单例 global 的 mainThreadState 与 registry 在正常路径按 Registry_Free→GlobalState_Free 释放；CHECK 早退可能跳过这段本地清理。源文件最后移除，缓存目录保留；计数比较要求无并发他人写入且临时根环境一致。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:143
 * - tests/library/test_native_plugin_shadow_io.c:144
 * - tests/library/test_native_plugin_shadow_io.c:155
 * - tests/library/test_native_plugin_shadow_io.c:157
 * - tests/library/test_native_plugin_shadow_io.c:159
 * - tests/library/test_native_plugin_shadow_io.c:164
 * - tests/library/test_native_plugin_shadow_io.c:166
 * - tests/library/test_native_plugin_shadow_io.c:170
 * - tests/library/test_native_plugin_shadow_io.c:171
 * - tests/library/test_native_plugin_shadow_io.c:173
 * - tests/library/test_native_plugin_shadow_io.c:174
 * - tests/library/test_native_plugin_shadow_io.c:178
 * - tests/library/test_native_plugin_shadow_io.c:188
 * - tests/library/test_native_plugin_shadow_io.c:192
 * - tests/library/test_native_plugin_shadow_io.c:193
 * - tests/library/test_native_plugin_shadow_io.c:194
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:227
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:234
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 为两个独立错误场景提供普通进程入口，先设置平台错误报告/信号策略，再按 selector 分派原测试 helper。
 *
 * ## Parameters
 * @param[in] argc 应为 3：程序名、场景选择器和独占 workspace。
 * @param[in] argv 有效 argv 字符串数组；选择器只接受 open_failure 或 target_close_failure，workspace 不在本入口创建。
 *
 * ## Returns
 * @return 返回选中 helper 的 0/1 结果；参数数目错误或未知 selector 返回 2。没有 Unity 汇总、自动 fixture teardown 或每 CHECK runtime 轨迹。
 *
 * ## Constraints and lifecycle
 * Windows 调整 ErrorMode/CRT 诊断至 stderr；POSIX 忽略 SIGPIPE，使断管错误能通过 stdio 返回。目录/临时根由 CMake 或外部运行器准备，父进程承担最终自有夹具清理；进程入口不递归删除缓存。CMake 的 30 秒是外层终止界限，函数没有内部超时或恢复协议，强制终止不保证执行尾部清理。
 *
 * ## References
 * - tests/library/test_native_plugin_shadow_io.c:198
 * - tests/library/test_native_plugin_shadow_io.c:200
 * - tests/library/test_native_plugin_shadow_io.c:202
 * - tests/library/test_native_plugin_shadow_io.c:205
 * - tests/library/test_native_plugin_shadow_io.c:207
 * - tests/library/test_native_plugin_shadow_io.c:208
 * - tests/library/test_native_plugin_shadow_io.c:209
 * - tests/library/test_native_plugin_shadow_io.c:210
 * - tests/cmake/native-plugin-shadow-io-tests.cmake:17
 * - tests/cmake/native-plugin-shadow-io-tests.cmake:19
 * - tests/cmake/native-plugin-shadow-io-tests.cmake:21
 * - tests/cmake/native-plugin-shadow-io-tests.cmake:22
 * - tests/cmake/native-plugin-shadow-io-tests.cmake:24
 */
