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
