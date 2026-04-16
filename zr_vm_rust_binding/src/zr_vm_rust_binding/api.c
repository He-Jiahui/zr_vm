#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <direct.h>
#define ZR_RUST_BINDING_MKDIR(path) _mkdir(path)
#else
#define ZR_RUST_BINDING_MKDIR(path) mkdir(path, ZR_VM_POSIX_DIRECTORY_CREATE_MODE)
#endif

static ZrRustBindingErrorInfo g_zr_rust_binding_last_error = {ZR_RUST_BINDING_STATUS_OK, {0}};

static ZrRustBindingStatus zr_rust_binding_workspace_init(const TZrChar *projectPath,
                                                          ZrRustBindingProjectWorkspace *workspace) {
    SZrGlobalState *global;

    if (projectPath == ZR_NULL || workspace == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "projectPath or workspace is null");
    }

    global = ZrCli_Project_CreateProjectGlobal(projectPath);
    if (global == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "failed to load project: %s",
                                         projectPath);
    }

    memset(workspace, 0, sizeof(*workspace));
    if (!ZrCli_ProjectContext_FromGlobal(&workspace->context, global, projectPath)) {
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to resolve project context: %s",
                                         projectPath);
    }

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

static const SZrCliManifestEntry *zr_rust_binding_manifest_entry_at(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex) {
    if (manifestSnapshot == ZR_NULL || entryIndex >= manifestSnapshot->manifest.count) {
        return ZR_NULL;
    }

    return &manifestSnapshot->manifest.entries[entryIndex];
}

static ZrRustBindingStatus zr_rust_binding_manifest_copy_entry_string(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        const TZrChar *source,
        TZrChar *buffer,
        TZrSize bufferSize,
        const TZrChar *fieldName) {
    if (zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex) == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "manifest entry index out of range");
    }
    if (!zr_rust_binding_copy_string_to_buffer(source != ZR_NULL ? source : "", buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL,
                                         "failed to copy manifest entry %s",
                                         fieldName != ZR_NULL ? fieldName : "field");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

static TZrBool zr_rust_binding_join_path(const TZrChar *left,
                                         const TZrChar *right,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    TZrSize leftLength;
    TZrBool needsSeparator;

    if (left == ZR_NULL || right == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    leftLength = strlen(left);
    needsSeparator = leftLength > 0 && left[leftLength - 1] != '/' && left[leftLength - 1] != '\\';
    return snprintf(buffer,
                    bufferSize,
                    needsSeparator ? "%s/%s" : "%s%s",
                    left,
                    right) >= 0;
}

static TZrBool zr_rust_binding_ensure_directory(const TZrChar *path) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize index;
    TZrSize length;

    if (path == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(path);
    if (length == 0 || length + 1U > sizeof(buffer)) {
        return ZR_FALSE;
    }

    memcpy(buffer, path, length + 1U);
    for (index = 0; index < length; index++) {
        if (buffer[index] != '/' && buffer[index] != '\\') {
            continue;
        }
        if (index == 0 || (index == 2 && buffer[1] == ':')) {
            continue;
        }
        buffer[index] = '\0';
        if (buffer[0] != '\0' && ZR_RUST_BINDING_MKDIR(buffer) != 0 && errno != EEXIST) {
            buffer[index] = '/';
            return ZR_FALSE;
        }
        buffer[index] = '/';
    }

    return ZR_RUST_BINDING_MKDIR(buffer) == 0 || errno == EEXIST;
}

static TZrBool zr_rust_binding_write_text_file(const TZrChar *path, const TZrChar *text) {
    FILE *file;

    if (path == ZR_NULL || text == ZR_NULL || !ZrCli_Project_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fwrite(text, 1, strlen(text), file);
    fclose(file);
    return ZR_TRUE;
}

static void zr_rust_binding_apply_runtime_options(const ZrRustBindingRuntime *runtime, SZrGlobalState *global) {
    if (runtime == ZR_NULL || global == ZR_NULL) {
        return;
    }

    if (runtime->options.heapLimitBytes > 0U) {
        ZrCore_GarbageCollector_SetHeapLimitBytes(global, (TZrMemoryOffset)runtime->options.heapLimitBytes);
    }
    if (runtime->options.pauseBudgetUs > 0U || runtime->options.remarkBudgetUs > 0U) {
        ZrCore_GarbageCollector_SetPauseBudgetUs(global,
                                                 runtime->options.pauseBudgetUs,
                                                 runtime->options.remarkBudgetUs);
    }
    if (runtime->options.workerCount > 0U) {
        ZrCore_GarbageCollector_SetWorkerCount(global, runtime->options.workerCount);
    }
}

static TZrBool zr_rust_binding_runtime_bootstrap_callback(SZrGlobalState *global, TZrPtr userData) {
    return zr_rust_binding_runtime_register_native_modules((ZrRustBindingRuntime *)userData, global);
}

void zr_rust_binding_clear_error(void) {
    g_zr_rust_binding_last_error.status = ZR_RUST_BINDING_STATUS_OK;
    g_zr_rust_binding_last_error.message[0] = '\0';
}

ZrRustBindingStatus zr_rust_binding_set_error(ZrRustBindingStatus status, const TZrChar *format, ...) {
    va_list arguments;

    g_zr_rust_binding_last_error.status = status;
    if (format == ZR_NULL) {
        g_zr_rust_binding_last_error.message[0] = '\0';
        return status;
    }

    va_start(arguments, format);
    vsnprintf(g_zr_rust_binding_last_error.message,
              sizeof(g_zr_rust_binding_last_error.message),
              format,
              arguments);
    va_end(arguments);
    return status;
}

void ZrRustBinding_GetLastErrorInfo(ZrRustBindingErrorInfo *outErrorInfo) {
    if (outErrorInfo != ZR_NULL) {
        *outErrorInfo = g_zr_rust_binding_last_error;
    }
}

ZrRustBindingStatus ZrRustBinding_Runtime_NewBare(const ZrRustBindingRuntimeOptions *options,
                                                  ZrRustBindingRuntime **outRuntime) {
    ZrRustBindingRuntime *runtime;

    if (outRuntime == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "outRuntime is null");
    }

    runtime = (ZrRustBindingRuntime *)calloc(1, sizeof(*runtime));
    if (runtime == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to allocate runtime");
    }

    if (options != ZR_NULL) {
        runtime->options = *options;
    }
    runtime->standardProfile = ZR_FALSE;
    *outRuntime = runtime;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Runtime_NewStandard(const ZrRustBindingRuntimeOptions *options,
                                                      ZrRustBindingRuntime **outRuntime) {
    ZrRustBindingStatus status = ZrRustBinding_Runtime_NewBare(options, outRuntime);
    if (status == ZR_RUST_BINDING_STATUS_OK) {
        (*outRuntime)->standardProfile = ZR_TRUE;
    }
    return status;
}

ZrRustBindingStatus ZrRustBinding_Runtime_Free(ZrRustBindingRuntime *runtime) {
    zr_rust_binding_runtime_native_registry_free(runtime);
    free(runtime);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Project_Scaffold(const ZrRustBindingScaffoldOptions *options,
                                                   ZrRustBindingProjectWorkspace **outWorkspace) {
    TZrChar projectFile[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar mainSourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestContent[512];
    ZrRustBindingProjectWorkspace *workspace;

    if (options == ZR_NULL || outWorkspace == ZR_NULL || options->rootPath == ZR_NULL || options->projectName == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "scaffold options are incomplete");
    }

    if (!zr_rust_binding_ensure_directory(options->rootPath) ||
        !zr_rust_binding_join_path(options->rootPath, "src", sourceDirectory, sizeof(sourceDirectory)) ||
        !zr_rust_binding_join_path(options->rootPath, "bin", binaryDirectory, sizeof(binaryDirectory)) ||
        !zr_rust_binding_join_path(options->rootPath, "src/main.zr", mainSourcePath, sizeof(mainSourcePath)) ||
        snprintf(projectFile,
                 sizeof(projectFile),
                 "%s/%s.zrp",
                 options->rootPath,
                 options->projectName) < 0 ||
        !zr_rust_binding_ensure_directory(sourceDirectory) ||
        !zr_rust_binding_ensure_directory(binaryDirectory)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_IO_ERROR, "failed to prepare scaffold directories");
    }

    if (!options->overwriteExisting &&
        (ZrLibrary_File_Exist((TZrNativeString)projectFile) == ZR_LIBRARY_FILE_IS_FILE ||
         ZrLibrary_File_Exist((TZrNativeString)mainSourcePath) == ZR_LIBRARY_FILE_IS_FILE)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_ALREADY_EXISTS,
                                         "project scaffold already exists under %s",
                                         options->rootPath);
    }

    snprintf(manifestContent,
             sizeof(manifestContent),
             "{\n  \"name\": \"%s\",\n  \"source\": \"src\",\n  \"binary\": \"bin\",\n  \"entry\": \"main\"\n}\n",
             options->projectName);
    if (!zr_rust_binding_write_text_file(projectFile, manifestContent) ||
        !zr_rust_binding_write_text_file(mainSourcePath, "return \"hello zr\";\n")) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_IO_ERROR, "failed to write scaffold files");
    }

    workspace = (ZrRustBindingProjectWorkspace *)calloc(1, sizeof(*workspace));
    if (workspace == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to allocate workspace");
    }

    if (zr_rust_binding_workspace_init(projectFile, workspace) != ZR_RUST_BINDING_STATUS_OK) {
        free(workspace);
        return g_zr_rust_binding_last_error.status;
    }

    *outWorkspace = workspace;
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Project_Open(const TZrChar *projectPath, ZrRustBindingProjectWorkspace **outWorkspace) {
    ZrRustBindingProjectWorkspace *workspace;

    if (projectPath == ZR_NULL || outWorkspace == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "projectPath or outWorkspace is null");
    }

    workspace = (ZrRustBindingProjectWorkspace *)calloc(1, sizeof(*workspace));
    if (workspace == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to allocate workspace");
    }

    if (zr_rust_binding_workspace_init(projectPath, workspace) != ZR_RUST_BINDING_STATUS_OK) {
        free(workspace);
        return g_zr_rust_binding_last_error.status;
    }

    *outWorkspace = workspace;
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_Free(ZrRustBindingProjectWorkspace *workspace) {
    free(workspace);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectPath(const ZrRustBindingProjectWorkspace *workspace,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize) {
    if (workspace == ZR_NULL || !zr_rust_binding_copy_string_to_buffer(workspace->context.projectPath, buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "failed to copy project path");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetProjectRoot(const ZrRustBindingProjectWorkspace *workspace,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize) {
    if (workspace == ZR_NULL || !zr_rust_binding_copy_string_to_buffer(workspace->context.projectRoot, buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "failed to copy project root");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetManifestPath(const ZrRustBindingProjectWorkspace *workspace,
                                                                  TZrChar *buffer,
                                                                  TZrSize bufferSize) {
    if (workspace == ZR_NULL || !zr_rust_binding_copy_string_to_buffer(workspace->context.manifestPath, buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "failed to copy manifest path");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_GetEntryModule(const ZrRustBindingProjectWorkspace *workspace,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize) {
    if (workspace == ZR_NULL || !zr_rust_binding_copy_string_to_buffer(workspace->context.entryModule, buffer, bufferSize)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "failed to copy entry module");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_ResolveArtifacts(const ZrRustBindingProjectWorkspace *workspace,
                                                                   const TZrChar *moduleName,
                                                                   TZrChar *zroBuffer,
                                                                   TZrSize zroBufferSize,
                                                                   TZrChar *zriBuffer,
                                                                   TZrSize zriBufferSize) {
    const TZrChar *effectiveModuleName;

    if (workspace == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "workspace is null");
    }

    effectiveModuleName = moduleName != ZR_NULL ? moduleName : workspace->context.entryModule;
    if ((zroBuffer != ZR_NULL && !ZrCli_Project_ResolveBinaryPath(&workspace->context, effectiveModuleName, zroBuffer, zroBufferSize)) ||
        (zriBuffer != ZR_NULL && !ZrCli_Project_ResolveIntermediatePath(&workspace->context, effectiveModuleName, zriBuffer, zriBufferSize))) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL, "failed to resolve artifact paths");
    }

    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectWorkspace_LoadManifest(
        const ZrRustBindingProjectWorkspace *workspace,
        ZrRustBindingManifestSnapshot **outManifestSnapshot) {
    ZrRustBindingManifestSnapshot *manifestSnapshot;

    if (workspace == ZR_NULL || outManifestSnapshot == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "workspace or outManifestSnapshot is null");
    }

    manifestSnapshot = (ZrRustBindingManifestSnapshot *)calloc(1, sizeof(*manifestSnapshot));
    if (manifestSnapshot == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate manifest snapshot");
    }

    ZrCli_Project_Manifest_Init(&manifestSnapshot->manifest);
    if (!ZrCli_Project_LoadManifest(&workspace->context, &manifestSnapshot->manifest)) {
        ZrCli_Project_Manifest_Free(&manifestSnapshot->manifest);
        free(manifestSnapshot);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_IO_ERROR,
                                         "failed to load manifest: %s",
                                         workspace->context.manifestPath);
    }

    *outManifestSnapshot = manifestSnapshot;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetVersion(const ZrRustBindingManifestSnapshot *manifestSnapshot,
                                                              TZrUInt32 *outVersion) {
    if (manifestSnapshot == ZR_NULL || outVersion == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "manifestSnapshot or outVersion is null");
    }

    *outVersion = manifestSnapshot->manifest.version;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize *outEntryCount) {
    if (manifestSnapshot == ZR_NULL || outEntryCount == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "manifestSnapshot or outEntryCount is null");
    }

    *outEntryCount = manifestSnapshot->manifest.count;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_FindEntry(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        const TZrChar *moduleName,
        TZrSize *outEntryIndex) {
    TZrSize index;

    if (manifestSnapshot == ZR_NULL || moduleName == ZR_NULL || outEntryIndex == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "manifestSnapshot, moduleName, or outEntryIndex is null");
    }

    for (index = 0; index < manifestSnapshot->manifest.count; index++) {
        if (strcmp(manifestSnapshot->manifest.entries[index].moduleName, moduleName) == 0) {
            *outEntryIndex = index;
            zr_rust_binding_clear_error();
            return ZR_RUST_BINDING_STATUS_OK;
        }
    }

    return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                     "manifest entry not found: %s",
                                     moduleName);
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryModuleName(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry != ZR_NULL ? entry->moduleName : ZR_NULL,
                                                      buffer,
                                                      bufferSize,
                                                      "moduleName");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntrySourceHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry != ZR_NULL ? entry->sourceHash : ZR_NULL,
                                                      buffer,
                                                      bufferSize,
                                                      "sourceHash");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroHash(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry != ZR_NULL ? entry->zroHash : ZR_NULL,
                                                      buffer,
                                                      bufferSize,
                                                      "zroHash");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZroPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry != ZR_NULL ? entry->zroPath : ZR_NULL,
                                                      buffer,
                                                      bufferSize,
                                                      "zroPath");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryZriPath(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry != ZR_NULL ? entry->zriPath : ZR_NULL,
                                                      buffer,
                                                      bufferSize,
                                                      "zriPath");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImportCount(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize *outImportCount) {
    const SZrCliManifestEntry *entry;

    if (outImportCount == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "outImportCount is null");
    }

    entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);
    if (entry == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "manifest entry index out of range");
    }

    *outImportCount = entry->imports.count;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_GetEntryImport(
        const ZrRustBindingManifestSnapshot *manifestSnapshot,
        TZrSize entryIndex,
        TZrSize importIndex,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrCliManifestEntry *entry = zr_rust_binding_manifest_entry_at(manifestSnapshot, entryIndex);

    if (entry == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "manifest entry index out of range");
    }
    if (importIndex >= entry->imports.count) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_NOT_FOUND,
                                         "manifest import index out of range");
    }

    return zr_rust_binding_manifest_copy_entry_string(manifestSnapshot,
                                                      entryIndex,
                                                      entry->imports.items[importIndex],
                                                      buffer,
                                                      bufferSize,
                                                      "import");
}

ZrRustBindingStatus ZrRustBinding_ManifestSnapshot_Free(ZrRustBindingManifestSnapshot *manifestSnapshot) {
    if (manifestSnapshot != ZR_NULL) {
        ZrCli_Project_Manifest_Free(&manifestSnapshot->manifest);
        free(manifestSnapshot);
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Project_Compile(ZrRustBindingRuntime *runtime,
                                                  const ZrRustBindingProjectWorkspace *workspace,
                                                  const ZrRustBindingCompileOptions *options,
                                                  ZrRustBindingCompileResult **outCompileResult) {
    SZrCliCommand command;
    SZrCliCompileSummary summary;
    ZrRustBindingCompileResult *compileResult;

    if (workspace == ZR_NULL || outCompileResult == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "workspace or outCompileResult is null");
    }

    memset(&command, 0, sizeof(command));
    memset(&summary, 0, sizeof(summary));
    command.mode = ZR_CLI_MODE_COMPILE_PROJECT;
    command.executionMode = ZR_CLI_EXECUTION_MODE_INTERP;
    command.projectPath = workspace->context.projectPath;
    command.emitIntermediate = options != ZR_NULL ? options->emitIntermediate : ZR_FALSE;
    command.incremental = options != ZR_NULL ? options->incremental : ZR_FALSE;

    if (!ZrCli_Compiler_CompileProjectWithSummaryAndBootstrap(&command,
                                                              &summary,
                                                              runtime != ZR_NULL
                                                                      ? zr_rust_binding_runtime_bootstrap_callback
                                                                      : ZR_NULL,
                                                              runtime)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_COMPILE_ERROR,
                                         "failed to compile project: %s",
                                         workspace->context.projectPath);
    }

    compileResult = (ZrRustBindingCompileResult *)calloc(1, sizeof(*compileResult));
    if (compileResult == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to allocate compile result");
    }

    compileResult->compiledCount = summary.compiledCount;
    compileResult->skippedCount = summary.skippedCount;
    compileResult->removedCount = summary.removedCount;
    *outCompileResult = compileResult;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_CompileResult_GetCounts(const ZrRustBindingCompileResult *compileResult,
                                                          TZrSize *outCompiledCount,
                                                          TZrSize *outSkippedCount,
                                                          TZrSize *outRemovedCount) {
    if (compileResult == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT, "compileResult is null");
    }

    if (outCompiledCount != ZR_NULL) {
        *outCompiledCount = compileResult->compiledCount;
    }
    if (outSkippedCount != ZR_NULL) {
        *outSkippedCount = compileResult->skippedCount;
    }
    if (outRemovedCount != ZR_NULL) {
        *outRemovedCount = compileResult->removedCount;
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_CompileResult_Free(ZrRustBindingCompileResult *compileResult) {
    free(compileResult);
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Project_Run(ZrRustBindingRuntime *runtime,
                                              const ZrRustBindingProjectWorkspace *workspace,
                                              const ZrRustBindingRunOptions *options,
                                              ZrRustBindingValue **outResult) {
    SZrCliCommand command;
    SZrCliPreparedProjectRuntime prepared;
    SZrCliRunCapture capture;
    ZrRustBindingExecutionOwner *owner;
    ZrRustBindingValue *value;

    if (runtime == ZR_NULL || workspace == ZR_NULL || outResult == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "runtime, workspace, or outResult is null");
    }
    if (!runtime->standardProfile) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_UNSUPPORTED,
                                         "bare runtime execution is not implemented yet");
    }

    memset(&command, 0, sizeof(command));
    command.mode = (options != ZR_NULL && options->moduleName != ZR_NULL)
                           ? ZR_CLI_MODE_RUN_PROJECT_MODULE
                           : ZR_CLI_MODE_RUN_PROJECT;
    command.executionMode = options != ZR_NULL && options->executionMode == ZR_RUST_BINDING_EXECUTION_MODE_BINARY
                                    ? ZR_CLI_EXECUTION_MODE_BINARY
                                    : ZR_CLI_EXECUTION_MODE_INTERP;
    command.projectPath = workspace->context.projectPath;
    command.moduleName = options != ZR_NULL ? options->moduleName : ZR_NULL;
    command.programArgs = options != ZR_NULL ? options->programArgs : ZR_NULL;
    command.programArgCount = options != ZR_NULL ? options->programArgCount : 0U;

    if (!ZrCli_Runtime_PrepareProjectExecutionWithBootstrap(&command,
                                                            &prepared,
                                                            zr_rust_binding_runtime_bootstrap_callback,
                                                            runtime)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                         "failed to run project: %s",
                                         workspace->context.projectPath);
    }

    zr_rust_binding_apply_runtime_options(runtime, prepared.global);
    if (!ZrCli_Runtime_RunPreparedProjectCapture(&prepared, &command, &capture)) {
        ZrCli_Runtime_PreparedProject_Free(&prepared);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                         "failed to run project: %s",
                                         workspace->context.projectPath);
    }

    owner = zr_rust_binding_execution_owner_new(capture.global, runtime);
    if (owner == ZR_NULL) {
        ZrCli_Runtime_RunCapture_Free(&capture);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to retain execution owner");
    }

    value = zr_rust_binding_value_new_live(owner, &capture.result);
    zr_rust_binding_execution_owner_release(owner);
    capture.global = ZR_NULL;
    ZrCli_Runtime_RunCapture_Free(&capture);
    if (value == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR, "failed to create result value");
    }

    *outResult = value;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_Project_CallModuleExport(ZrRustBindingRuntime *runtime,
                                                           const ZrRustBindingProjectWorkspace *workspace,
                                                           const ZrRustBindingRunOptions *options,
                                                           const TZrChar *moduleName,
                                                           const TZrChar *exportName,
                                                           ZrRustBindingValue *const *arguments,
                                                           TZrSize argumentCount,
                                                           ZrRustBindingValue **outResult) {
    SZrCliCommand command;
    SZrCliPreparedProjectRuntime prepared;
    SZrState *state;
    ZrLibTempValueRoot *argumentRoots = ZR_NULL;
    SZrTypeValue *materializedArguments = ZR_NULL;
    SZrTypeValue resultValue;
    ZrRustBindingExecutionOwner *owner = ZR_NULL;
    ZrRustBindingValue *resultHandle = ZR_NULL;
    TZrSize index;

    if (runtime == ZR_NULL || workspace == ZR_NULL || moduleName == ZR_NULL || exportName == ZR_NULL || outResult == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "runtime, workspace, moduleName, exportName, or outResult is null");
    }
    if (!runtime->standardProfile) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_UNSUPPORTED,
                                         "bare runtime execution is not implemented yet");
    }

    memset(&command, 0, sizeof(command));
    ZrCore_Value_ResetAsNull(&resultValue);
    command.mode = (options != ZR_NULL && options->moduleName != ZR_NULL)
                           ? ZR_CLI_MODE_RUN_PROJECT_MODULE
                           : ZR_CLI_MODE_RUN_PROJECT;
    command.executionMode = options != ZR_NULL && options->executionMode == ZR_RUST_BINDING_EXECUTION_MODE_BINARY
                                    ? ZR_CLI_EXECUTION_MODE_BINARY
                                    : ZR_CLI_EXECUTION_MODE_INTERP;
    command.projectPath = workspace->context.projectPath;
    command.moduleName = options != ZR_NULL ? options->moduleName : ZR_NULL;
    command.programArgs = options != ZR_NULL ? options->programArgs : ZR_NULL;
    command.programArgCount = options != ZR_NULL ? options->programArgCount : 0U;

    if (!ZrCli_Runtime_PrepareProjectExecutionWithBootstrap(&command,
                                                            &prepared,
                                                            zr_rust_binding_runtime_bootstrap_callback,
                                                            runtime)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                         "failed to prepare project runtime for export call: %s",
                                         workspace->context.projectPath);
    }

    zr_rust_binding_apply_runtime_options(runtime, prepared.global);
    state = prepared.global->mainThreadState;
    if (argumentCount > 0U) {
        argumentRoots = (ZrLibTempValueRoot *)calloc(argumentCount, sizeof(*argumentRoots));
        materializedArguments = (SZrTypeValue *)calloc(argumentCount, sizeof(*materializedArguments));
        if (argumentRoots == ZR_NULL || materializedArguments == ZR_NULL) {
            ZrCli_Runtime_PreparedProject_Free(&prepared);
            free(argumentRoots);
            free(materializedArguments);
            return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                             "failed to allocate module export arguments");
        }

        for (index = 0; index < argumentCount; index++) {
            SZrTypeValue *rootedArgument;

            if (arguments == ZR_NULL || arguments[index] == ZR_NULL || !ZrLib_TempValueRoot_Begin(state, &argumentRoots[index])) {
                goto zr_rust_binding_call_module_export_cleanup_error;
            }

            rootedArgument = ZrLib_TempValueRoot_Value(&argumentRoots[index]);
            if (rootedArgument == ZR_NULL ||
                !zr_rust_binding_materialize_value(state, arguments[index], &materializedArguments[index]) ||
                !ZrLib_TempValueRoot_SetValue(&argumentRoots[index], &materializedArguments[index])) {
                goto zr_rust_binding_call_module_export_cleanup_error;
            }

            materializedArguments[index] = *rootedArgument;
        }
    }

    if (!ZrLib_CallModuleExport(state, moduleName, exportName, materializedArguments, argumentCount, &resultValue)) {
        goto zr_rust_binding_call_module_export_cleanup_error;
    }

    owner = zr_rust_binding_execution_owner_new(prepared.global, runtime);
    if (owner == ZR_NULL) {
        goto zr_rust_binding_call_module_export_cleanup_error;
    }

    resultHandle = zr_rust_binding_value_new_live(owner, &resultValue);
    zr_rust_binding_execution_owner_release(owner);
    owner = ZR_NULL;
    if (resultHandle == ZR_NULL) {
        goto zr_rust_binding_call_module_export_cleanup_error;
    }

    for (index = 0; index < argumentCount; index++) {
        ZrLib_TempValueRoot_End(&argumentRoots[index]);
    }
    free(argumentRoots);
    free(materializedArguments);
    prepared.global = ZR_NULL;
    ZrCli_Runtime_PreparedProject_Free(&prepared);

    *outResult = resultHandle;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;

zr_rust_binding_call_module_export_cleanup_error:
    for (index = 0; index < argumentCount; index++) {
        ZrLib_TempValueRoot_End(&argumentRoots[index]);
    }
    free(argumentRoots);
    free(materializedArguments);
    zr_rust_binding_execution_owner_release(owner);
    ZrCli_Runtime_PreparedProject_Free(&prepared);
    return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                     "failed to call module export %s.%s",
                                     moduleName,
                                     exportName);
}
