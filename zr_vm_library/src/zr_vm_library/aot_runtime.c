#include "zr_vm_library/aot_runtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct SZrLibraryAotLoadedModule {
    EZrAotBackendKind backendKind;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    TZrChar *zroPath;
    TZrChar *libraryPath;
    void *libraryHandle;
    const ZrAotCompiledModuleV1 *descriptor;
    SZrFunction *shimFunction;
    SZrObjectModule *module;
    TZrBool moduleExecuted;
} SZrLibraryAotLoadedModule;

typedef struct SZrLibraryAotRuntimeState {
    EZrLibraryProjectExecutionMode configuredExecutionMode;
    EZrLibraryExecutedVia executedVia;
    TZrBool requireAotPath;
    TZrBool strictProjectAot;
    TZrChar lastError[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrLibraryAotLoadedModule *records;
    TZrSize recordCount;
    TZrSize recordCapacity;
    SZrLibraryAotLoadedModule *activeRecord;
} SZrLibraryAotRuntimeState;

typedef struct ZrLibraryAotEntryRequest {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    SZrTypeValue *result;
    TZrBool success;
} ZrLibraryAotEntryRequest;

static SZrLibrary_Project *aot_runtime_get_project(SZrGlobalState *global);
static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_project(SZrLibrary_Project *project);
static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(SZrGlobalState *global);
static SZrLibraryAotRuntimeState *aot_runtime_ensure_state(SZrGlobalState *global);
static TZrPtr aot_runtime_reallocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);
static TZrChar *aot_runtime_duplicate_string(SZrGlobalState *global, const TZrChar *text);
static void aot_runtime_free_string(SZrGlobalState *global, TZrChar *text);
static void aot_runtime_set_error(SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...);
static void aot_runtime_fail(SZrState *state, SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...);
static TZrBool aot_runtime_normalize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize);
static TZrBool aot_runtime_resolve_module_file(const SZrLibrary_Project *project,
                                               const TZrChar *rootDirectory,
                                               const TZrChar *moduleName,
                                               const TZrChar *extension,
                                               TZrChar *buffer,
                                               TZrSize bufferSize);
static void aot_runtime_sanitize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize);
static const TZrChar *aot_runtime_dynamic_library_extension(void);
static TZrBool aot_runtime_resolve_library_path(const SZrLibrary_Project *project,
                                                EZrAotBackendKind backendKind,
                                                const TZrChar *moduleName,
                                                TZrChar *buffer,
                                                TZrSize bufferSize);
static TZrBool aot_runtime_hash_file(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize);
static void *aot_runtime_open_library(const TZrChar *path);
static void aot_runtime_close_library(void *handle);
static TZrPtr aot_runtime_find_symbol(void *handle, const TZrChar *symbolName);
static FZrVmGetAotCompiledModuleV1 aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer);
static SZrLibraryAotLoadedModule *aot_runtime_find_record(SZrLibraryAotRuntimeState *runtimeState,
                                                          EZrAotBackendKind backendKind,
                                                          const TZrChar *moduleName);
static TZrBool aot_runtime_append_record(SZrGlobalState *global,
                                         SZrLibraryAotRuntimeState *runtimeState,
                                         const SZrLibraryAotLoadedModule *record,
                                         SZrLibraryAotLoadedModule **outRecord);
static TZrBool aot_runtime_load_zro_function(SZrState *state, const TZrChar *zroPath, SZrFunction **outFunction);
static TZrBool aot_runtime_execute_vm_shim_direct(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrStackValuePointer *outResultBase);
static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer resultBase);
static TZrBool aot_runtime_call_record_direct(SZrState *state,
                                              SZrLibraryAotRuntimeState *runtimeState,
                                              SZrLibraryAotLoadedModule *record,
                                              TZrBool captureResult,
                                              SZrTypeValue *result);
static EZrLibraryExecutedVia aot_runtime_backend_to_executed_via(EZrAotBackendKind backendKind);
static TZrBool aot_runtime_prepare_record(SZrState *state,
                                          SZrLibraryAotRuntimeState *runtimeState,
                                          EZrAotBackendKind backendKind,
                                          const TZrChar *moduleName,
                                          SZrLibraryAotLoadedModule **outRecord);
static void aot_runtime_execute_entry_body(SZrState *state, TZrPtr arguments);

static SZrLibrary_Project *aot_runtime_get_project(SZrGlobalState *global) {
    return (global != ZR_NULL && global->userData != ZR_NULL) ? (SZrLibrary_Project *)global->userData : ZR_NULL;
}

static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_project(SZrLibrary_Project *project) {
    return project != ZR_NULL ? (SZrLibraryAotRuntimeState *)project->aotRuntime : ZR_NULL;
}

static SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(SZrGlobalState *global) {
    return aot_runtime_get_state_from_project(aot_runtime_get_project(global));
}

static TZrPtr aot_runtime_reallocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize) {
    return (global == ZR_NULL || global->allocator == ZR_NULL)
                   ? ZR_NULL
                   : global->allocator(global->userAllocationArguments,
                                       pointer,
                                       originalSize,
                                       newSize,
                                       ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static TZrChar *aot_runtime_duplicate_string(SZrGlobalState *global, const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (global == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *)aot_runtime_reallocate(global, ZR_NULL, 0, length + 1);
    if (copy != ZR_NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static void aot_runtime_free_string(SZrGlobalState *global, TZrChar *text) {
    if (global != ZR_NULL && text != ZR_NULL) {
        aot_runtime_reallocate(global, text, strlen(text) + 1, 0);
    }
}

static void aot_runtime_set_error(SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...) {
    va_list arguments;

    if (runtimeState == ZR_NULL) {
        return;
    }

    runtimeState->lastError[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(runtimeState->lastError, sizeof(runtimeState->lastError), format, arguments);
    va_end(arguments);
}

static void aot_runtime_fail(SZrState *state, SZrLibraryAotRuntimeState *runtimeState, const TZrChar *format, ...) {
    va_list arguments;

    if (runtimeState != ZR_NULL) {
        runtimeState->lastError[0] = '\0';
        if (format != ZR_NULL) {
            va_start(arguments, format);
            vsnprintf(runtimeState->lastError, sizeof(runtimeState->lastError), format, arguments);
            va_end(arguments);
        }
    }

    if (state != ZR_NULL && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
}

static SZrLibraryAotRuntimeState *aot_runtime_ensure_state(SZrGlobalState *global) {
    SZrLibrary_Project *project;
    SZrLibraryAotRuntimeState *runtimeState;

    project = aot_runtime_get_project(global);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeState = aot_runtime_get_state_from_project(project);
    if (runtimeState != ZR_NULL) {
        return runtimeState;
    }

    runtimeState = (SZrLibraryAotRuntimeState *)aot_runtime_reallocate(global, ZR_NULL, 0, sizeof(*runtimeState));
    if (runtimeState == ZR_NULL) {
        return ZR_NULL;
    }

    memset(runtimeState, 0, sizeof(*runtimeState));
    runtimeState->configuredExecutionMode = ZR_LIBRARY_PROJECT_EXECUTION_MODE_INTERP;
    runtimeState->executedVia = ZR_LIBRARY_EXECUTED_VIA_NONE;
    project->aotRuntime = runtimeState;
    return runtimeState;
}

/* remaining helpers and exported functions are appended below */
static TZrBool aot_runtime_normalize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length;
    TZrSize writeIndex = 0;

    if (moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(moduleName);
    while (length > 0 && (moduleName[length - 1] == '/' || moduleName[length - 1] == '\\')) {
        length--;
    }
    if (length >= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH &&
        memcmp(moduleName + length - ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION,
               ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH) == 0) {
        length -= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH;
    } else if (length >= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH &&
               memcmp(moduleName + length - ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH,
                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                      ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH) == 0) {
        length -= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH;
    }

    while (length > 0 && (*moduleName == '/' || *moduleName == '\\')) {
        moduleName++;
        length--;
    }
    if (length == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length && writeIndex + 1 < bufferSize; index++) {
        buffer[writeIndex++] = moduleName[index] == '\\' ? '/' : moduleName[index];
    }
    if (writeIndex == 0 || writeIndex + 1 > bufferSize) {
        return ZR_FALSE;
    }
    buffer[writeIndex] = '\0';
    return ZR_TRUE;
}

static TZrBool aot_runtime_resolve_module_file(const SZrLibrary_Project *project,
                                               const TZrChar *rootDirectory,
                                               const TZrChar *moduleName,
                                               const TZrChar *extension,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;

    if (project == ZR_NULL || rootDirectory == ZR_NULL || moduleName == ZR_NULL || extension == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(ZrCore_String_GetNativeString(project->directory), rootDirectory, rootPath);
    for (TZrSize index = 0; moduleName[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = moduleName[index] == '/' ? ZR_SEPARATOR : moduleName[index];
    }
    relativePath[writeIndex] = '\0';

    return snprintf(buffer, bufferSize, "%s%c%s%s", rootPath, ZR_SEPARATOR, relativePath, extension) < (int)bufferSize;
}

static void aot_runtime_sanitize_module_name(const TZrChar *moduleName, TZrChar *buffer, TZrSize bufferSize) {
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

static const TZrChar *aot_runtime_dynamic_library_extension(void) {
#if defined(ZR_PLATFORM_WIN)
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static TZrBool aot_runtime_resolve_library_path(const SZrLibrary_Project *project,
                                                EZrAotBackendKind backendKind,
                                                const TZrChar *moduleName,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar backendRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *libraryExtension;

    if (project == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    aot_runtime_sanitize_module_name(moduleName, sanitizedModuleName, sizeof(sanitizedModuleName));
    if (sanitizedModuleName[0] == '\0') {
        return ZR_FALSE;
    }

    if (backendKind == ZR_AOT_BACKEND_KIND_C) {
        snprintf(backendRoot, sizeof(backendRoot), "%s%c%s%c%s",
                 ZrCore_String_GetNativeString(project->binary),
                 ZR_SEPARATOR,
                 "aot_c",
                 ZR_SEPARATOR,
                 "lib");
    } else if (backendKind == ZR_AOT_BACKEND_KIND_LLVM) {
        snprintf(backendRoot, sizeof(backendRoot), "%s%c%s%c%s",
                 ZrCore_String_GetNativeString(project->binary),
                 ZR_SEPARATOR,
                 "aot_llvm",
                 ZR_SEPARATOR,
                 "lib");
    } else {
        return ZR_FALSE;
    }

    libraryExtension = aot_runtime_dynamic_library_extension();
    return snprintf(buffer,
                    bufferSize,
                    "%s%c%s%c%s%s%s",
                    ZrCore_String_GetNativeString(project->directory),
                    ZR_SEPARATOR,
                    backendRoot,
                    ZR_SEPARATOR,
                    "zrvm_aot_",
                    sanitizedModuleName,
                    libraryExtension) < (int)bufferSize;
}

static TZrBool aot_runtime_hash_file(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        for (TZrSize index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    fclose(file);
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
    return ZR_TRUE;
}

static void *aot_runtime_open_library(const TZrChar *path) {
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void aot_runtime_close_library(void *handle) {
    if (handle == ZR_NULL) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

static TZrPtr aot_runtime_find_symbol(void *handle, const TZrChar *symbolName) {
    if (handle == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }
#if defined(ZR_PLATFORM_WIN)
    return (TZrPtr)GetProcAddress((HMODULE)handle, symbolName);
#else
    return (TZrPtr)dlsym(handle, symbolName);
#endif
}

static FZrVmGetAotCompiledModuleV1 aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer) {
    FZrVmGetAotCompiledModuleV1 symbol = ZR_NULL;
    if (symbolPointer != ZR_NULL) {
        memcpy(&symbol, &symbolPointer, sizeof(symbol));
    }
    return symbol;
}

static SZrLibraryAotLoadedModule *aot_runtime_find_record(SZrLibraryAotRuntimeState *runtimeState,
                                                          EZrAotBackendKind backendKind,
                                                          const TZrChar *moduleName) {
    if (runtimeState == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < runtimeState->recordCount; index++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[index];
        if (record->backendKind == backendKind &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, moduleName) == 0) {
            return record;
        }
    }
    return ZR_NULL;
}

static TZrBool aot_runtime_append_record(SZrGlobalState *global,
                                         SZrLibraryAotRuntimeState *runtimeState,
                                         const SZrLibraryAotLoadedModule *record,
                                         SZrLibraryAotLoadedModule **outRecord) {
    SZrLibraryAotLoadedModule *newRecords;
    TZrSize newCapacity;

    if (global == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    if (runtimeState->recordCount == runtimeState->recordCapacity) {
        newCapacity = runtimeState->recordCapacity == 0 ? 4 : runtimeState->recordCapacity * 2;
        newRecords = (SZrLibraryAotLoadedModule *)aot_runtime_reallocate(global,
                                                                         runtimeState->records,
                                                                         runtimeState->recordCapacity * sizeof(*runtimeState->records),
                                                                         newCapacity * sizeof(*runtimeState->records));
        if (newRecords == ZR_NULL) {
            return ZR_FALSE;
        }
        runtimeState->records = newRecords;
        runtimeState->recordCapacity = newCapacity;
    }

    runtimeState->records[runtimeState->recordCount] = *record;
    if (outRecord != ZR_NULL) {
        *outRecord = &runtimeState->records[runtimeState->recordCount];
    }
    runtimeState->recordCount++;
    return ZR_TRUE;
}

static TZrBool aot_runtime_load_zro_function(SZrState *state, const TZrChar *zroPath, SZrFunction **outFunction) {
    SZrLibrary_File_Reader *reader;
    SZrIo io;
    SZrIoSource *ioSource;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || zroPath == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)zroPath, ZR_TRUE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, &io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io.isBinary = ZR_TRUE;
    ioSource = ZrCore_Io_ReadSourceNew(&io);
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }
    if (ioSource == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    ZrCore_Io_ReadSourceFree(state->global, ioSource);
    return *outFunction != ZR_NULL;
}

static TZrBool aot_runtime_execute_vm_shim_direct(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrStackValuePointer *outResultBase) {
    SZrClosure *closure;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (outResultBase != ZR_NULL) {
        *outResultBase = ZR_NULL;
    }
    if (state == ZR_NULL || function == ZR_NULL || outResultBase == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, function->stackSize + 1, base, base, &anchor);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    *outResultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    return state->threadStatus == ZR_THREAD_STATUS_FINE && *outResultBase != ZR_NULL;
}

static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer resultBase) {
    TZrStackValuePointer exportedValuesTop;

    if (state == ZR_NULL || record == ZR_NULL || record->module == ZR_NULL || record->shimFunction == ZR_NULL ||
        resultBase == ZR_NULL) {
        return ZR_FALSE;
    }

    if (record->shimFunction->exportedVariables == ZR_NULL || record->shimFunction->exportedVariableLength == 0) {
        record->moduleExecuted = ZR_TRUE;
        return ZR_TRUE;
    }

    exportedValuesTop = resultBase + 1 + record->shimFunction->stackSize;
    for (TZrUInt32 index = 0; index < record->shimFunction->exportedVariableLength; index++) {
        struct SZrFunctionExportedVariable *exportVar = &record->shimFunction->exportedVariables[index];
        TZrStackValuePointer varPointer;
        SZrTypeValue *varValue;

        if (exportVar->name == ZR_NULL) {
            continue;
        }

        varPointer = resultBase + 1 + exportVar->stackSlot;
        if (varPointer >= exportedValuesTop) {
            continue;
        }

        varValue = ZrCore_Stack_GetValue(varPointer);
        if (varValue == ZR_NULL) {
            continue;
        }

        if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
            ZrCore_Module_AddPubExport(state, record->module, exportVar->name, varValue);
        } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
            ZrCore_Module_AddProExport(state, record->module, exportVar->name, varValue);
        }
    }

    record->moduleExecuted = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool aot_runtime_call_record_direct(SZrState *state,
                                              SZrLibraryAotRuntimeState *runtimeState,
                                              SZrLibraryAotLoadedModule *record,
                                              TZrBool captureResult,
                                              SZrTypeValue *result) {
    SZrClosureNative *nativeClosure;
    SZrLibraryAotLoadedModule *savedRecord;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;
    TZrStackValuePointer resultBase;

    if (state == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL || record->descriptor == ZR_NULL ||
        record->descriptor->entryThunk == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    if (nativeClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeClosure->nativeFunction = record->descriptor->entryThunk;
    nativeClosure->aotShimFunction = record->shimFunction;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, 1, base, base, &anchor);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_TRUE;
    state->stackTop.valuePointer++;

    savedRecord = runtimeState->activeRecord;
    runtimeState->activeRecord = record;
    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, captureResult ? 1 : 0);
    runtimeState->activeRecord = savedRecord;

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (captureResult) {
        if (result == ZR_NULL || resultBase == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(resultBase));
    }
    return ZR_TRUE;
}

static EZrLibraryExecutedVia aot_runtime_backend_to_executed_via(EZrAotBackendKind backendKind) {
    switch (backendKind) {
        case ZR_AOT_BACKEND_KIND_C:
            return ZR_LIBRARY_EXECUTED_VIA_AOT_C;
        case ZR_AOT_BACKEND_KIND_LLVM:
            return ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM;
        case ZR_AOT_BACKEND_KIND_NONE:
        default:
            return ZR_LIBRARY_EXECUTED_VIA_NONE;
    }
}

static TZrBool aot_runtime_prepare_record(SZrState *state,
                                          SZrLibraryAotRuntimeState *runtimeState,
                                          EZrAotBackendKind backendKind,
                                          const TZrChar *moduleName,
                                          SZrLibraryAotLoadedModule **outRecord) {
    SZrGlobalState *global;
    SZrLibrary_Project *project;
    SZrLibraryAotLoadedModule *existing;
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar zroPath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar libraryPath[ZR_LIBRARY_MAX_PATH_LENGTH] = {0};
    TZrChar sourceHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrBool isProjectLocal;
    void *handle;
    FZrVmGetAotCompiledModuleV1 descriptorSymbol;
    const ZrAotCompiledModuleV1 *descriptor;
    SZrFunction *shimFunction;
    SZrLibraryAotLoadedModule record;
    SZrString *moduleNameString;

    if (outRecord != ZR_NULL) {
        *outRecord = ZR_NULL;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || runtimeState == ZR_NULL || moduleName == ZR_NULL ||
        outRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    project = aot_runtime_get_project(global);
    if (project == ZR_NULL || !aot_runtime_normalize_module_name(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    existing = aot_runtime_find_record(runtimeState, backendKind, normalizedModule);
    if (existing != ZR_NULL) {
        *outRecord = existing;
        return ZR_TRUE;
    }

    isProjectLocal =
            aot_runtime_resolve_module_file(project,
                                            ZrCore_String_GetNativeString(project->source),
                                            normalizedModule,
                                            ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                            sourcePath,
                                            sizeof(sourcePath)) &&
            ZrLibrary_File_Exist(sourcePath) == ZR_LIBRARY_FILE_IS_FILE;
    if (!isProjectLocal) {
        isProjectLocal =
                aot_runtime_resolve_module_file(project,
                                                ZrCore_String_GetNativeString(project->binary),
                                                normalizedModule,
                                                ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                zroPath,
                                                sizeof(zroPath)) &&
                ZrLibrary_File_Exist(zroPath) == ZR_LIBRARY_FILE_IS_FILE;
    } else {
        aot_runtime_resolve_module_file(project,
                                        ZrCore_String_GetNativeString(project->binary),
                                        normalizedModule,
                                        ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                        zroPath,
                                        sizeof(zroPath));
    }

    if (!isProjectLocal) {
        return ZR_FALSE;
    }

    if (ZrLibrary_File_Exist(zroPath) != ZR_LIBRARY_FILE_IS_FILE ||
        !aot_runtime_resolve_library_path(project, backendKind, normalizedModule, libraryPath, sizeof(libraryPath)) ||
        ZrLibrary_File_Exist(libraryPath) != ZR_LIBRARY_FILE_IS_FILE) {
        aot_runtime_fail(state, runtimeState, "missing AOT artifacts for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    handle = aot_runtime_open_library(libraryPath);
    if (handle == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "failed to load AOT library '%s'", libraryPath);
        return ZR_FALSE;
    }

    descriptorSymbol = aot_runtime_cast_descriptor_symbol(aot_runtime_find_symbol(handle, "ZrVm_GetAotCompiledModule_v1"));
    if (descriptorSymbol == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT library '%s' is missing ZrVm_GetAotCompiledModule_v1", libraryPath);
        return ZR_FALSE;
    }

    descriptor = descriptorSymbol();
    if (descriptor == ZR_NULL || descriptor->abiVersion != ZR_VM_AOT_ABI_VERSION ||
        (EZrAotBackendKind)descriptor->backendKind != backendKind ||
        descriptor->moduleName == ZR_NULL ||
        strcmp(descriptor->moduleName, normalizedModule) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT descriptor validation failed for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (sourcePath[0] != '\0' && aot_runtime_hash_file(sourcePath, sourceHash, sizeof(sourceHash)) &&
        descriptor->sourceHash != ZR_NULL && descriptor->sourceHash[0] != '\0' &&
        strcmp(descriptor->sourceHash, sourceHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT source hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (!aot_runtime_hash_file(zroPath, zroHash, sizeof(zroHash)) ||
        descriptor->zroHash == ZR_NULL ||
        strcmp(descriptor->zroHash, zroHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT zro hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (!aot_runtime_load_zro_function(state, zroPath, &shimFunction)) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to load companion zro for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    memset(&record, 0, sizeof(record));
    record.backendKind = backendKind;
    record.moduleName = aot_runtime_duplicate_string(global, normalizedModule);
    record.sourcePath = sourcePath[0] != '\0' ? aot_runtime_duplicate_string(global, sourcePath) : ZR_NULL;
    record.zroPath = aot_runtime_duplicate_string(global, zroPath);
    record.libraryPath = aot_runtime_duplicate_string(global, libraryPath);
    record.libraryHandle = handle;
    record.descriptor = descriptor;
    record.shimFunction = shimFunction;
    record.module = ZrCore_Module_Create(state);
    if (record.moduleName == ZR_NULL || record.zroPath == ZR_NULL || record.libraryPath == ZR_NULL || record.module == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        aot_runtime_fail(state, runtimeState, "failed to allocate AOT runtime record for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    moduleNameString = ZrCore_String_CreateFromNative(state, normalizedModule);
    if (moduleNameString != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, moduleNameString);
        ZrCore_Module_SetInfo(state, record.module, moduleNameString, pathHash, moduleNameString);
    }
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, record.module, record.shimFunction);
    ZrCore_Module_CreatePrototypesFromConstants(state, record.module, record.shimFunction);

    if (!aot_runtime_append_record(global, runtimeState, &record, outRecord)) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to store AOT runtime record for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void aot_runtime_execute_entry_body(SZrState *state, TZrPtr arguments) {
    ZrLibraryAotEntryRequest *request = (ZrLibraryAotEntryRequest *)arguments;

    if (request == ZR_NULL) {
        return;
    }
    request->success =
            aot_runtime_call_record_direct(state, request->runtimeState, request->record, ZR_TRUE, request->result);
}

TZrBool ZrLibrary_AotRuntime_ConfigureGlobal(SZrGlobalState *global,
                                             EZrLibraryProjectExecutionMode executionMode,
                                             TZrBool requireAotPath) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_ensure_state(global);

    if (runtimeState == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState->configuredExecutionMode = executionMode;
    runtimeState->requireAotPath = requireAotPath;
    runtimeState->strictProjectAot = executionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C ||
                                     executionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM;
    runtimeState->executedVia = ZR_LIBRARY_EXECUTED_VIA_NONE;
    runtimeState->activeRecord = ZR_NULL;
    aot_runtime_set_error(runtimeState, ZR_NULL);
    ZrCore_GlobalState_SetAotModuleLoader(global,
                                          runtimeState->strictProjectAot ? ZrLibrary_AotRuntime_ModuleLoader : ZR_NULL,
                                          runtimeState->strictProjectAot ? runtimeState : ZR_NULL);
    return ZR_TRUE;
}

void ZrLibrary_AotRuntime_FreeProjectState(SZrState *state, SZrLibrary_Project *project) {
    SZrGlobalState *global;
    SZrLibraryAotRuntimeState *runtimeState;

    if (state == ZR_NULL || project == ZR_NULL) {
        return;
    }

    global = state->global;
    runtimeState = aot_runtime_get_state_from_project(project);
    if (runtimeState == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < runtimeState->recordCount; index++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[index];
        aot_runtime_close_library(record->libraryHandle);
        aot_runtime_free_string(global, record->moduleName);
        aot_runtime_free_string(global, record->sourcePath);
        aot_runtime_free_string(global, record->zroPath);
        aot_runtime_free_string(global, record->libraryPath);
    }
    if (runtimeState->records != ZR_NULL) {
        aot_runtime_reallocate(global,
                               runtimeState->records,
                               runtimeState->recordCapacity * sizeof(*runtimeState->records),
                               0);
    }
    aot_runtime_reallocate(global, runtimeState, sizeof(*runtimeState), 0);
    project->aotRuntime = ZR_NULL;
}

const TZrChar *ZrLibrary_AotRuntime_ExecutedViaName(EZrLibraryExecutedVia executedVia) {
    switch (executedVia) {
        case ZR_LIBRARY_EXECUTED_VIA_INTERP:
            return "interp";
        case ZR_LIBRARY_EXECUTED_VIA_BINARY:
            return "binary";
        case ZR_LIBRARY_EXECUTED_VIA_AOT_C:
            return "aot_c";
        case ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM:
            return "aot_llvm";
        case ZR_LIBRARY_EXECUTED_VIA_NONE:
        default:
            return "none";
    }
}

EZrLibraryExecutedVia ZrLibrary_AotRuntime_GetExecutedVia(SZrGlobalState *global) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_get_state_from_global(global);
    return runtimeState != ZR_NULL ? runtimeState->executedVia : ZR_LIBRARY_EXECUTED_VIA_NONE;
}

const TZrChar *ZrLibrary_AotRuntime_GetLastError(SZrGlobalState *global) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_get_state_from_global(global);
    if (runtimeState == ZR_NULL || runtimeState->lastError[0] == '\0') {
        return ZR_NULL;
    }
    return runtimeState->lastError;
}

SZrObjectModule *ZrLibrary_AotRuntime_ModuleLoader(SZrState *state, SZrString *moduleName, TZrPtr userData) {
    SZrLibraryAotRuntimeState *runtimeState = (SZrLibraryAotRuntimeState *)userData;
    EZrAotBackendKind backendKind;
    SZrLibraryAotLoadedModule *record = ZR_NULL;

    if (state == ZR_NULL || runtimeState == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    backendKind = runtimeState->configuredExecutionMode == ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM
                          ? ZR_AOT_BACKEND_KIND_LLVM
                          : ZR_AOT_BACKEND_KIND_C;

    if (!aot_runtime_prepare_record(state,
                                    runtimeState,
                                    backendKind,
                                    ZrCore_String_GetNativeString(moduleName),
                                    &record) ||
        record == ZR_NULL) {
        return ZR_NULL;
    }

    if (!record->moduleExecuted &&
        !aot_runtime_call_record_direct(state, runtimeState, record, ZR_FALSE, ZR_NULL)) {
        return ZR_NULL;
    }

    return record->module;
}

TZrBool ZrLibrary_AotRuntime_ExecuteEntry(SZrState *state,
                                          EZrAotBackendKind backendKind,
                                          SZrTypeValue *result) {
    SZrLibrary_Project *project;
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record = ZR_NULL;
    ZrLibraryAotEntryRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    project = aot_runtime_get_project(state->global);
    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (project == ZR_NULL || project->entry == ZR_NULL || runtimeState == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_record(state,
                                    runtimeState,
                                    backendKind,
                                    ZrCore_String_GetNativeString(project->entry),
                                    &record) ||
        record == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    request.runtimeState = runtimeState;
    request.record = record;
    request.result = result;
    ZrCore_Value_ResetAsNull(result);

    status = ZrCore_Exception_TryRun(state, aot_runtime_execute_entry_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        state->threadStatus = status;
        return ZR_FALSE;
    }
    return request.success;
}

TZrInt64 ZrLibrary_AotRuntime_InvokeActiveShim(SZrState *state, EZrAotBackendKind backendKind) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    TZrStackValuePointer resultBase = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (runtimeState == ZR_NULL || runtimeState->activeRecord == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT entry thunk invoked without an active record");
        return 0;
    }

    record = runtimeState->activeRecord;
    if (record->backendKind != backendKind) {
        aot_runtime_fail(state, runtimeState, "AOT backend mismatch for module '%s'", record->moduleName);
        return 0;
    }

    runtimeState->executedVia = aot_runtime_backend_to_executed_via(backendKind);
    if (!aot_runtime_execute_vm_shim_direct(state, record->shimFunction, &resultBase)) {
        return 0;
    }

    if (!record->moduleExecuted) {
        aot_runtime_materialize_exports(state, record, resultBase);
    }
    return 1;
}
