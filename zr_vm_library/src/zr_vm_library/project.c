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
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

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
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrLibraryProjectExecuteRequest;

SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }
    memset(project, 0, sizeof(SZrLibrary_Project));

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
    {
        cJSON *supportMultithread = cJSON_GetObjectItemCaseSensitive(projectJson, "supportMultithread");
        cJSON *autoCoroutine = cJSON_GetObjectItemCaseSensitive(projectJson, "autoCoroutine");

        project->supportMultithread =
                cJSON_IsBool(supportMultithread) ? (cJSON_IsTrue(supportMultithread) ? ZR_TRUE : ZR_FALSE) : ZR_FALSE;
        project->autoCoroutine =
                cJSON_IsBool(autoCoroutine) ? (cJSON_IsTrue(autoCoroutine) ? ZR_TRUE : ZR_FALSE) : ZR_TRUE;
    }
    project->aotRuntime = ZR_NULL;

    cJSON_Delete(projectJson);

    if (project->source == ZR_NULL || project->binary == ZR_NULL || project->entry == ZR_NULL) {
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
    ZrLibrary_AotRuntime_FreeProjectState(state, project);
    ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static TZrBool library_project_normalize_module_path(TZrNativeString modulePath, TZrChar *normalizedPath) {
    if (modulePath == ZR_NULL || normalizedPath == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrSize length = ZrCore_NativeString_Length(modulePath);
    if (length == 0 || length >= ZR_LIBRARY_MAX_PATH_LENGTH) {
        return ZR_FALSE;
    }

    if (length >= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH &&
        strcmp(modulePath + length - ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION) == 0) {
        length -= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH;
    } else if (length >= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH &&
               strcmp(modulePath + length - ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH,
                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION) == 0) {
        length -= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH;
    }

    if (length == 0 || length >= ZR_LIBRARY_MAX_PATH_LENGTH) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length; index++) {
        TZrChar current = modulePath[index];
        normalizedPath[index] = (current == '/' || current == '\\') ? ZR_SEPARATOR : current;
    }
    normalizedPath[length] = '\0';
    return ZR_TRUE;
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

    if (!library_project_normalize_module_path(modulePath, normalizedPath)) {
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
    SZrClosure *closure;
    TZrBool ignoredClosure = ZR_FALSE;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return;
    }

    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = request->function;
    request->closure = closure;
    ZrCore_Closure_InitValue(state, closure);

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &anchor);

    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
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
    status = ZrCore_Exception_TryRun(state, library_project_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return library_project_normalize_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return library_project_normalize_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    if (request.closure != ZR_NULL) {
        request.closure->function = ZR_NULL;
    }
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return ZR_THREAD_STATUS_FINE;
}

EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result) {
    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project = global->userData;
    ZrCore_Value_ResetAsNull(result);

    if (project == ZR_NULL || project->entry == ZR_NULL || global->compileSource == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }

    TZrChar entrySourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (!library_project_resolve_source_path(project, ZrCore_String_GetNativeString(project->entry), entrySourcePath)) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }

    TZrNativeString sourceCode = ZrLibrary_File_ReadAll(global, entrySourcePath);
    if (sourceCode == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }

    TZrSize sourceLength = ZrCore_NativeString_Length(sourceCode);
    SZrString *sourceName = ZrCore_String_Create(state, entrySourcePath, ZrCore_NativeString_Length(entrySourcePath));
    SZrFunction *function = global->compileSource(state, sourceCode, sourceLength, sourceName);
    TZrBool ignoredFunction = ZR_FALSE;
    EZrThreadStatus status;
    ZrCore_Memory_RawFreeWithType(global, sourceCode, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);

    if (function == ZR_NULL) {
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        }
        return library_project_normalize_failure(state, state->threadStatus);
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));

    SZrObjectModule *projectModule = ZrCore_Module_Create(state);
    if (projectModule != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, sourceName);
        ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, sourceName);
        ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, function);
    }

    status = library_project_execute_function(state, function, result);
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

    SZrLibrary_Project *project = state->global->userData;
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
