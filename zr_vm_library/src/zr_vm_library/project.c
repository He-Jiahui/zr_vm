//
// Created by HeJiahui on 2025/7/27.
//
#include "cJSON/cJSON.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
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

    if (length >= 4 && strcmp(modulePath + length - 4, ".zro") == 0) {
        length -= 4;
    } else if (length >= 3 && strcmp(modulePath + length - 3, ".zr") == 0) {
        length -= 3;
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
    return library_project_resolve_module_file(project, ZrCore_String_GetNativeString(project->source), modulePath, ".zr",
                                               resolvedPath);
}

static TZrBool library_project_resolve_binary_path(const SZrLibrary_Project *project, TZrNativeString modulePath,
                                                 TZrChar *resolvedPath) {
    return library_project_resolve_module_file(project, ZrCore_String_GetNativeString(project->binary), modulePath,
                                               ZR_LIBRARY_BINARY_FILE_EXT, resolvedPath);
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
    ZrCore_Memory_RawFreeWithType(global, sourceCode, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);

    if (function == ZR_NULL) {
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        }
        return state->threadStatus;
    }

    SZrObjectModule *projectModule = ZrCore_Module_Create(state);
    if (projectModule != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, sourceName);
        ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, sourceName);
        ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, function);
    }

    SZrClosure *closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            state->threadStatus = ZR_THREAD_STATUS_MEMORY_ERROR;
        }
        return state->threadStatus;
    }

    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    TZrStackValuePointer callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, function->stackSize + 1, callBase);

    SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;

    state->stackTop.valuePointer = callBase + 1;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_Function_Call(state, callBase, 1);

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(callBase));
    }

    return state->threadStatus;
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
