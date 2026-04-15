#include "project/project.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <direct.h>
#define ZR_CLI_MKDIR(path) _mkdir(path)
#else
#define ZR_CLI_MKDIR(path) mkdir(path, ZR_VM_POSIX_DIRECTORY_CREATE_MODE)
#endif

#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_network/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"

#define ZR_CLI_MANIFEST_FORMAT_VERSION 2U

static TZrPtr zr_cli_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    TZrBool canReleasePointer;

    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    canReleasePointer = pointer != ZR_NULL &&
                        (uintptr_t)pointer >= (uintptr_t)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND;

    if (newSize == 0) {
        if (canReleasePointer) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL || !canReleasePointer) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static TZrChar *zr_cli_strdup(const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *) malloc(length + 1);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void zr_cli_normalize_separators(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static TZrBool zr_cli_make_directory(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    if (ZR_CLI_MKDIR(path) == 0) {
        return ZR_TRUE;
    }

    return errno == EEXIST ? ZR_TRUE : ZR_FALSE;
}

static TZrBool zr_cli_append_manifest_entry(SZrCliIncrementalManifest *manifest, const SZrCliManifestEntry *entry) {
    SZrCliManifestEntry *newEntries;
    TZrSize newCapacity;

    if (manifest == ZR_NULL || entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (manifest->count == manifest->capacity) {
        newCapacity = manifest->capacity == 0 ? ZR_CLI_COLLECTION_INITIAL_CAPACITY
                                              : manifest->capacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
        newEntries = (SZrCliManifestEntry *) realloc(manifest->entries, newCapacity * sizeof(*newEntries));
        if (newEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        manifest->entries = newEntries;
        manifest->capacity = newCapacity;
    }

    manifest->entries[manifest->count] = *entry;
    manifest->count++;
    return ZR_TRUE;
}

static TZrChar *zr_cli_next_line(TZrChar **cursor) {
    TZrChar *line;
    TZrChar *end;

    if (cursor == ZR_NULL || *cursor == ZR_NULL || **cursor == '\0') {
        return ZR_NULL;
    }

    line = *cursor;
    end = strchr(line, '\n');
    if (end != ZR_NULL) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = line + strlen(line);
    }

    while (*line != '\0' && (*line == ' ' || *line == '\t' || *line == '\r')) {
        line++;
    }

    end = line + strlen(line);
    while (end > line && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = '\0';
    }

    return line;
}

static TZrBool zr_cli_manifest_match_key(const TZrChar *line, const TZrChar *key, const TZrChar **value) {
    TZrSize keyLength;

    if (line == ZR_NULL || key == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    keyLength = strlen(key);
    if (strncmp(line, key, keyLength) != 0) {
        return ZR_FALSE;
    }

    if (line[keyLength] == '\0') {
        *value = line + keyLength;
        return ZR_TRUE;
    }

    if (line[keyLength] != ' ') {
        return ZR_FALSE;
    }

    *value = line + keyLength + 1;
    return ZR_TRUE;
}

static TZrBool zr_cli_resolve_output_path(const TZrChar *rootPath,
                                          const TZrChar *moduleName,
                                          const TZrChar *extension,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    TZrSize relativeLength;
    TZrSize extensionLength;
    TZrSize totalLength;
    TZrSize index;
    TZrSize writeIndex = 0;

    if (rootPath == ZR_NULL || moduleName == ZR_NULL || extension == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !ZrCli_Project_NormalizeModuleName(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    for (index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }
    relativePath[writeIndex] = '\0';

    rootLength = strlen(rootPath);
    relativeLength = strlen(relativePath);
    extensionLength = strlen(extension);
    totalLength = rootLength + 1 + relativeLength + extensionLength;
    if (totalLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, rootPath, rootLength);
    buffer[rootLength] = ZR_SEPARATOR;
    memcpy(buffer + rootLength + 1, relativePath, relativeLength);
    memcpy(buffer + rootLength + 1 + relativeLength, extension, extensionLength);
    buffer[totalLength] = '\0';
    return ZR_TRUE;
}

static void zr_cli_sanitize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize cursor = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (moduleName == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; moduleName[index] != '\0' && cursor + 1 < bufferSize; index++) {
        TZrChar current = moduleName[index];
        buffer[cursor++] = (TZrChar)(((current >= 'a' && current <= 'z') ||
                                      (current >= 'A' && current <= 'Z') ||
                                      (current >= '0' && current <= '9'))
                                             ? current
                                             : '_');
    }
    buffer[cursor] = '\0';
}

static const TZrChar *zr_cli_dynamic_library_extension(void) {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

SZrGlobalState *ZrCli_Project_CreateBareGlobal(void) {
    SZrCallbackGlobal callbacks = {0};
    return ZrCore_GlobalState_New(zr_cli_allocator, ZR_NULL, 0x5A525F434C495F42ULL, &callbacks);
}

SZrGlobalState *ZrCli_Project_CreateProjectGlobal(const TZrChar *projectPath) {
    return ZrLibrary_CommonState_CommonGlobalState_New((TZrNativeString)projectPath);
}

TZrBool ZrCli_Project_RegisterStandardModules(SZrGlobalState *global) {
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_ToGlobalState_Register(global->mainThreadState);
    return ZrVmLibMath_Register(global) &&
           ZrVmLibSystem_Register(global) &&
           ZrVmLibNetwork_Register(global) &&
           ZrVmLibContainer_Register(global) &&
           ZrVmLibFfi_Register(global) &&
           ZrCore_TaskRuntime_RegisterBuiltins(global);
}

TZrBool ZrCli_ProjectContext_FromGlobal(SZrCliProjectContext *context,
                                        SZrGlobalState *global,
                                        const TZrChar *projectPath) {
    SZrLibrary_Project *project;
    static const TZrChar manifestFileName[] = ".zr_cli_manifest";
    TZrSize binaryRootLength;
    TZrSize manifestNameLength;
    TZrSize manifestLength;

    if (context == ZR_NULL || global == ZR_NULL || global->userData == ZR_NULL || projectPath == ZR_NULL) {
        return ZR_FALSE;
    }

    project = (SZrLibrary_Project *) global->userData;
    if (project->directory == ZR_NULL || project->source == ZR_NULL || project->binary == ZR_NULL || project->entry == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(context, 0, sizeof(*context));
    snprintf(context->projectPath, sizeof(context->projectPath), "%s", projectPath);
    snprintf(context->projectRoot, sizeof(context->projectRoot), "%s", ZrCore_String_GetNativeString(project->directory));
    ZrLibrary_File_PathJoin(context->projectRoot, ZrCore_String_GetNativeString(project->source), context->sourceRoot);
    ZrLibrary_File_PathJoin(context->projectRoot, ZrCore_String_GetNativeString(project->binary), context->binaryRoot);
    if (!ZrCli_Project_NormalizeModuleName(ZrCore_String_GetNativeString(project->entry),
                                           context->entryModule,
                                           sizeof(context->entryModule))) {
        return ZR_FALSE;
    }
    binaryRootLength = strlen(context->binaryRoot);
    manifestNameLength = sizeof(manifestFileName) - 1;
    manifestLength = binaryRootLength + 1 + manifestNameLength;
    if (manifestLength + 1 > sizeof(context->manifestPath)) {
        return ZR_FALSE;
    }

    memcpy(context->manifestPath, context->binaryRoot, binaryRootLength);
    context->manifestPath[binaryRootLength] = ZR_SEPARATOR;
    memcpy(context->manifestPath + binaryRootLength + 1, manifestFileName, manifestNameLength);
    context->manifestPath[manifestLength] = '\0';
    return ZR_TRUE;
}

TZrBool ZrCli_Project_NormalizeModuleName(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLibrary_Project_NormalizeModuleKey(modulePath, buffer, bufferSize);
}

TZrBool ZrCli_Project_ResolveSourcePath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_cli_resolve_output_path(context->sourceRoot,
                                      moduleName,
                                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                      buffer,
                                      bufferSize);
}

TZrBool ZrCli_Project_ResolveBinaryPath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_cli_resolve_output_path(context->binaryRoot,
                                      moduleName,
                                      ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                      buffer,
                                      bufferSize);
}

TZrBool ZrCli_Project_ResolveIntermediatePath(const SZrCliProjectContext *context,
                                              const TZrChar *moduleName,
                                              TZrChar *buffer,
                                              TZrSize bufferSize) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_cli_resolve_output_path(context->binaryRoot,
                                      moduleName,
                                      ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                      buffer,
                                      bufferSize);
}

TZrBool ZrCli_Project_ResolveAotCSourcePath(const SZrCliProjectContext *context,
                                            const TZrChar *moduleName,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (context == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (snprintf(rootPath, sizeof(rootPath), "%s%c%s%c%s",
                 context->binaryRoot,
                 ZR_SEPARATOR,
                 "aot_c",
                 ZR_SEPARATOR,
                 "src") >= (int)sizeof(rootPath)) {
        return ZR_FALSE;
    }

    return zr_cli_resolve_output_path(rootPath, moduleName, ".c", buffer, bufferSize);
}

TZrBool ZrCli_Project_ResolveAotCLibraryPath(const SZrCliProjectContext *context,
                                             const TZrChar *moduleName,
                                             TZrChar *buffer,
                                             TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sanitizedName[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (context == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (snprintf(rootPath, sizeof(rootPath), "%s%c%s%c%s",
                 context->binaryRoot,
                 ZR_SEPARATOR,
                 "aot_c",
                 ZR_SEPARATOR,
                 "lib") >= (int)sizeof(rootPath)) {
        return ZR_FALSE;
    }

    zr_cli_sanitize_module_name(moduleName, sanitizedName, sizeof(sanitizedName));
    return snprintf(buffer,
                    bufferSize,
                    "%s%c%s%s%s",
                    rootPath,
                    ZR_SEPARATOR,
                    "zrvm_aot_",
                    sanitizedName,
                    zr_cli_dynamic_library_extension()) < (int)bufferSize;
}

TZrBool ZrCli_Project_ResolveAotLlvmIrPath(const SZrCliProjectContext *context,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (context == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (snprintf(rootPath, sizeof(rootPath), "%s%c%s%c%s",
                 context->binaryRoot,
                 ZR_SEPARATOR,
                 "aot_llvm",
                 ZR_SEPARATOR,
                 "ir") >= (int)sizeof(rootPath)) {
        return ZR_FALSE;
    }

    return zr_cli_resolve_output_path(rootPath, moduleName, ".ll", buffer, bufferSize);
}

TZrBool ZrCli_Project_ResolveAotLlvmLibraryPath(const SZrCliProjectContext *context,
                                                const TZrChar *moduleName,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sanitizedName[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (context == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (snprintf(rootPath, sizeof(rootPath), "%s%c%s%c%s",
                 context->binaryRoot,
                 ZR_SEPARATOR,
                 "aot_llvm",
                 ZR_SEPARATOR,
                 "lib") >= (int)sizeof(rootPath)) {
        return ZR_FALSE;
    }

    zr_cli_sanitize_module_name(moduleName, sanitizedName, sizeof(sanitizedName));
    return snprintf(buffer,
                    bufferSize,
                    "%s%c%s%s%s",
                    rootPath,
                    ZR_SEPARATOR,
                    "zrvm_aot_",
                    sanitizedName,
                    zr_cli_dynamic_library_extension()) < (int)bufferSize;
}

TZrBool ZrCli_Project_OpenFileIo(SZrState *state, const TZrChar *path, TZrBool isBinary, SZrIo *io) {
    SZrLibrary_File_Reader *reader;

    if (state == ZR_NULL || state->global == ZR_NULL || path == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)path, isBinary);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = isBinary;
    return ZR_TRUE;
}

TZrBool ZrCli_Project_EnsureParentDirectory(const TZrChar *filePath) {
    TZrChar working[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize startIndex = 0;
    TZrSize index;
    TZrSize length;

    if (filePath == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(working, sizeof(working), "%s", filePath);
    zr_cli_normalize_separators(working);

    length = strlen(working);
    while (length > 0 && working[length - 1] != '/') {
        working[--length] = '\0';
    }

    if (length == 0) {
        return ZR_TRUE;
    }

    if (length >= 3 && working[1] == ':' && working[2] == '/') {
        startIndex = 3;
    } else if (working[0] == '/') {
        startIndex = 1;
    }

    for (index = startIndex; working[index] != '\0'; index++) {
        if (working[index] != '/') {
            continue;
        }

        working[index] = '\0';
        if (working[0] != '\0' && !zr_cli_make_directory(working)) {
            return ZR_FALSE;
        }
        working[index] = '/';
    }

    return zr_cli_make_directory(working);
}

TZrBool ZrCli_Project_RemoveFileIfExists(const TZrChar *filePath) {
    if (filePath == ZR_NULL || filePath[0] == '\0') {
        return ZR_FALSE;
    }

    if (remove(filePath) == 0) {
        return ZR_TRUE;
    }

    return errno == ENOENT ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrCli_Project_ReadTextFile(const TZrChar *path, TZrChar **outBuffer, TZrSize *outLength) {
    FILE *file;
    TZrChar *buffer;
    long fileSize;
    TZrSize readSize;

    if (path == ZR_NULL || outBuffer == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    *outBuffer = ZR_NULL;
    *outLength = 0;
    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    buffer = (TZrChar *) malloc((TZrSize) fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    readSize = (TZrSize) fread(buffer, 1, (TZrSize) fileSize, file);
    fclose(file);
    if (readSize != (TZrSize) fileSize) {
        free(buffer);
        return ZR_FALSE;
    }

    buffer[readSize] = '\0';
    *outBuffer = buffer;
    *outLength = readSize;
    return ZR_TRUE;
}

TZrUInt64 ZrCli_Project_StableHashBytes(const TZrByte *bytes, TZrSize length) {
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;

    if (bytes == ZR_NULL) {
        return 0;
    }

    for (TZrSize index = 0; index < length; index++) {
        hash ^= bytes[index];
        hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
    }

    return hash;
}

void ZrCli_Project_HashToHex(TZrUInt64 hash, TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long) hash);
}

void ZrCli_Project_StringList_Init(SZrCliStringList *list) {
    if (list == ZR_NULL) {
        return;
    }

    list->items = ZR_NULL;
    list->count = 0;
    list->capacity = 0;
}

void ZrCli_Project_StringList_Free(SZrCliStringList *list) {
    if (list == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < list->count; index++) {
        free(list->items[index]);
    }
    free(list->items);
    list->items = ZR_NULL;
    list->count = 0;
    list->capacity = 0;
}

TZrBool ZrCli_Project_StringList_AppendUnique(SZrCliStringList *list, const TZrChar *value) {
    TZrChar *copy;
    TZrChar **newItems;
    TZrSize newCapacity;

    if (list == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < list->count; index++) {
        if (strcmp(list->items[index], value) == 0) {
            return ZR_TRUE;
        }
    }

    if (list->count == list->capacity) {
        newCapacity = list->capacity == 0 ? ZR_CLI_SMALL_COLLECTION_INITIAL_CAPACITY
                                          : list->capacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
        newItems = (TZrChar **) realloc(list->items, newCapacity * sizeof(*newItems));
        if (newItems == ZR_NULL) {
            return ZR_FALSE;
        }
        list->items = newItems;
        list->capacity = newCapacity;
    }

    copy = zr_cli_strdup(value);
    if (copy == ZR_NULL) {
        return ZR_FALSE;
    }

    list->items[list->count++] = copy;
    return ZR_TRUE;
}

TZrBool ZrCli_Project_StringList_Equals(const SZrCliStringList *left, const SZrCliStringList *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }

    if (left->count != right->count) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < left->count; index++) {
        if (strcmp(left->items[index], right->items[index]) != 0) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrCli_Project_StringList_Copy(SZrCliStringList *destination, const SZrCliStringList *source) {
    if (destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCli_Project_StringList_Init(destination);
    for (TZrSize index = 0; index < source->count; index++) {
        if (!ZrCli_Project_StringList_AppendUnique(destination, source->items[index])) {
            ZrCli_Project_StringList_Free(destination);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

void ZrCli_Project_Manifest_Init(SZrCliIncrementalManifest *manifest) {
    if (manifest == ZR_NULL) {
        return;
    }

    manifest->version = ZR_CLI_MANIFEST_FORMAT_VERSION;
    manifest->entries = ZR_NULL;
    manifest->count = 0;
    manifest->capacity = 0;
}

void ZrCli_Project_Manifest_Free(SZrCliIncrementalManifest *manifest) {
    if (manifest == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < manifest->count; index++) {
        ZrCli_Project_StringList_Free(&manifest->entries[index].imports);
    }
    free(manifest->entries);
    manifest->entries = ZR_NULL;
    manifest->count = 0;
    manifest->capacity = 0;
    manifest->version = ZR_CLI_MANIFEST_FORMAT_VERSION;
}

TZrBool ZrCli_Project_LoadManifest(const SZrCliProjectContext *context, SZrCliIncrementalManifest *manifest) {
    TZrChar *content = ZR_NULL;
    TZrSize length = 0;
    TZrChar *cursor;
    TZrChar *line;
    SZrCliManifestEntry *current = ZR_NULL;
    const TZrChar *value = ZR_NULL;

    if (context == ZR_NULL || manifest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCli_Project_Manifest_Init(manifest);
    if (ZrLibrary_File_Exist((TZrNativeString)context->manifestPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_TRUE;
    }

    if (!ZrCli_Project_ReadTextFile(context->manifestPath, &content, &length)) {
        return ZR_FALSE;
    }

    cursor = content;
    line = zr_cli_next_line(&cursor);
    if (line == ZR_NULL ||
        (strcmp(line, "zr_cli_manifest_v1") != 0 && strcmp(line, "zr_cli_manifest_v2") != 0)) {
        free(content);
        ZrCli_Project_Manifest_Free(manifest);
        return ZR_FALSE;
    }
    manifest->version = strcmp(line, "zr_cli_manifest_v2") == 0 ? 2U : 1U;

    while ((line = zr_cli_next_line(&cursor)) != ZR_NULL) {
        if (line[0] == '\0') {
            continue;
        }

        if (strncmp(line, "module ", 7) == 0) {
            SZrCliManifestEntry entry;
            memset(&entry, 0, sizeof(entry));
            ZrCli_Project_StringList_Init(&entry.imports);
            snprintf(entry.moduleName, sizeof(entry.moduleName), "%s", line + 7);
            if (!zr_cli_append_manifest_entry(manifest, &entry)) {
                ZrCli_Project_StringList_Free(&entry.imports);
                free(content);
                ZrCli_Project_Manifest_Free(manifest);
                return ZR_FALSE;
            }
            current = &manifest->entries[manifest->count - 1];
            continue;
        }

        if (strcmp(line, "end") == 0) {
            current = ZR_NULL;
            continue;
        }

        if (current == ZR_NULL) {
            free(content);
            ZrCli_Project_Manifest_Free(manifest);
            return ZR_FALSE;
        }

        if (zr_cli_manifest_match_key(line, "hash", &value)) {
            snprintf(current->sourceHash, sizeof(current->sourceHash), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "zro_hash", &value)) {
            snprintf(current->zroHash, sizeof(current->zroHash), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "aot_c_input_hash", &value)) {
            snprintf(current->aotCInputHash, sizeof(current->aotCInputHash), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "zro", &value)) {
            snprintf(current->zroPath, sizeof(current->zroPath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "zri", &value)) {
            snprintf(current->zriPath, sizeof(current->zriPath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "aot_c_src", &value)) {
            snprintf(current->aotCSourcePath, sizeof(current->aotCSourcePath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "aot_c_lib", &value)) {
            snprintf(current->aotCLibraryPath, sizeof(current->aotCLibraryPath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "aot_c_input_kind", &value)) {
            current->aotCInputKind = (TZrUInt32)strtoul(value, ZR_NULL, 10);
        } else if (zr_cli_manifest_match_key(line, "aot_c_abi_version", &value)) {
            current->aotCAbiVersion = (TZrUInt32)strtoul(value, ZR_NULL, 10);
        } else if (zr_cli_manifest_match_key(line, "aot_llvm_ir", &value)) {
            snprintf(current->aotLlvmIrPath, sizeof(current->aotLlvmIrPath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "aot_llvm_lib", &value)) {
            snprintf(current->aotLlvmLibraryPath, sizeof(current->aotLlvmLibraryPath), "%s", value);
        } else if (zr_cli_manifest_match_key(line, "import", &value)) {
            if (!ZrCli_Project_StringList_AppendUnique(&current->imports, value)) {
                free(content);
                ZrCli_Project_Manifest_Free(manifest);
                return ZR_FALSE;
            }
        } else if (zr_cli_manifest_match_key(line, "imports", &value)) {
            continue;
        } else {
            free(content);
            ZrCli_Project_Manifest_Free(manifest);
            return ZR_FALSE;
        }
    }

    free(content);
    return current == ZR_NULL;
}

TZrBool ZrCli_Project_SaveManifest(const SZrCliProjectContext *context, const SZrCliIncrementalManifest *manifest) {
    FILE *file;

    if (context == ZR_NULL || manifest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_EnsureParentDirectory(context->manifestPath)) {
        return ZR_FALSE;
    }

    file = fopen(context->manifestPath, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file, "zr_cli_manifest_v%u\n", (unsigned)ZR_CLI_MANIFEST_FORMAT_VERSION);
    for (TZrSize index = 0; index < manifest->count; index++) {
        const SZrCliManifestEntry *entry = &manifest->entries[index];

        fprintf(file, "module %s\n", entry->moduleName);
        fprintf(file, "hash %s\n", entry->sourceHash);
        fprintf(file, "zro_hash %s\n", entry->zroHash);
        fprintf(file, "aot_c_input_hash %s\n", entry->aotCInputHash);
        fprintf(file, "zro %s\n", entry->zroPath);
        fprintf(file, "zri %s\n", entry->zriPath);
        fprintf(file, "aot_c_src %s\n", entry->aotCSourcePath);
        fprintf(file, "aot_c_lib %s\n", entry->aotCLibraryPath);
        fprintf(file, "aot_c_input_kind %u\n", (unsigned)entry->aotCInputKind);
        fprintf(file, "aot_c_abi_version %u\n", (unsigned)entry->aotCAbiVersion);
        fprintf(file, "aot_llvm_ir %s\n", entry->aotLlvmIrPath);
        fprintf(file, "aot_llvm_lib %s\n", entry->aotLlvmLibraryPath);
        fprintf(file, "imports %llu\n", (unsigned long long) entry->imports.count);
        for (TZrSize importIndex = 0; importIndex < entry->imports.count; importIndex++) {
            fprintf(file, "import %s\n", entry->imports.items[importIndex]);
        }
        fprintf(file, "end\n");
    }

    fclose(file);
    return ZR_TRUE;
}

SZrCliManifestEntry *ZrCli_Project_FindManifestEntry(SZrCliIncrementalManifest *manifest, const TZrChar *moduleName) {
    if (manifest == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < manifest->count; index++) {
        if (strcmp(manifest->entries[index].moduleName, moduleName) == 0) {
            return &manifest->entries[index];
        }
    }

    return ZR_NULL;
}

const SZrCliManifestEntry *ZrCli_Project_FindManifestEntryConst(const SZrCliIncrementalManifest *manifest,
                                                                const TZrChar *moduleName) {
    if (manifest == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < manifest->count; index++) {
        if (strcmp(manifest->entries[index].moduleName, moduleName) == 0) {
            return &manifest->entries[index];
        }
    }

    return ZR_NULL;
}
