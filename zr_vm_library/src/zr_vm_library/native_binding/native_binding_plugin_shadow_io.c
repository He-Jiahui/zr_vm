#include "native_binding/native_binding_internal.h"

#include "native_binding/native_binding_plugin_shadow_io.h"

/* 影子副本只写入已有缓存目录；目录创建失败交由加载器回退原始插件路径。 */
static TZrBool native_registry_ensure_directory_exists(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    if (ZrLibrary_File_Exist((TZrNativeString)path) == ZR_LIBRARY_FILE_IS_DIRECTORY) {
        return ZR_TRUE;
    }

#if defined(ZR_PLATFORM_WIN)
    return CreateDirectoryA(path, ZR_NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

/* 每个进程共用临时缓存目录；实际副本名称另含进程号和递增序号。 */
static TZrBool native_registry_get_plugin_shadow_cache_directory(TZrChar *buffer, TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
#if defined(ZR_PLATFORM_WIN)
    {
        DWORD length = GetTempPathA((DWORD)sizeof(rootPath), rootPath);
        if (length == 0 || length >= sizeof(rootPath)) {
            return ZR_FALSE;
        }
    }
#else
    {
        const TZrChar *tempPath = getenv("TMPDIR");
        if (tempPath == ZR_NULL || tempPath[0] == '\0') {
            tempPath = "/tmp";
        }
        snprintf(rootPath, sizeof(rootPath), "%s", tempPath);
    }
#endif

    ZrLibrary_File_PathJoin(rootPath, "zr_vm_native_plugin_cache", buffer);
    return native_registry_ensure_directory_exists(buffer);
}

/* 接管两个有效流；读写及关闭任一失败均报告失败，始终分别关闭两个流。 */
TZrBool native_registry_copy_plugin_streams(FILE *source, FILE *target) {
    TZrByte buffer[4096];
    size_t readSize;
    TZrBool success = ZR_TRUE;

    while ((readSize = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, readSize, target) != readSize) {
            success = ZR_FALSE;
            break;
        }
    }

    if (ferror(source) || ferror(target)) {
        success = ZR_FALSE;
    }

    if (fclose(source) != 0) {
        success = ZR_FALSE;
    }
    /* 缓冲写可能直到关闭才失败；不能短路，否则另一流可能未被关闭。 */
    if (fclose(target) != 0) {
        success = ZR_FALSE;
    }

    return success;
}

/* 插件热重载先复制可加载映像；中途读写失败不留下可误装载的半成品。 */
static TZrBool native_registry_copy_binary_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    FILE *source;
    FILE *target;
    TZrBool success;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        return ZR_FALSE;
    }

    source = fopen(sourcePath, "rb");
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    target = fopen(targetPath, "wb");
    if (target == ZR_NULL) {
        fclose(source);
        return ZR_FALSE;
    }

    success = native_registry_copy_plugin_streams(source, target);
    if (!success) {
        remove(targetPath);
    }
    return success;
}

/* 影子路径使旧版本仍被进程持有时可同时装入新版本；成功后由 handle record 负责删除。 */
TZrBool native_registry_prepare_shadow_plugin_path(const TZrChar *moduleName,
                                                          const TZrChar *sourcePath,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    static TZrUInt64 shadowCounter = 0;
    TZrChar cacheDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *baseName;
    TZrChar fileName[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *extension;
    TZrUInt64 counter;
    TZrSize baseNameLength;
    unsigned long processId;

    if (moduleName == ZR_NULL || sourcePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !native_registry_get_plugin_shadow_cache_directory(cacheDirectory, sizeof(cacheDirectory))) {
        return ZR_FALSE;
    }

    native_registry_sanitize_module_name(moduleName, sanitizedModuleName, sizeof(sanitizedModuleName));
    baseName = sanitizedModuleName[0] != '\0' ? sanitizedModuleName : "plugin";
    baseNameLength = strlen(baseName);
    if (baseNameLength > 96) {
        baseNameLength = 96;
    }
    extension = native_registry_dynamic_library_extension();
    /* TODO: 序号是进程级静态状态而非原子值；核实多线程并发插件装载是否受外层串行化约束。 */
    counter = ++shadowCounter;
#if defined(ZR_PLATFORM_WIN)
    processId = (unsigned long)GetCurrentProcessId();
#else
    processId = (unsigned long)getpid();
#endif

    snprintf(fileName,
             sizeof(fileName),
             "zrvm_shadow_%.*s_%lu_%llu%s",
             (int)baseNameLength,
             baseName,
             processId,
             (unsigned long long)counter,
             extension);
    ZrLibrary_File_PathJoin(cacheDirectory, fileName, buffer);
    return native_registry_copy_binary_file(sourcePath, buffer);
}

/**
 * @fn static TZrBool native_registry_ensure_directory_exists(const TZrChar *path)
 *
 * ## Summary
 * 为影子缓存路径探测或尝试创建一层目录，并将普通失败交还装载回退路径。
 *
 * ## Parameters
 * @param[in] path 调用期间稳定、NUL 结尾的路径；NULL 或空串被拒绝，不取得字符存储所有权。
 *
 * ## Returns
 * @return 已有目录、目录创建成功，或平台报告 already-exists/EEXIST 时返回 TRUE；其余显式分支返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 不递归创建父目录，也不提供独占性、权限认证或抗链接替换保证。already-exists 分支不再次检查对象类型，TRUE 不是末端目录类型的完整证明。无新 heap 对象或句柄交给调用方；不回滚已创建的目录，errno/GetLastError 可由下层改变。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:6
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:7
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:11
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:16
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:18
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:49
 */

/**
 * @fn static TZrBool native_registry_get_plugin_shadow_cache_directory(TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 在复制映像前确定平台临时根下的共享缓存子路径，并尝试准备这一层目录。
 *
 * ## Parameters
 * @param[out] buffer 有效输出需独立可写且实际至少有 ZR_LIBRARY_MAX_PATH_LENGTH 字节；非空检查之后先写空串。
 * @param[in] bufferSize 只检查是否为零；后续 PathJoin 使用固定 MAX 容量，不能把此值视为通用输出上限。
 *
 * ## Returns
 * @return 临时根取得后，返回目录准备 helper 的布尔结果；空输出、零值参数或 Windows 临时根查询失败返回 FALSE。
 *
 * ## Constraints and lifecycle
 * Windows 使用 GetTempPathA 并检查返回长度；POSIX 借用 TMPDIR，空缺时使用 /tmp，复制到固定数组的 snprintf 截断不被单独检测。调用方需使临时根和组合路径适合固定容量；路径组合不提供不可信目录认证。结果是调用方持有的字符副本，目录被保留，不能由返回值推断缓存独占或递归清理责任。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:23
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:26
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:30
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:33
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:40
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:44
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:48
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:49
 * - zr_vm_library/src/zr_vm_library/file_path.c:468
 */

/**
 * @fn TZrBool native_registry_copy_plugin_streams(FILE *source, FILE *target)
 *
 * ## Summary
 * 接管插件复制使用的两个 stdio 流，把数据传到目标后汇总读、写和两次关闭的普通错误。
 *
 * ## Parameters
 * @param[in,out] source 已打开且有效的可读 FILE；必须与 target 是不同流，调用结束后不再由调用方 fclose 或访问。
 * @param[in,out] target 已打开且有效的可写 FILE，与 source 的数据使用不得冲突；本函数消费它的关闭责任。
 *
 * ## Returns
 * @return 读写及两次 fclose 都未报告错误时返回 TRUE；任一检测到的读、写或关闭错误返回 FALSE。返回后两个流均已各执行一次 fclose，即使失败也不得重用 FILE。
 *
 * ## Constraints and lifecycle
 * 不检查 NULL、同一 FILE、文件身份、读写模式或外部并发访问；这些是调用方前提。短写停止复制，但仍检查流错误并分别关闭。FALSE 不撤销已写字节，函数不拥有路径、不删除目标；路径层负责尝试移除失败副本。检查 stdio 成功不等于 fsync 持久化、可加载映像验证或每种设备错误覆盖。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:53
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:58
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:59
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:65
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:69
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:73
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:101
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:103
 * - tests/library/test_native_plugin_shadow_io.c:79
 * - tests/library/test_native_plugin_shadow_io.c:102
 * - tests/library/test_native_plugin_shadow_io.c:104
 */

/**
 * @fn static TZrBool native_registry_copy_binary_file(const TZrChar *sourcePath, const TZrChar *targetPath)
 *
 * ## Summary
 * 为一次影子装载打开二进制源和目标，将流交给 owning helper，并在复制失败时尝试删除目标。
 *
 * ## Parameters
 * @param[in] sourcePath 有效且稳定的 NUL 结尾路径；NULL 被拒绝；调用方保有字符存储。
 * @param[in] targetPath 有效且稳定的目标路径，须指向调用方独占的、与源不同的文件；wb 可截断已有文件。
 *
 * ## Returns
 * @return 只有成功打开两流且流复制 helper 返回 TRUE 时返回 TRUE；其余分支返回 FALSE。复制 helper 失败时尝试 remove(targetPath)，不保证删除一定成功。
 *
 * ## Constraints and lifecycle
 * 本函数不检测同址文件、硬链接、并发写者或目标预先存在；这些条件须由影子路径调用方保证。目标打开失败只关闭已打开的源；成功打开目标后，两流关闭责任交给 helper。FALSE 不承诺目标完全不存在、原目标字节不变或 errno 特定值；不创建父目录，不接管路径字符串。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:81
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:86
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:90
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:95
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:97
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:101
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:103
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:152
 */

/**
 * @fn TZrBool native_registry_prepare_shadow_plugin_path(const TZrChar *moduleName, const TZrChar *sourcePath, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 在注册器装载映像之前，以模块名、进程号和进程级序号构造缓存文件名并复制源文件；本步骤不发布注册记录。
 *
 * ## Parameters
 * @param[in] moduleName 调用期间稳定的 NUL 结尾模块名；NULL 被拒绝；文件名使用清洗后的前 96 字节，空结果回退 plugin。
 * @param[in] sourcePath 调用期间稳定的源路径，应与输出和影子目标独立；有效映像和模块身份由装载器后续验证。
 * @param[out] buffer 调用方独立、可写且实际至少 MAX 路径容量的存储；返回 FALSE 时不保证为空或保持原内容。
 * @param[in] bufferSize 传入真实容量；当前实现仅拒绝零，组合路径的写入上限来自 PathJoin 的固定 MAX。
 *
 * ## Returns
 * @return 复制 helper 成功返回 TRUE，buffer 指向的字符数组保存影子路径；显式参数、目录或复制失败返回 FALSE。TRUE 仅表示本次复制的 stdio 成功，不表示动态装载或注册成功。
 *
 * ## Constraints and lifecycle
 * 调用者必须串行化对进程共享静态序号的访问，并确保源、目标及缓存目录不被并发改写。目录可被共享，文件名不是独占创建协议；消耗的序号在失败时不回退。成功副本的后续删除由注册器失败路径或 handle record 释放路径承担；仅生成路径不会自动卸载或删除。TODO: 核实多状态并发插件装载的实际外层串行化约束，入口 zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:219、zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:113、zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:136；现有两个 selector 没有线程并发见证。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:109
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:113
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:123
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:128
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:131
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:135
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:136
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:151
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:152
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:219
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:233
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:333
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:359
 */

/**
 * @var TZrUInt64 native_registry_prepare_shadow_plugin_path::shadowCounter
 *
 * ## Summary
 * 为当前进程的影子文件名提供跨调用递增序号，不随 registry/global 实例销毁而重置。
 *
 * ## Value model
 * 函数局部的 TZrUInt64 静态计数初始为零；只参与本函数的文件名构造，不向调用方交付引用。
 *
 * ## Constraints and lifecycle
 * 静态无符号计数先增再用于文件名；失败复制也消耗一个值，未做原子更新、锁、独占文件创建或防回绕保证。调用期间须由外层串行化；TODO: 核查 zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:219 对多个状态的并发进入约束，结合 zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:113、zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:136 与文件名 zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:145；两个既有测试仅串行执行。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:113
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:135
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:136
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:145
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:219
 */
