#include "zr_vm_library/aot_runtime.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
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
    const ZrAotCompiledModule *descriptor;
    SZrFunction *moduleFunction;
    SZrFunction **functionTable;
    TZrUInt32 functionCount;
    TZrUInt32 functionCapacity;
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
static FZrVmGetAotCompiledModule aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer);
static SZrLibraryAotLoadedModule *aot_runtime_find_record(SZrLibraryAotRuntimeState *runtimeState,
                                                          EZrAotBackendKind backendKind,
                                                          const TZrChar *moduleName);
static TZrBool aot_runtime_append_record(SZrGlobalState *global,
                                         SZrLibraryAotRuntimeState *runtimeState,
                                         const SZrLibraryAotLoadedModule *record,
                                         SZrLibraryAotLoadedModule **outRecord);
static TZrBool aot_runtime_load_zro_function(SZrState *state, const TZrChar *zroPath, SZrFunction **outFunction);
static TZrBool aot_runtime_load_embedded_function(SZrState *state,
                                                  const TZrByte *blob,
                                                  TZrSize blobLength,
                                                  SZrFunction **outFunction);
static TZrBool aot_runtime_build_function_table(SZrState *state,
                                                SZrFunction *function,
                                                SZrFunction ***outFunctions,
                                                TZrUInt32 *outCount,
                                                TZrUInt32 *outCapacity);
static TZrBool aot_runtime_materialize_callable_constant(SZrState *state,
                                                         SZrLibraryAotLoadedModule *record,
                                                         const SZrTypeValue *source,
                                                         TZrBool forceClosure,
                                                         SZrTypeValue *destination);
static TZrUInt32 aot_runtime_find_function_index_in_record(const SZrLibraryAotLoadedModule *record,
                                                           const SZrFunction *function);
static SZrLibraryAotLoadedModule *aot_runtime_find_record_for_function(SZrLibraryAotRuntimeState *runtimeState,
                                                                       const SZrFunction *function);
static const SZrFunction *aot_runtime_frame_function(const ZrAotGeneratedFrame *frame);
static TZrStackValuePointer aot_runtime_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex);
static TZrBool aot_runtime_resolve_member_symbol(const SZrFunction *function,
                                                 TZrUInt32 memberId,
                                                 SZrString **outSymbol);
static TZrBool aot_runtime_execute_vm_shim_direct(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrStackValuePointer *outResultBase);
static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer slotBase);
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

static FZrVmGetAotCompiledModule aot_runtime_cast_descriptor_symbol(TZrPtr symbolPointer) {
    FZrVmGetAotCompiledModule symbol = ZR_NULL;
    if (symbolPointer != ZR_NULL) {
        memcpy(&symbol, &symbolPointer, sizeof(symbol));
    }
    return symbol;
}

typedef struct SZrAotRuntimeBlobReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrAotRuntimeBlobReader;

static TZrBytePtr aot_runtime_blob_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrAotRuntimeBlobReader *reader = (SZrAotRuntimeBlobReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL || reader->length == 0) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void aot_runtime_blob_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
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

static TZrBool aot_runtime_load_embedded_function(SZrState *state,
                                                  const TZrByte *blob,
                                                  TZrSize blobLength,
                                                  SZrFunction **outFunction) {
    SZrAotRuntimeBlobReader reader;
    SZrIo io;
    SZrIoSource *ioSource;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || blob == ZR_NULL || blobLength == 0 || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&reader, 0, sizeof(reader));
    reader.bytes = blob;
    reader.length = blobLength;

    ZrCore_Io_Init(state, &io, aot_runtime_blob_reader_read, aot_runtime_blob_reader_close, &reader);
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

static SZrFunction *aot_runtime_function_from_constant_value(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, value->value.object);
    }

    if (value->type == ZR_VALUE_TYPE_CLOSURE &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrUInt32 aot_runtime_count_function_graph_capacity(SZrState *state, const SZrFunction *function) {
    TZrUInt32 count = 1;

    if (state == ZR_NULL || function == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrFunction *constantFunction =
                aot_runtime_function_from_constant_value(state, &function->constantValueList[index]);
        if (constantFunction != ZR_NULL) {
            count += aot_runtime_count_function_graph_capacity(state, constantFunction);
        }
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        count += aot_runtime_count_function_graph_capacity(state, &function->childFunctionList[index]);
    }
    return count;
}

static TZrBool aot_runtime_functions_equivalent(const SZrFunction *left, const SZrFunction *right) {
    TZrBool sameFunctionName;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    sameFunctionName = left->functionName == right->functionName ||
                       (left->functionName == ZR_NULL && right->functionName == ZR_NULL) ||
                       (left->functionName != ZR_NULL && right->functionName != ZR_NULL &&
                        ZrCore_String_Equal(left->functionName, right->functionName));

    return sameFunctionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static TZrBool aot_runtime_function_table_contains(SZrFunction *const *functions,
                                                   TZrUInt32 count,
                                                   const SZrFunction *function) {
    if (functions == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < count; index++) {
        if (functions[index] == function || aot_runtime_functions_equivalent(functions[index], function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void aot_runtime_flatten_function_graph(SZrState *state,
                                               SZrFunction *function,
                                               SZrFunction **functions,
                                               TZrUInt32 capacity,
                                               TZrUInt32 *ioIndex) {
    if (state == ZR_NULL || function == ZR_NULL || functions == ZR_NULL || ioIndex == ZR_NULL) {
        return;
    }

    if (aot_runtime_function_table_contains(functions, *ioIndex, function)) {
        return;
    }
    if (*ioIndex >= capacity) {
        return;
    }

    functions[*ioIndex] = function;
    (*ioIndex)++;
    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrFunction *constantFunction =
                aot_runtime_function_from_constant_value(state, &function->constantValueList[index]);
        if (constantFunction != ZR_NULL) {
            aot_runtime_flatten_function_graph(state, constantFunction, functions, capacity, ioIndex);
        }
    }
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        aot_runtime_flatten_function_graph(state, &function->childFunctionList[index], functions, capacity, ioIndex);
    }
}

static TZrBool aot_runtime_build_function_table(SZrState *state,
                                                SZrFunction *function,
                                                SZrFunction ***outFunctions,
                                                TZrUInt32 *outCount,
                                                TZrUInt32 *outCapacity) {
    SZrFunction **functions;
    TZrUInt32 capacity;
    TZrUInt32 writeIndex = 0;

    if (outFunctions != ZR_NULL) {
        *outFunctions = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outCapacity != ZR_NULL) {
        *outCapacity = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outFunctions == ZR_NULL ||
        outCount == ZR_NULL || outCapacity == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_RebindConstantFunctionValuesToChildren(function);
    capacity = aot_runtime_count_function_graph_capacity(state, function);
    if (capacity == 0) {
        return ZR_TRUE;
    }

    functions = (SZrFunction **)aot_runtime_reallocate(state->global, ZR_NULL, 0, sizeof(*functions) * capacity);
    if (functions == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(functions, 0, sizeof(*functions) * capacity);
    aot_runtime_flatten_function_graph(state, function, functions, capacity, &writeIndex);
    *outFunctions = functions;
    *outCount = writeIndex;
    *outCapacity = capacity;
    return ZR_TRUE;
}

static TZrUInt32 aot_runtime_find_function_index_in_record(const SZrLibraryAotLoadedModule *record,
                                                           const SZrFunction *function) {
    if (record == ZR_NULL || function == ZR_NULL || record->functionTable == ZR_NULL) {
        return UINT32_MAX;
    }

    for (TZrUInt32 index = 0; index < record->functionCount; index++) {
        if (record->functionTable[index] == function) {
            return index;
        }
    }
    return UINT32_MAX;
}

static SZrLibraryAotLoadedModule *aot_runtime_find_record_for_function(SZrLibraryAotRuntimeState *runtimeState,
                                                                       const SZrFunction *function) {
    if (runtimeState == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize recordIndex = 0; recordIndex < runtimeState->recordCount; recordIndex++) {
        SZrLibraryAotLoadedModule *record = &runtimeState->records[recordIndex];
        if (aot_runtime_find_function_index_in_record(record, function) != UINT32_MAX) {
            return record;
        }
    }
    return ZR_NULL;
}

static const SZrFunction *aot_runtime_frame_function(const ZrAotGeneratedFrame *frame) {
    return frame != ZR_NULL ? frame->function : ZR_NULL;
}

static TZrStackValuePointer aot_runtime_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_NULL;
    }

    if (frame->function == ZR_NULL || slotIndex >= frame->function->stackSize) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

static TZrBool aot_runtime_resolve_member_symbol(const SZrFunction *function,
                                                 TZrUInt32 memberId,
                                                 SZrString **outSymbol) {
    if (outSymbol != ZR_NULL) {
        *outSymbol = ZR_NULL;
    }
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberId >= function->memberEntryLength ||
        outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSymbol = function->memberEntries[memberId].symbol;
    return *outSymbol != ZR_NULL;
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

static TZrBool aot_runtime_materialize_callable_constant(SZrState *state,
                                                         SZrLibraryAotLoadedModule *record,
                                                         const SZrTypeValue *source,
                                                         TZrBool forceClosure,
                                                         SZrTypeValue *destination) {
    SZrFunction *metadataFunction;
    TZrUInt32 functionIndex;
    SZrClosureNative *closure;
    SZrLibraryAotRuntimeState *runtimeState;

    if (state == ZR_NULL || record == ZR_NULL || source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState = state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, source);
    if (metadataFunction == ZR_NULL) {
        ZrCore_Value_Copy(state, destination, source);
        return ZR_TRUE;
    }

    if (record->descriptor == ZR_NULL || record->descriptor->functionThunks == ZR_NULL ||
        record->descriptor->functionThunkCount == 0) {
        ZrCore_Value_Copy(state, destination, source);
        return ZR_TRUE;
    }

    functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->descriptor->functionThunkCount ||
        record->descriptor->functionThunks[functionIndex] == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT callable thunk missing for module '%s' function index %u",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                         (unsigned)functionIndex);
        return ZR_FALSE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = record->descriptor->functionThunks[functionIndex];
    closure->aotShimFunction = metadataFunction;
    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    destination->type = forceClosure ? ZR_VALUE_TYPE_CLOSURE : ZR_VALUE_TYPE_CLOSURE;
    destination->isGarbageCollectable = ZR_TRUE;
    destination->isNative = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool aot_runtime_materialize_exports(SZrState *state,
                                               SZrLibraryAotLoadedModule *record,
                                               TZrStackValuePointer slotBase) {
    TZrStackValuePointer exportedValuesTop;

    if (state == ZR_NULL || record == ZR_NULL || record->module == ZR_NULL || record->moduleFunction == ZR_NULL ||
        slotBase == ZR_NULL) {
        return ZR_FALSE;
    }

    if (record->moduleFunction->exportedVariables == ZR_NULL || record->moduleFunction->exportedVariableLength == 0) {
        record->moduleExecuted = ZR_TRUE;
        return ZR_TRUE;
    }

    exportedValuesTop = slotBase + record->moduleFunction->stackSize;
    for (TZrUInt32 index = 0; index < record->moduleFunction->exportedVariableLength; index++) {
        struct SZrFunctionExportedVariable *exportVar = &record->moduleFunction->exportedVariables[index];
        TZrStackValuePointer varPointer;
        SZrTypeValue *varValue;
        SZrTypeValue publishedValue;

        if (exportVar->name == ZR_NULL) {
            continue;
        }

        varPointer = slotBase + exportVar->stackSlot;
        if (varPointer >= exportedValuesTop) {
            continue;
        }

        varValue = ZrCore_Stack_GetValue(varPointer);
        if (varValue == ZR_NULL) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&publishedValue);
        if (!aot_runtime_materialize_callable_constant(state, record, varValue, ZR_TRUE, &publishedValue)) {
            return ZR_FALSE;
        }

        if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
            ZrCore_Module_AddPubExport(state, record->module, exportVar->name, &publishedValue);
        } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
            ZrCore_Module_AddProExport(state, record->module, exportVar->name, &publishedValue);
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

    FZrAotEntryThunk entryThunk = ZR_NULL;

    if (record != ZR_NULL && record->descriptor != ZR_NULL) {
        entryThunk = record->descriptor->entryThunk;
    }

    if (state == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL || entryThunk == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    if (nativeClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeClosure->nativeFunction = entryThunk;
    nativeClosure->aotShimFunction = record->moduleFunction;
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
    void *handle;
    FZrVmGetAotCompiledModule descriptorSymbol;
    const ZrAotCompiledModule *descriptor = ZR_NULL;
    SZrFunction *moduleFunction = ZR_NULL;
    SZrFunction **functionTable = ZR_NULL;
    TZrUInt32 functionCount = 0;
    TZrUInt32 functionCapacity = 0;
    SZrLibraryAotLoadedModule record;
    SZrString *moduleNameString;
    TZrBool sourceExists;
    TZrBool zroExists;

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

    aot_runtime_resolve_module_file(project,
                                    ZrCore_String_GetNativeString(project->source),
                                    normalizedModule,
                                    ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                    sourcePath,
                                    sizeof(sourcePath));
    aot_runtime_resolve_module_file(project,
                                    ZrCore_String_GetNativeString(project->binary),
                                    normalizedModule,
                                    ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                    zroPath,
                                    sizeof(zroPath));

    sourceExists = sourcePath[0] != '\0' && ZrLibrary_File_Exist(sourcePath) == ZR_LIBRARY_FILE_IS_FILE;
    zroExists = zroPath[0] != '\0' && ZrLibrary_File_Exist(zroPath) == ZR_LIBRARY_FILE_IS_FILE;

    if (!aot_runtime_resolve_library_path(project, backendKind, normalizedModule, libraryPath, sizeof(libraryPath)) ||
        ZrLibrary_File_Exist(libraryPath) != ZR_LIBRARY_FILE_IS_FILE) {
        if (!sourceExists && !zroExists) {
            aot_runtime_set_error(runtimeState, ZR_NULL);
            return ZR_FALSE;
        }
        aot_runtime_fail(state, runtimeState, "missing AOT artifacts for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    handle = aot_runtime_open_library(libraryPath);
    if (handle == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "failed to load AOT library '%s'", libraryPath);
        return ZR_FALSE;
    }

    descriptorSymbol =
            aot_runtime_cast_descriptor_symbol(aot_runtime_find_symbol(handle, "ZrVm_GetAotCompiledModule"));
    if (descriptorSymbol == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT library '%s' is missing ZrVm_GetAotCompiledModule", libraryPath);
        return ZR_FALSE;
    }

    descriptor = descriptorSymbol();
    if (descriptor == ZR_NULL || descriptor->abiVersion != ZR_VM_AOT_ABI_VERSION ||
        (EZrAotBackendKind)descriptor->backendKind != backendKind || descriptor->moduleName == ZR_NULL ||
        strcmp(descriptor->moduleName, normalizedModule) != 0 || descriptor->entryThunk == ZR_NULL) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT descriptor validation failed for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (backendKind == ZR_AOT_BACKEND_KIND_C &&
        (descriptor->embeddedModuleBlob == ZR_NULL || descriptor->embeddedModuleBlobLength == 0 ||
         descriptor->functionThunks == ZR_NULL || descriptor->functionThunkCount == 0)) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT descriptor validation failed for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if ((EZrAotInputKind)descriptor->inputKind == ZR_AOT_INPUT_KIND_SOURCE && sourceExists &&
        descriptor->inputHash != ZR_NULL && descriptor->inputHash[0] != '\0' &&
        aot_runtime_hash_file(sourcePath, sourceHash, sizeof(sourceHash)) &&
        strcmp(descriptor->inputHash, sourceHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT source hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if ((EZrAotInputKind)descriptor->inputKind == ZR_AOT_INPUT_KIND_BINARY && zroExists &&
        descriptor->inputHash != ZR_NULL && descriptor->inputHash[0] != '\0' &&
        aot_runtime_hash_file(zroPath, zroHash, sizeof(zroHash)) &&
        strcmp(descriptor->inputHash, zroHash) != 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "AOT binary hash mismatch for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (descriptor->embeddedModuleBlob != ZR_NULL && descriptor->embeddedModuleBlobLength > 0) {
        if (!aot_runtime_load_embedded_function(state,
                                                descriptor->embeddedModuleBlob,
                                                descriptor->embeddedModuleBlobLength,
                                                &moduleFunction)) {
            aot_runtime_close_library(handle);
            aot_runtime_fail(state, runtimeState, "failed to load embedded module blob for module '%s'", normalizedModule);
            return ZR_FALSE;
        }
    } else if (!zroExists || !aot_runtime_load_zro_function(state, zroPath, &moduleFunction)) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to load companion zro for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if (!aot_runtime_build_function_table(state, moduleFunction, &functionTable, &functionCount, &functionCapacity) ||
        functionCount == 0) {
        aot_runtime_close_library(handle);
        aot_runtime_fail(state, runtimeState, "failed to build AOT function table for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    if ((descriptor->functionThunks != ZR_NULL || descriptor->functionThunkCount != 0) &&
        (descriptor->functionThunks == ZR_NULL || descriptor->functionThunkCount != functionCount)) {
        TZrUInt32 descriptorThunkCount = descriptor != ZR_NULL ? descriptor->functionThunkCount : 0;
        aot_runtime_close_library(handle);
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT thunk table mismatch for module '%s' descriptor=%u runtime=%u",
                         normalizedModule,
                         (unsigned)descriptorThunkCount,
                         (unsigned)functionCount);
        return ZR_FALSE;
    }

    memset(&record, 0, sizeof(record));
    record.backendKind = backendKind;
    record.moduleName = aot_runtime_duplicate_string(global, normalizedModule);
    record.sourcePath = sourceExists ? aot_runtime_duplicate_string(global, sourcePath) : ZR_NULL;
    record.zroPath = zroExists ? aot_runtime_duplicate_string(global, zroPath) : ZR_NULL;
    record.libraryPath = aot_runtime_duplicate_string(global, libraryPath);
    record.libraryHandle = handle;
    record.descriptor = descriptor;
    record.moduleFunction = moduleFunction;
    record.functionTable = functionTable;
    record.functionCount = functionCount;
    record.functionCapacity = functionCapacity;
    record.module = ZrCore_Module_Create(state);
    if (record.moduleName == ZR_NULL || record.libraryPath == ZR_NULL || record.module == ZR_NULL ||
        (zroExists && record.zroPath == ZR_NULL)) {
        aot_runtime_close_library(handle);
        aot_runtime_free_string(global, record.moduleName);
        aot_runtime_free_string(global, record.sourcePath);
        aot_runtime_free_string(global, record.zroPath);
        aot_runtime_free_string(global, record.libraryPath);
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
        aot_runtime_fail(state, runtimeState, "failed to allocate AOT runtime record for module '%s'", normalizedModule);
        return ZR_FALSE;
    }

    moduleNameString = ZrCore_String_CreateFromNative(state, normalizedModule);
    if (moduleNameString != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, moduleNameString);
        ZrCore_Module_SetInfo(state, record.module, moduleNameString, pathHash, moduleNameString);
    }
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, record.module, record.moduleFunction);
    ZrCore_Module_CreatePrototypesFromConstants(state, record.module, record.moduleFunction);

    if (!aot_runtime_append_record(global, runtimeState, &record, outRecord)) {
        aot_runtime_close_library(handle);
        if (functionTable != ZR_NULL) {
            aot_runtime_reallocate(global, functionTable, sizeof(*functionTable) * functionCapacity, 0);
        }
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
        if (record->functionTable != ZR_NULL && record->functionCapacity > 0) {
            aot_runtime_reallocate(global,
                                   record->functionTable,
                                   sizeof(*record->functionTable) * record->functionCapacity,
                                   0);
        }
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

TZrBool ZrLibrary_AotRuntime_BeginGeneratedFunction(SZrState *state,
                                                    TZrUInt32 functionIndex,
                                                    ZrAotGeneratedFrame *frame) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 resolvedIndex;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer slotBase;
    TZrSize argumentCount;
    TZrSize frameSlotCount;
    TZrStackValuePointer frameTop;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor returnAnchor;
    TZrBool hasReturnAnchor;

    if (frame != ZR_NULL) {
        memset(frame, 0, sizeof(*frame));
    }
    if (state == ZR_NULL || state->global == ZR_NULL || frame == ZR_NULL) {
        return ZR_FALSE;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    callInfo = state->callInfoList;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (runtimeState == ZR_NULL || callInfo == ZR_NULL || metadataFunction == ZR_NULL || record == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT function invoked without matching runtime record");
        return ZR_FALSE;
    }

    resolvedIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (resolvedIndex == UINT32_MAX || resolvedIndex != functionIndex) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT function index mismatch for module '%s' (expected %u, got %u)",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                         (unsigned)functionIndex,
                         (unsigned)resolvedIndex);
        return ZR_FALSE;
    }

    functionBase = callInfo->functionBase.valuePointer;
    if (functionBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT function has no call frame");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, functionBase, &baseAnchor);
    hasReturnAnchor = callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL;
    if (hasReturnAnchor) {
        ZrCore_Function_StackAnchorInit(state, callInfo->returnDestination, &returnAnchor);
    }

    slotBase = ZrCore_Function_CheckStackAndGc(state, metadataFunction->stackSize, functionBase + 1);
    functionBase = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    callInfo->functionBase.valuePointer = functionBase;
    if (hasReturnAnchor) {
        callInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnAnchor);
    }
    slotBase = functionBase + 1;

    argumentCount =
            (state->stackTop.valuePointer != ZR_NULL && state->stackTop.valuePointer > slotBase)
                    ? (TZrSize)(state->stackTop.valuePointer - slotBase)
                    : 0;
    frameSlotCount = metadataFunction->stackSize > argumentCount ? metadataFunction->stackSize : argumentCount;
    frameTop = slotBase + frameSlotCount;

    for (TZrUInt32 slot = (TZrUInt32)argumentCount; slot < metadataFunction->stackSize; slot++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slotBase + slot));
    }

    if (callInfo->functionTop.valuePointer < frameTop) {
        callInfo->functionTop.valuePointer = frameTop;
    }
    if (state->stackTop.valuePointer < frameTop) {
        state->stackTop.valuePointer = frameTop;
    }

    runtimeState->executedVia = aot_runtime_backend_to_executed_via(record->backendKind);
    frame->recordHandle = record;
    frame->function = metadataFunction;
    frame->slotBase = slotBase;
    frame->functionIndex = resolvedIndex;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CopyConstant(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 constantIndex) {
    SZrLibraryAotLoadedModule *record;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer;
    const SZrTypeValue *source;

    function = aot_runtime_frame_function(frame);
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (state == ZR_NULL || function == ZR_NULL || record == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    source = &function->constantValueList[constantIndex];
    return aot_runtime_materialize_callable_constant(state, record, source, ZR_FALSE, ZrCore_Stack_GetValue(destinationPointer));
}

TZrBool ZrLibrary_AotRuntime_CreateClosure(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 constantIndex) {
    SZrLibraryAotLoadedModule *record;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer;
    const SZrTypeValue *source;

    function = aot_runtime_frame_function(frame);
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (state == ZR_NULL || function == ZR_NULL || record == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    source = &function->constantValueList[constantIndex];
    return aot_runtime_materialize_callable_constant(state, record, source, ZR_TRUE, ZrCore_Stack_GetValue(destinationPointer));
}

TZrBool ZrLibrary_AotRuntime_CopyStack(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(sourcePointer));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_AddInt(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt + rightInt);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetMember(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 receiverSlot,
                                       TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue stableReceiver;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(aot_runtime_frame_function(frame), memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER: invalid member id");
        return ZR_FALSE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_GetMember(state, &stableReceiver, memberSymbol, ZrCore_Stack_GetValue(destinationPointer))) {
        aot_runtime_fail(state, runtimeState, "GET_MEMBER: missing member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Call(SZrState *state,
                                  ZrAotGeneratedFrame *frame,
                                  TZrUInt32 destinationSlot,
                                  TZrUInt32 functionSlot,
                                  TZrUInt32 argumentCount) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callAnchor;

    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = state->callInfoList;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (callInfo == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callBase = ZrCore_Function_CallAndRestoreAnchor(state, &callAnchor, 1);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(callBase));
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToInt(SZrState *state,
                                   ZrAotGeneratedFrame *frame,
                                   TZrUInt32 destinationSlot,
                                   TZrUInt32 sourceSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *source;
    SZrTypeValue *destination;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    source = ZrCore_Stack_GetValue(sourcePointer);
    destination = ZrCore_Stack_GetValue(destinationPointer);
    if (source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(source->type)) {
        ZrCore_Value_Copy(state, destination, source);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(source->type)) {
        ZrCore_Value_InitAsInt(state, destination, (TZrInt64)source->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(source->type)) {
        ZrCore_Value_InitAsInt(state, destination, (TZrInt64)source->value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_BOOL(source->type)) {
        ZrCore_Value_InitAsInt(state, destination, source->value.nativeObject.nativeBool ? 1 : 0);
    } else {
        ZrCore_Value_InitAsInt(state, destination, 0);
    }
    return ZR_TRUE;
}

TZrInt64 ZrLibrary_AotRuntime_Return(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 sourceSlot,
                                     TZrBool publishExports) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    SZrLibraryAotLoadedModule *record;
    TZrStackValuePointer sourcePointer;
    SZrTypeValue *resultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT return failed");
        return 0;
    }

    resultValue = ZrCore_Stack_GetValue(sourcePointer);
    if (publishExports && record != ZR_NULL && !record->moduleExecuted &&
        !aot_runtime_materialize_exports(state, record, frame->slotBase)) {
        return 0;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer), resultValue);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrLibrary_AotRuntime_ReportUnsupportedInstruction(SZrState *state,
                                                           TZrUInt32 functionIndex,
                                                           TZrUInt32 instructionIndex,
                                                           TZrUInt32 opcode) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;

    aot_runtime_fail(state,
                     runtimeState,
                     "unsupported generated AOT instruction: functionIndex=%u instructionIndex=%u opcode=%u",
                     (unsigned)functionIndex,
                     (unsigned)instructionIndex,
                     (unsigned)opcode);
    return 0;
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
    if (!aot_runtime_execute_vm_shim_direct(state, record->moduleFunction, &resultBase)) {
        return 0;
    }

    if (!record->moduleExecuted) {
        aot_runtime_materialize_exports(state, record, resultBase + 1);
    }
    return 1;
}

TZrInt64 ZrLibrary_AotRuntime_InvokeCurrentClosureShim(SZrState *state, EZrAotBackendKind backendKind) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    SZrCallInfo *callInfo;
    SZrFunction *shimFunction;
    TZrStackValuePointer sourceBase;
    TZrStackValuePointer callBase;
    TZrStackValuePointer resultBase;
    SZrClosure *closure;
    SZrTypeValue *closureValue;
    TZrSize argumentCount;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return 0;
    }

    runtimeState = aot_runtime_get_state_from_global(state->global);
    if (runtimeState == ZR_NULL || runtimeState->activeRecord == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT closure shim invoked without an active record");
        return 0;
    }

    record = runtimeState->activeRecord;
    if (record->backendKind != backendKind) {
        aot_runtime_fail(state, runtimeState, "AOT backend mismatch for module '%s'", record->moduleName);
        return 0;
    }

    callInfo = state->callInfoList;
    shimFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL || shimFunction == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT closure shim is missing metadata function for module '%s'",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>");
        return 0;
    }

    sourceBase = callInfo->functionBase.valuePointer;
    argumentCount = (TZrSize)(state->stackTop.valuePointer - (sourceBase + 1));
    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_ReserveScratchSlots(state, argumentCount + 1, callBase);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "AOT closure shim failed to reserve call slots");
        return 0;
    }
    sourceBase = callInfo->functionBase.valuePointer;

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return 0;
    }

    closure->function = shimFunction;
    ZrCore_Closure_InitValue(state, closure);

    closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;

    for (TZrSize index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state, callBase + 1 + index, ZrCore_Stack_GetValue(sourceBase + 1 + index));
    }

    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    runtimeState->executedVia = aot_runtime_backend_to_executed_via(backendKind);
    resultBase = ZrCore_Function_CallAndRestore(state, callBase, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || resultBase == ZR_NULL) {
        return 0;
    }

    return 1;
}
