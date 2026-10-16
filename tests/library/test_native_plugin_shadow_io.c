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
