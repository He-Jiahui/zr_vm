//
// Created by HeJiahui on 2025/7/27.
//
#include "cJSON/cJSON.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define ZR_JSON_READ_STRING(STATE, OBJECT, NAME)                                                                       \
    SZrString *NAME = ZR_NULL;                                                                                         \
    {                                                                                                                  \
        cJSON *JSON_##NAME = cJSON_GetObjectItemCaseSensitive(OBJECT, #NAME);                                          \
        if (cJSON_IsString(JSON_##NAME) && JSON_##NAME->valuestring != ZR_NULL) {                                      \
            NAME = ZrCore_String_CreateTryHitCache(STATE, JSON_##NAME->valuestring);                                         \
        } else {                                                                                                       \
            NAME = ZR_NULL;                                                                                            \
        }                                                                                                              \
    }

typedef struct ZrLibraryProjectExecuteRequest {
    SZrFunction *function;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrLibraryProjectExecuteRequest;

static void library_project_free_path_aliases(SZrGlobalState *global, SZrLibrary_Project *project);
static TZrBool library_project_validate_alias_key(const TZrChar *aliasKey);
static TZrBool library_project_validate_alias_module_prefix(const TZrChar *modulePrefix);
static TZrBool library_project_parse_path_aliases(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);

static TZrBool zr_library_project_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void zr_library_project_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!zr_library_project_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-library-project] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

static void library_project_free_path_aliases(SZrGlobalState *global, SZrLibrary_Project *project) {
    TZrSize aliasBytes;

    if (global == ZR_NULL || project == ZR_NULL || project->pathAliases == ZR_NULL || project->pathAliasCount == 0) {
        return;
    }

    aliasBytes = sizeof(*project->pathAliases) * project->pathAliasCount;
    ZrCore_Memory_RawFreeWithType(global, project->pathAliases, aliasBytes, ZR_MEMORY_NATIVE_TYPE_PROJECT);
    project->pathAliases = ZR_NULL;
    project->pathAliasCount = 0;
}

static TZrBool library_project_validate_alias_key(const TZrChar *aliasKey) {
    TZrSize index;

    if (aliasKey == ZR_NULL || aliasKey[0] != '@' || aliasKey[1] == '\0') {
        return ZR_FALSE;
    }

    for (index = 1; aliasKey[index] != '\0'; index++) {
        TZrChar current = aliasKey[index];
        if (current == '/' || current == '\\' || current == '.' || current == ' ' || current == '\t' ||
            current == '\r' || current == '\n') {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool library_project_validate_alias_module_prefix(const TZrChar *modulePrefix) {
    TZrSize segmentLength = 0;
    TZrSize index = 0;

    if (modulePrefix == ZR_NULL || modulePrefix[0] == '\0' || modulePrefix[0] == '@' || modulePrefix[0] == '.') {
        return ZR_FALSE;
    }

    for (;;) {
        TZrChar current = modulePrefix[index++];
        if (current == '\0' || current == '/') {
            if (segmentLength == 0) {
                return ZR_FALSE;
            }
            if (segmentLength == 1 && modulePrefix[index - 2] == '.') {
                return ZR_FALSE;
            }
            if (segmentLength == 2 && modulePrefix[index - 3] == '.' && modulePrefix[index - 2] == '.') {
                return ZR_FALSE;
            }
            if (current == '\0') {
                return ZR_TRUE;
            }
            segmentLength = 0;
            continue;
        }
        segmentLength++;
    }
}

static TZrBool library_project_parse_path_aliases(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *pathAliasesJson;
    cJSON *aliasEntry;
    TZrSize aliasCount = 0;
    TZrSize aliasIndex = 0;
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    pathAliasesJson = cJSON_GetObjectItemCaseSensitive(projectJson, "pathAliases");
    if (pathAliasesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(pathAliasesJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(aliasEntry, pathAliasesJson) {
        aliasCount++;
    }
    if (aliasCount == 0) {
        return ZR_TRUE;
    }

    project->pathAliases = (SZrLibrary_ProjectPathAlias *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(*project->pathAliases) * aliasCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->pathAliases == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->pathAliases, 0, sizeof(*project->pathAliases) * aliasCount);
    project->pathAliasCount = aliasCount;

    cJSON_ArrayForEach(aliasEntry, pathAliasesJson) {
        TZrChar normalizedPrefix[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize previousIndex;

        if (aliasEntry->string == ZR_NULL ||
            !cJSON_IsString(aliasEntry) ||
            aliasEntry->valuestring == ZR_NULL ||
            !library_project_validate_alias_key(aliasEntry->string) ||
            !ZrLibrary_Project_NormalizeModuleKey(aliasEntry->valuestring,
                                                  normalizedPrefix,
                                                  sizeof(normalizedPrefix)) ||
            !library_project_validate_alias_module_prefix(normalizedPrefix)) {
            return ZR_FALSE;
        }

        for (previousIndex = 0; previousIndex < aliasIndex; previousIndex++) {
            const TZrChar *existingAlias = ZrCore_String_GetNativeString(project->pathAliases[previousIndex].alias);
            if (existingAlias != ZR_NULL && strcmp(existingAlias, aliasEntry->string) == 0) {
                return ZR_FALSE;
            }
        }

        project->pathAliases[aliasIndex].alias =
                ZrCore_String_CreateTryHitCache(state, aliasEntry->string);
        project->pathAliases[aliasIndex].modulePrefix =
                ZrCore_String_CreateTryHitCache(state, normalizedPrefix);
        if (project->pathAliases[aliasIndex].alias == ZR_NULL ||
            project->pathAliases[aliasIndex].modulePrefix == ZR_NULL) {
            return ZR_FALSE;
        }
        aliasIndex++;
    }

    project->pathAliasCount = aliasIndex;
    return ZR_TRUE;
}

SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }
    memset(project, 0, sizeof(SZrLibrary_Project));
    project->signature = ZR_LIBRARY_PROJECT_SIGNATURE;

    cJSON *projectJson = cJSON_Parse(raw);
    if (projectJson == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }

    project->file = ZrCore_String_CreateTryHitCache(state, file);

    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool success = ZrLibrary_File_GetDirectory(file, path);
    if (!success) {
        cJSON_Delete(projectJson);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    project->directory = ZrCore_String_CreateTryHitCache(state, path);
    // cJSON *name = cJSON_GetObjectItemCaseSensitive(projectJson, "name");
    // if (cJSON_IsString(name) && name->valuestring != ZR_NULL) {
    //     project->name = ZrCore_String_CreateTryHitCache(state, name->valuestring);
    // } else {
    //     project->name = ZR_NULL;
    // }
    ZR_JSON_READ_STRING(state, projectJson, name);
    project->name = name;

    ZR_JSON_READ_STRING(state, projectJson, version);
    project->version = version;

    ZR_JSON_READ_STRING(state, projectJson, description);
    project->description = description;

    ZR_JSON_READ_STRING(state, projectJson, author);
    project->author = author;

    ZR_JSON_READ_STRING(state, projectJson, email);
    project->email = email;

    ZR_JSON_READ_STRING(state, projectJson, url);
    project->url = url;

    ZR_JSON_READ_STRING(state, projectJson, license);
    project->license = license;

    ZR_JSON_READ_STRING(state, projectJson, copyright);
    project->copyright = copyright;

    ZR_JSON_READ_STRING(state, projectJson, binary);
    project->binary = binary;

    ZR_JSON_READ_STRING(state, projectJson, source);
    project->source = source;

    ZR_JSON_READ_STRING(state, projectJson, entry);
    project->entry = entry;

    ZR_JSON_READ_STRING(state, projectJson, dependency);
    project->dependency = dependency;

    ZR_JSON_READ_STRING(state, projectJson, local);
    project->local = local;
    if (!library_project_parse_path_aliases(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    {
        cJSON *supportMultithread = cJSON_GetObjectItemCaseSensitive(projectJson, "supportMultithread");
        cJSON *autoCoroutine = cJSON_GetObjectItemCaseSensitive(projectJson, "autoCoroutine");

        project->supportMultithread =
                cJSON_IsBool(supportMultithread) ? (cJSON_IsTrue(supportMultithread) ? ZR_TRUE : ZR_FALSE) : ZR_FALSE;
        project->autoCoroutine =
                cJSON_IsBool(autoCoroutine) ? (cJSON_IsTrue(autoCoroutine) ? ZR_TRUE : ZR_FALSE) : ZR_TRUE;
    }
    cJSON_Delete(projectJson);

    if (project->source == ZR_NULL || project->binary == ZR_NULL || project->entry == ZR_NULL) {
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }

    return project;
}

void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project) {
    if (project == ZR_NULL) {
        return;
    }
    SZrGlobalState *global = state->global;
    library_project_free_path_aliases(global, project);
    ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static TZrBool library_project_resolve_module_file(const SZrLibrary_Project *project, TZrNativeString rootDirectory,
                                                 TZrNativeString modulePath, TZrNativeString extension,
                                                 TZrChar *resolvedPath) {
    if (project == ZR_NULL || rootDirectory == ZR_NULL || modulePath == ZR_NULL || extension == ZR_NULL ||
        resolvedPath == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fullRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleFile[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString projectDirectory = ZrCore_String_GetNativeString(project->directory);

    if (!ZrLibrary_Project_NormalizeModuleKey(modulePath, normalizedPath, sizeof(normalizedPath))) {
        return ZR_FALSE;
    }

    if (snprintf(moduleFile, sizeof(moduleFile), "%s%s", normalizedPath, extension) >=
        (int) sizeof(moduleFile)) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, rootDirectory, fullRoot);
    ZrLibrary_File_PathJoin(fullRoot, moduleFile, resolvedPath);
    return resolvedPath[0] != '\0';
}

static TZrBool library_project_resolve_source_path(const SZrLibrary_Project *project, TZrNativeString modulePath,
                                                 TZrChar *resolvedPath) {
    return library_project_resolve_module_file(project,
                                               ZrCore_String_GetNativeString(project->source),
                                               modulePath,
                                               ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                               resolvedPath);
}

static TZrBool library_project_resolve_binary_path(const SZrLibrary_Project *project, TZrNativeString modulePath,
                                                 TZrChar *resolvedPath) {
    return library_project_resolve_module_file(project, ZrCore_String_GetNativeString(project->binary), modulePath,
                                               ZR_VM_BINARY_MODULE_FILE_EXTENSION, resolvedPath);
}

static TZrBool library_project_load_resolved_file(SZrState *state, TZrNativeString filePath, TZrBool isBinary, SZrIo *io) {
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(state->global, filePath, isBinary);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = isBinary;
    return ZR_TRUE;
}

static EZrThreadStatus library_project_normalize_failure(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus effectiveStatus;

    if (state == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    effectiveStatus = status;
    if (effectiveStatus == ZR_THREAD_STATUS_FINE && state->threadStatus != ZR_THREAD_STATUS_FINE) {
        effectiveStatus = state->threadStatus;
    }
    if (effectiveStatus == ZR_THREAD_STATUS_FINE) {
        effectiveStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    if (!state->hasCurrentException) {
        (void)ZrCore_Exception_NormalizeStatus(state, effectiveStatus);
    }
    state->threadStatus = effectiveStatus;
    return effectiveStatus;
}

static void library_project_execute_body(SZrState *state, TZrPtr arguments) {
    ZrLibraryProjectExecuteRequest *request = (ZrLibraryProjectExecuteRequest *)arguments;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    zr_library_project_trace("execute body start function=%p stackTop=%p stackSize=%llu",
                             (void *)request->function,
                             (void *)state->stackTop.valuePointer,
                             (unsigned long long)request->function->stackSize);
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &anchor);
    zr_library_project_trace("execute body anchored base=%p stackTop=%p",
                             (void *)base,
                             (void *)state->stackTop.valuePointer);

    ZrCore_Closure_PushToStack(state, request->function, ZR_NULL, base, state->stackTop.valuePointer);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    if (closureValue == ZR_NULL || closureValue->value.object == ZR_NULL) {
        zr_library_project_trace("execute body closure push failed slot=%p",
                                 (void *)state->stackTop.valuePointer);
        return;
    }
    zr_library_project_trace("execute body closure slot=%p object=%p rawType=%d",
                             (void *)state->stackTop.valuePointer,
                             (void *)closureValue->value.object,
                             (int)closureValue->value.object->type);
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    zr_library_project_trace("execute body call start slot=%p stackTop=%p",
                             (void *)(state->stackTop.valuePointer - 1),
                             (void *)state->stackTop.valuePointer);
    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    zr_library_project_trace("execute body call done resultBase=%p threadStatus=%d",
                             (void *)request->resultBase,
                             (int)state->threadStatus);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static EZrThreadStatus library_project_execute_function(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrLibraryProjectExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    zr_library_project_trace("execute function enter function=%p", (void *)function);
    status = ZrCore_Exception_TryRun(state, library_project_execute_body, &request);
    zr_library_project_trace("execute function tryrun status=%d requestResult=%p callCompleted=%d threadStatus=%d",
                             (int)status,
                             (void *)request.resultBase,
                             (int)request.callCompleted,
                             (int)state->threadStatus);
    if (status != ZR_THREAD_STATUS_FINE) {
        return library_project_normalize_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        return library_project_normalize_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return ZR_THREAD_STATUS_FINE;
}

EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result) {
    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    SZrGlobalState *global = state->global;
    const SZrLibrary_Project *project = ZrLibrary_Project_GetFromGlobal(global);
    ZrCore_Value_ResetAsNull(result);

    if (project == ZR_NULL || project->entry == ZR_NULL || global->compileSource == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }
    zr_library_project_trace("run entry='%s'",
                             ZrCore_String_GetNativeString(project->entry));

    TZrChar entrySourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (!library_project_resolve_source_path(project, ZrCore_String_GetNativeString(project->entry), entrySourcePath)) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }
    zr_library_project_trace("resolved entry source='%s'", entrySourcePath);

    TZrNativeString sourceCode = ZrLibrary_File_ReadAll(global, entrySourcePath);
    zr_library_project_trace("read source=%p", (void *)sourceCode);
    if (sourceCode == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }

    TZrSize sourceLength = ZrCore_NativeString_Length(sourceCode);
    SZrString *sourceName = ZrCore_String_Create(state, entrySourcePath, ZrCore_NativeString_Length(entrySourcePath));
    zr_library_project_trace("compile source length=%llu sourceName=%p",
                             (unsigned long long)sourceLength,
                             (void *)sourceName);
    SZrFunction *function = global->compileSource(state, sourceCode, sourceLength, sourceName);
    TZrBool ignoredFunction = ZR_FALSE;
    EZrThreadStatus status;
    zr_library_project_trace("compiled function=%p threadStatus=%d", (void *)function, (int)state->threadStatus);
    ZrCore_Memory_RawFreeWithType(global, sourceCode, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);

    if (function == ZR_NULL) {
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        }
        return library_project_normalize_failure(state, state->threadStatus);
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));

    SZrObjectModule *projectModule = ZrCore_Module_Create(state);
    zr_library_project_trace("project module=%p", (void *)projectModule);
    if (projectModule != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, sourceName);
        ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, sourceName);
        ZrCore_Reflection_AttachModuleRuntimeMetadata(state, projectModule, function);
        ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, function);
        zr_library_project_trace("project module prepared pathHash=%llu", (unsigned long long)pathHash);
    }

    zr_library_project_trace("execute compiled function");
    status = library_project_execute_function(state, function, result);
    zr_library_project_trace("execute finished status=%d threadStatus=%d", (int)status, (int)state->threadStatus);
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    return status;
}

void ZrLibrary_Project_Do(SZrState *state) {
    SZrTypeValue ignoredResult;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    ZrLibrary_Project_Run(state, &ignoredResult);
}

TZrBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || state->global == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    const SZrLibrary_Project *project = ZrLibrary_Project_GetFromGlobal(state->global);
    if (project == ZR_NULL || path == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrChar fullPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (library_project_resolve_source_path(project, path, fullPath) &&
        ZrLibrary_File_Exist(fullPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, fullPath, ZR_FALSE, io);
    }

    if (library_project_resolve_binary_path(project, path, fullPath) &&
        ZrLibrary_File_Exist(fullPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, fullPath, ZR_TRUE, io);
    }

    return ZR_FALSE;
}
