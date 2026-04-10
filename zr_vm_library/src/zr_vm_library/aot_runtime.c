#include "zr_vm_library/aot_runtime.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
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

typedef enum EZrAotRuntimeFloatBinaryOp {
    ZR_AOT_RUNTIME_FLOAT_BINARY_ADD = 0,
    ZR_AOT_RUNTIME_FLOAT_BINARY_SUB,
    ZR_AOT_RUNTIME_FLOAT_BINARY_MUL,
    ZR_AOT_RUNTIME_FLOAT_BINARY_DIV,
    ZR_AOT_RUNTIME_FLOAT_BINARY_MOD,
    ZR_AOT_RUNTIME_FLOAT_BINARY_POW
} EZrAotRuntimeFloatBinaryOp;

typedef enum EZrAotRuntimeCompareOp {
    ZR_AOT_RUNTIME_COMPARE_GREATER = 0,
    ZR_AOT_RUNTIME_COMPARE_LESS,
    ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL,
    ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL
} EZrAotRuntimeCompareOp;

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
static TZrBool aot_runtime_descriptor_has_true_aot_payload(const ZrAotCompiledModule *descriptor);
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
static SZrTypeValue *aot_runtime_get_closure_capture_from_value(SZrState *state,
                                                                const SZrTypeValue *closureContainerValue,
                                                                TZrUInt32 captureIndex);
static TZrBool aot_runtime_bind_native_closure_captures_from_source(SZrState *state,
                                                                    SZrClosureNative *destinationClosure,
                                                                    const SZrTypeValue *source,
                                                                    TZrUInt32 captureCount);
static TZrBool aot_runtime_bind_native_closure_captures_from_frame(SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame,
                                                                   SZrClosureNative *destinationClosure,
                                                                   SZrFunction *metadataFunction);
static TZrBool aot_runtime_materialize_callable_constant_with_context(SZrState *state,
                                                                      SZrLibraryAotLoadedModule *record,
                                                                      const SZrTypeValue *source,
                                                                      const ZrAotGeneratedFrame *frame,
                                                                      TZrBool forceClosure,
                                                                      SZrTypeValue *destination);
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
static TZrBool aot_runtime_refresh_frame_from_callinfo(SZrState *state, ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo);
static TZrBool aot_runtime_frame_resume_index(const ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo, TZrUInt32 *outIndex);
static TZrBool aot_runtime_resume_exception_in_current_frame(SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 *outResumeInstructionIndex);
static TZrBool aot_runtime_resume_pending_control_in_current_frame(SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 *outResumeInstructionIndex);
static TZrBool aot_runtime_resolve_current_closure_capture(SZrState *state,
                                                           const ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 closureIndex,
                                                           SZrTypeValue **outClosureValue,
                                                           SZrRawObject **outBarrierObject);
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
static void aot_runtime_resolve_observation_policy(const SZrState *state,
                                                   TZrUInt32 *outObservationMask,
                                                   TZrBool *outPublishAllInstructions);
static TZrBool aot_runtime_value_is_truthy(SZrState *state, const SZrTypeValue *value);
static TZrBool aot_runtime_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue);
static TZrBool aot_runtime_extract_integer_like_value(const SZrTypeValue *value, TZrInt64 *outValue);
static TZrBool aot_runtime_extract_unsigned_integer_like_value(const SZrTypeValue *value, TZrUInt64 *outValue);
static TZrBool aot_runtime_eval_binary_numeric_float(EZrAotRuntimeFloatBinaryOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrFloat64 *outResult);
static TZrBool aot_runtime_eval_binary_numeric_compare(EZrAotRuntimeCompareOp operation,
                                                       TZrFloat64 leftValue,
                                                       TZrFloat64 rightValue,
                                                       TZrBool *outResult);
static TZrSize aot_runtime_close_scope_registrations(SZrState *state, TZrSize cleanupCount);
static TZrBool aot_runtime_apply_float_binary_operation(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot,
                                                        EZrAotRuntimeFloatBinaryOp operation,
                                                        const TZrChar *instructionName);
static TZrBool aot_runtime_apply_float_compare_operation(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         EZrAotRuntimeCompareOp operation,
                                                         const TZrChar *instructionName);
static TZrBool aot_runtime_call_temp_base_without_yield(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrStackValuePointer callBase,
                                                        TZrUInt32 argumentCount);
static TZrBool aot_runtime_invoke_binary_meta(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              const SZrTypeValue *leftValue,
                                              const SZrTypeValue *rightValue,
                                              SZrFunction *metaFunction);

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

static TZrBool aot_runtime_descriptor_has_true_aot_payload(const ZrAotCompiledModule *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (descriptor->embeddedModuleBlob == ZR_NULL || descriptor->embeddedModuleBlobLength == 0) {
        return ZR_FALSE;
    }

    if (descriptor->functionThunks == ZR_NULL || descriptor->functionThunkCount == 0) {
        return ZR_FALSE;
    }

    return descriptor->entryThunk != ZR_NULL;
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

static TZrUInt32 aot_runtime_frame_slot_count(const ZrAotGeneratedFrame *frame) {
    return frame != ZR_NULL && frame->function != ZR_NULL
                   ? ZrCore_Function_GetGeneratedFrameSlotCount(frame->function)
                   : 0u;
}

static TZrStackValuePointer aot_runtime_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_NULL;
    }

    if (frame->function == ZR_NULL || slotIndex >= aot_runtime_frame_slot_count(frame)) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

static const SZrTypeValue *aot_runtime_frame_constant(const ZrAotGeneratedFrame *frame, TZrUInt32 constantIndex) {
    if (frame == ZR_NULL || frame->function == ZR_NULL || frame->function->constantValueList == ZR_NULL ||
        constantIndex >= frame->function->constantValueLength) {
        return ZR_NULL;
    }

    return &frame->function->constantValueList[constantIndex];
}

static TZrBool aot_runtime_refresh_frame_from_callinfo(SZrState *state, ZrAotGeneratedFrame *frame, SZrCallInfo *callInfo) {
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    frame->callInfo = callInfo;
    frame->slotBase = callInfo->functionBase.valuePointer + 1;
    state->callInfoList = callInfo;
    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    return ZR_TRUE;
}

static TZrBool aot_runtime_frame_resume_index(const ZrAotGeneratedFrame *frame,
                                              SZrCallInfo *callInfo,
                                              TZrUInt32 *outIndex) {
    const SZrFunction *function;

    if (outIndex != ZR_NULL) {
        *outIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    function = aot_runtime_frame_function(frame);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || callInfo == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callInfo->context.context.programCounter < function->instructionsList ||
        callInfo->context.context.programCounter >= function->instructionsList + function->instructionsLength) {
        return ZR_FALSE;
    }

    *outIndex = (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
    return ZR_TRUE;
}

static TZrBool aot_runtime_resume_exception_in_current_frame(SZrState *state,
                                                             ZrAotGeneratedFrame *frame,
                                                             TZrUInt32 *outResumeInstructionIndex) {
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    if (state == ZR_NULL || frame == ZR_NULL || !state->hasCurrentException) {
        return ZR_FALSE;
    }

    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!execution_unwind_exception_to_handler(state, &callInfo)) {
        return ZR_FALSE;
    }

    if (callInfo != frame->callInfo) {
        return ZR_FALSE;
    }

    if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }

    return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
}

static TZrBool aot_runtime_resume_pending_control_in_current_frame(SZrState *state,
                                                                   ZrAotGeneratedFrame *frame,
                                                                   TZrUInt32 *outResumeInstructionIndex) {
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
        if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
        return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
    }

    if (!execution_jump_to_instruction_offset(state,
                                              &callInfo,
                                              callInfo,
                                              state->pendingControl.targetInstructionOffset)) {
        execution_clear_pending_control(state);
        return ZR_FALSE;
    }

    execution_clear_pending_control(state);
    if (callInfo != frame->callInfo || !aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
        return ZR_FALSE;
    }
    return aot_runtime_frame_resume_index(frame, callInfo, outResumeInstructionIndex);
}

static TZrBool aot_runtime_resolve_current_closure_capture(SZrState *state,
                                                           const ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 closureIndex,
                                                           SZrTypeValue **outClosureValue,
                                                           SZrRawObject **outBarrierObject) {
    const SZrTypeValue *currentClosureValue;

    if (outClosureValue != ZR_NULL) {
        *outClosureValue = ZR_NULL;
    }
    if (outBarrierObject != ZR_NULL) {
        *outBarrierObject = ZR_NULL;
    }
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL || outClosureValue == ZR_NULL) {
        return ZR_FALSE;
    }

    currentClosureValue = ZrCore_Stack_GetValue(frame->slotBase - 1);
    if (currentClosureValue == ZR_NULL || currentClosureValue->type != ZR_VALUE_TYPE_CLOSURE ||
        currentClosureValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (currentClosureValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, currentClosureValue->value.object);
        SZrRawObject *captureOwner;
        if (nativeClosure == ZR_NULL || closureIndex >= nativeClosure->closureValueCount) {
            return ZR_FALSE;
        }
        captureOwner = ZrCore_ClosureNative_GetCaptureOwner(nativeClosure, closureIndex);
        *outClosureValue = ZrCore_ClosureNative_GetCaptureValue(nativeClosure, closureIndex);
        if (outBarrierObject != ZR_NULL) {
            *outBarrierObject = captureOwner != ZR_NULL ? captureOwner : ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure);
        }
        return *outClosureValue != ZR_NULL;
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, currentClosureValue->value.object);
        SZrClosureValue *closureValue;
        if (vmClosure == ZR_NULL || closureIndex >= vmClosure->closureValueCount) {
            return ZR_FALSE;
        }
        closureValue = vmClosure->closureValuesExtend[closureIndex];
        if (closureValue == ZR_NULL) {
            return ZR_FALSE;
        }
        *outClosureValue = ZrCore_ClosureValue_GetValue(closureValue);
        if (outBarrierObject != ZR_NULL) {
            *outBarrierObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
        }
        return *outClosureValue != ZR_NULL;
    }
}

static SZrCallInfo *aot_runtime_reserve_call_info(SZrState *state, SZrCallInfo *callerCallInfo) {
    SZrCallInfo *callInfo;
    SZrCallInfo *nextCallInfo = ZR_NULL;

    if (state == ZR_NULL || callerCallInfo == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = callerCallInfo->next != ZR_NULL ? callerCallInfo->next : ZrCore_CallInfo_Extend(state);
    if (callInfo == ZR_NULL) {
        return ZR_NULL;
    }

    nextCallInfo = callInfo->next;
    memset(callInfo, 0, sizeof(*callInfo));
    callInfo->next = nextCallInfo;
    callInfo->previous = callerCallInfo;
    return callInfo;
}

static void aot_runtime_initialize_vm_call_frame_slots(TZrStackValuePointer functionBase,
                                                       TZrUInt32 preservedArgumentCount,
                                                       TZrUInt32 stackSize) {
    TZrUInt32 slotIndex;

    if (functionBase == ZR_NULL) {
        return;
    }

    if (preservedArgumentCount > stackSize) {
        preservedArgumentCount = stackSize;
    }

    for (slotIndex = preservedArgumentCount; slotIndex < stackSize; slotIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + 1 + slotIndex));
    }
}

static TZrUInt32 aot_runtime_generated_resume_instruction_index(const ZrAotGeneratedFrame *frame) {
    if (frame == ZR_NULL || frame->function == ZR_NULL ||
        frame->currentInstructionIndex >= frame->function->instructionsLength ||
        frame->currentInstructionIndex + 1 >= frame->function->instructionsLength) {
        return ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    return frame->currentInstructionIndex + 1;
}

static void aot_runtime_record_direct_call_context(const ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 calleeFunctionIndex,
                                                   ZrAotGeneratedDirectCall *directCall) {
    if (directCall == ZR_NULL) {
        return;
    }

    directCall->callerFunctionIndex = frame != ZR_NULL ? frame->functionIndex : UINT32_MAX;
    directCall->calleeFunctionIndex = calleeFunctionIndex;
    directCall->callInstructionIndex = frame != ZR_NULL ? frame->currentInstructionIndex : UINT32_MAX;
    directCall->resumeInstructionIndex = aot_runtime_generated_resume_instruction_index(frame);
    directCall->observationMaskSnapshot = frame != ZR_NULL ? frame->observationMask : 0;
    directCall->publishAllInstructionsSnapshot = frame != ZR_NULL ? frame->publishAllInstructions : ZR_FALSE;
}

static TZrBool aot_runtime_prepare_vm_direct_call_frame(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 functionSlot,
                                                        TZrUInt32 argumentCount,
                                                        SZrFunction *metadataFunction,
                                                        TZrUInt32 calleeFunctionIndex,
                                                        ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;
    SZrCallInfo *callInfo;
    TZrUInt32 callValueCount;
    TZrSize frameSlotCount;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callerCallInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL ||
        callerCallInfo == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL || metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callBase + 1 + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call has invalid stack range");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    frameSlotCount = ZrCore_Function_GetGeneratedFrameSlotCount(metadataFunction);
    ZrCore_Function_CheckStackAndGc(state, frameSlotCount, callBase);
    callBase = ZrCore_Function_StackAnchorRestore(state, &callAnchor);
    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = aot_runtime_reserve_call_info(state, callerCallInfo);
    if (callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo->functionBase.valuePointer = callBase;
    callInfo->functionTop.valuePointer = callBase + 1 + frameSlotCount;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->context.context.programCounter = metadataFunction->instructionsList;
    callInfo->context.context.variableArgumentCount = 0;
    callInfo->context.context.trap = 0;
    callInfo->expectedReturnCount = 1;
    callInfo->returnDestination = destinationPointer;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = ZR_TRUE;

    aot_runtime_initialize_vm_call_frame_slots(callBase, argumentCount, frameSlotCount);

    state->callInfoList = callInfo;
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
        callValueCount = (TZrUInt32)(state->stackTop.valuePointer - callBase);
        ZrCore_Debug_Hook(state,
                          ZR_DEBUG_HOOK_EVENT_CALL,
                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                          1,
                          callValueCount);
    }

    directCall->callerCallInfo = callerCallInfo;
    directCall->calleeCallInfo = callInfo;
    aot_runtime_record_direct_call_context(frame, calleeFunctionIndex, directCall);
    directCall->prepared = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool aot_runtime_try_prepare_direct_native_call(SZrState *state,
                                                          ZrAotGeneratedFrame *frame,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 functionSlot,
                                                          TZrUInt32 argumentCount,
                                                          ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    SZrTypeValue *functionValue;
    SZrClosureNative *closureNative;
    SZrLibraryAotLoadedModule *record;
    SZrFunction *metadataFunction;
    TZrUInt32 functionIndex;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (functionValue == ZR_NULL || functionValue->type != ZR_VALUE_TYPE_CLOSURE || !functionValue->isNative ||
        functionValue->value.object == ZR_NULL) {
        return ZR_TRUE;
    }

    closureNative = ZR_CAST_NATIVE_CLOSURE(state, functionValue->value.object);
    metadataFunction = closureNative != ZR_NULL ? closureNative->aotShimFunction : ZR_NULL;
    if (closureNative == ZR_NULL || closureNative->nativeFunction == ZR_NULL || metadataFunction == ZR_NULL) {
        return ZR_TRUE;
    }

    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (record == ZR_NULL || record->descriptor == ZR_NULL || record->descriptor->functionThunks == ZR_NULL ||
        record->descriptor->functionThunkCount == 0) {
        return ZR_TRUE;
    }

    functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->descriptor->functionThunkCount ||
        record->descriptor->functionThunks[functionIndex] == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  functionSlot,
                                                  argumentCount,
                                                  metadataFunction,
                                                  functionIndex,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = record->descriptor->functionThunks[functionIndex];
    return ZR_TRUE;
}

static TZrBool aot_runtime_prepare_meta_target(SZrState *state,
                                               TZrStackValuePointer callBase,
                                               TZrUInt32 argumentCount,
                                               SZrFunction **outMetadataFunction) {
    SZrTypeValue *receiverValue;
    SZrMeta *metaValue;
    TZrStackValuePointer cursor;

    if (outMetadataFunction != ZR_NULL) {
        *outMetadataFunction = ZR_NULL;
    }
    if (state == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(callBase);
    metaValue = receiverValue != ZR_NULL ? ZrCore_Value_GetMeta(state, receiverValue, ZR_META_CALL) : ZR_NULL;
    if (receiverValue == ZR_NULL || metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    for (cursor = state->stackTop.valuePointer; cursor > callBase; cursor--) {
        ZrCore_Stack_CopyValue(state, cursor, ZrCore_Stack_GetValue(cursor - 1));
    }
    state->stackTop.valuePointer++;

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metaValue->function));
    if (outMetadataFunction != ZR_NULL) {
        *outMetadataFunction = metaValue->function;
    }
    return ZR_TRUE;
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

static TZrBool aot_runtime_resolve_cached_meta_symbol(const SZrFunction *function,
                                                      TZrUInt32 cacheIndex,
                                                      TZrUInt32 expectedKind,
                                                      TZrBool expectedStatic,
                                                      SZrString **outSymbol) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;
    const SZrFunctionMemberEntry *memberEntry;
    TZrBool actualStatic;

    if (outSymbol != ZR_NULL) {
        *outSymbol = ZR_NULL;
    }
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL || function->memberEntries == ZR_NULL ||
        outSymbol == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return ZR_FALSE;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if (cacheEntry->kind != expectedKind || cacheEntry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_FALSE;
    }

    memberEntry = &function->memberEntries[cacheEntry->memberEntryIndex];
    actualStatic = (TZrBool)((memberEntry->reserved0 & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
    if (actualStatic != expectedStatic || memberEntry->symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSymbol = memberEntry->symbol;
    return ZR_TRUE;
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

static SZrTypeValue *aot_runtime_get_closure_capture_from_value(SZrState *state,
                                                                const SZrTypeValue *closureContainerValue,
                                                                TZrUInt32 captureIndex) {
    if (state == ZR_NULL || closureContainerValue == ZR_NULL || closureContainerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        closureContainerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (closureContainerValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, closureContainerValue->value.object);
        if (nativeClosure == ZR_NULL || captureIndex >= nativeClosure->closureValueCount) {
            return ZR_NULL;
        }
        return ZrCore_ClosureNative_GetCaptureValue(nativeClosure, captureIndex);
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, closureContainerValue->value.object);
        if (vmClosure == ZR_NULL || captureIndex >= vmClosure->closureValueCount) {
            return ZR_NULL;
        }
        return vmClosure->closureValuesExtend[captureIndex] != ZR_NULL
                       ? ZrCore_ClosureValue_GetValue(vmClosure->closureValuesExtend[captureIndex])
                       : ZR_NULL;
    }
}

static SZrRawObject *aot_runtime_get_closure_capture_owner_from_value(SZrState *state,
                                                                      const SZrTypeValue *closureContainerValue,
                                                                      TZrUInt32 captureIndex) {
    if (state == ZR_NULL || closureContainerValue == ZR_NULL || closureContainerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        closureContainerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (closureContainerValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, closureContainerValue->value.object);
        if (nativeClosure == ZR_NULL || captureIndex >= nativeClosure->closureValueCount) {
            return ZR_NULL;
        }
        return ZrCore_ClosureNative_GetCaptureOwner(nativeClosure, captureIndex);
    }

    {
        SZrClosure *vmClosure = ZR_CAST_VM_CLOSURE(state, closureContainerValue->value.object);
        if (vmClosure == ZR_NULL || captureIndex >= vmClosure->closureValueCount ||
            vmClosure->closureValuesExtend[captureIndex] == ZR_NULL) {
            return ZR_NULL;
        }
        return ZR_CAST_RAW_OBJECT_AS_SUPER(vmClosure->closureValuesExtend[captureIndex]);
    }
}

static TZrBool aot_runtime_project_closure_into_vm_shim(SZrState *state,
                                                        const SZrTypeValue *sourceClosureValue,
                                                        SZrFunction *shimFunction,
                                                        SZrClosure **outClosure) {
    SZrClosure *closure;
    TZrUInt32 captureCount;
    SZrTypeValue projectedSelfValue;

    if (outClosure != ZR_NULL) {
        *outClosure = ZR_NULL;
    }
    if (state == ZR_NULL || sourceClosureValue == ZR_NULL || shimFunction == ZR_NULL || outClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    captureCount = shimFunction->closureValueLength;
    closure = ZrCore_Closure_New(state, captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->function = shimFunction;
    ZrCore_Closure_InitValue(state, closure);
    ZrCore_Value_InitAsRawObject(state, &projectedSelfValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    projectedSelfValue.type = ZR_VALUE_TYPE_CLOSURE;
    projectedSelfValue.isGarbageCollectable = ZR_TRUE;
    projectedSelfValue.isNative = ZR_FALSE;

    for (TZrUInt32 captureIndex = 0; captureIndex < captureCount; captureIndex++) {
        SZrClosureValue *destinationCapture = closure->closureValuesExtend[captureIndex];
        SZrTypeValue *sourceCaptureValue =
                aot_runtime_get_closure_capture_from_value(state, sourceClosureValue, captureIndex);
        SZrTypeValue *destinationCaptureValue;

        if (destinationCapture == ZR_NULL || sourceCaptureValue == ZR_NULL) {
            return ZR_FALSE;
        }

        if (sourceCaptureValue->type == ZR_VALUE_TYPE_CLOSURE &&
            sourceClosureValue->type == ZR_VALUE_TYPE_CLOSURE &&
            sourceCaptureValue->value.object == sourceClosureValue->value.object) {
            sourceCaptureValue = &projectedSelfValue;
        }

        destinationCaptureValue = ZrCore_ClosureValue_GetValue(destinationCapture);
        if (destinationCaptureValue == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_Copy(state, destinationCaptureValue, sourceCaptureValue);
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationCapture), sourceCaptureValue);
    }

    *outClosure = closure;
    return ZR_TRUE;
}

static TZrBool aot_runtime_bind_native_closure_capture(SZrState *state,
                                                       SZrClosureNative *destinationClosure,
                                                       TZrUInt32 destinationIndex,
                                                       SZrTypeValue *captureValue,
                                                       SZrRawObject *captureOwner) {
    SZrRawObject **captureOwners;

    if (state == ZR_NULL || destinationClosure == ZR_NULL || captureValue == ZR_NULL ||
        destinationIndex >= destinationClosure->closureValueCount) {
        return ZR_FALSE;
    }

    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(destinationClosure);
    destinationClosure->closureValuesExtend[destinationIndex] = captureValue;
    if (captureOwners != ZR_NULL) {
        captureOwners[destinationIndex] = captureOwner;
    }
    if (captureOwner != ZR_NULL) {
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationClosure), captureOwner);
    } else {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destinationClosure), captureValue);
    }
    return ZR_TRUE;
}

static TZrBool aot_runtime_bind_native_closure_captures_from_source(SZrState *state,
                                                                    SZrClosureNative *destinationClosure,
                                                                    const SZrTypeValue *source,
                                                                    TZrUInt32 captureCount) {
    TZrUInt32 captureIndex;

    if (captureCount == 0) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || destinationClosure == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    for (captureIndex = 0; captureIndex < captureCount; captureIndex++) {
        SZrTypeValue *captureValue = aot_runtime_get_closure_capture_from_value(state, source, captureIndex);
        SZrRawObject *captureOwner = aot_runtime_get_closure_capture_owner_from_value(state, source, captureIndex);
        if (!aot_runtime_bind_native_closure_capture(state,
                                                     destinationClosure,
                                                     captureIndex,
                                                     captureValue,
                                                     captureOwner)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_bind_native_closure_captures_from_frame(SZrState *state,
                                                                   const ZrAotGeneratedFrame *frame,
                                                                   SZrClosureNative *destinationClosure,
                                                                   SZrFunction *metadataFunction) {
    const SZrTypeValue *currentClosureValue;
    TZrUInt32 captureIndex;

    if (metadataFunction == ZR_NULL || metadataFunction->closureValueLength == 0) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL || destinationClosure == ZR_NULL ||
        metadataFunction->closureValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    currentClosureValue = ZrCore_Stack_GetValue(frame->slotBase - 1);
    for (captureIndex = 0; captureIndex < metadataFunction->closureValueLength; captureIndex++) {
        const SZrFunctionClosureVariable *closureVariable = &metadataFunction->closureValueList[captureIndex];
        SZrTypeValue *captureValue = ZR_NULL;
        SZrRawObject *captureOwner = ZR_NULL;

        if (closureVariable->inStack) {
            SZrClosureValue *closureValue = ZrCore_Closure_FindOrCreateValue(state, frame->slotBase + closureVariable->index);
            if (closureValue != ZR_NULL) {
                captureValue = ZrCore_ClosureValue_GetValue(closureValue);
                captureOwner = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
            }
        } else {
            captureValue = aot_runtime_get_closure_capture_from_value(state, currentClosureValue, closureVariable->index);
            captureOwner =
                    aot_runtime_get_closure_capture_owner_from_value(state, currentClosureValue, closureVariable->index);
        }

        if (!aot_runtime_bind_native_closure_capture(state,
                                                     destinationClosure,
                                                     captureIndex,
                                                     captureValue,
                                                     captureOwner)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_materialize_callable_constant_with_context(SZrState *state,
                                                                      SZrLibraryAotLoadedModule *record,
                                                                      const SZrTypeValue *source,
                                                                      const ZrAotGeneratedFrame *frame,
                                                                      TZrBool forceClosure,
                                                                      SZrTypeValue *destination) {
    SZrFunction *metadataFunction;
    TZrUInt32 functionIndex;
    SZrClosureNative *closure;
    SZrLibraryAotRuntimeState *runtimeState;
    TZrUInt32 captureCount;
    TZrBool capturesBound = ZR_FALSE;

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

    captureCount = metadataFunction->closureValueLength;
    closure = ZrCore_ClosureNative_New(state, captureCount);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = (FZrNativeFunction)record->descriptor->functionThunks[functionIndex];
    closure->aotShimFunction = metadataFunction;

    if (captureCount > 0) {
        if (frame != ZR_NULL) {
            capturesBound = aot_runtime_bind_native_closure_captures_from_frame(state, frame, closure, metadataFunction);
        }
        if (!capturesBound && source->type == ZR_VALUE_TYPE_CLOSURE && source->value.object != ZR_NULL) {
            capturesBound = aot_runtime_bind_native_closure_captures_from_source(state, closure, source, captureCount);
        }
        if (!capturesBound) {
            aot_runtime_fail(state,
                             runtimeState,
                             "AOT native closure capture binding failed for module '%s' function index %u",
                             record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                             (unsigned)functionIndex);
            return ZR_FALSE;
        }
    }

    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    destination->type = forceClosure ? ZR_VALUE_TYPE_CLOSURE : ZR_VALUE_TYPE_CLOSURE;
    destination->isGarbageCollectable = ZR_TRUE;
    destination->isNative = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool aot_runtime_materialize_callable_constant(SZrState *state,
                                                         SZrLibraryAotLoadedModule *record,
                                                         const SZrTypeValue *source,
                                                         TZrBool forceClosure,
                                                         SZrTypeValue *destination) {
    return aot_runtime_materialize_callable_constant_with_context(state,
                                                                  record,
                                                                  source,
                                                                  ZR_NULL,
                                                                  forceClosure,
                                                                  destination);
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
    SZrClosureNative *entryClosure;
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

    entryClosure = ZrCore_ClosureNative_New(state, 0);
    if (entryClosure == ZR_NULL) {
        return ZR_FALSE;
    }

    entryClosure->nativeFunction = (FZrNativeFunction)entryThunk;
    entryClosure->aotShimFunction = record->moduleFunction;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, 1, base, base, &anchor);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryClosure));
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

    if (!aot_runtime_descriptor_has_true_aot_payload(descriptor)) {
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

static void aot_runtime_resolve_observation_policy(const SZrState *state,
                                                   TZrUInt32 *outObservationMask,
                                                   TZrBool *outPublishAllInstructions) {
    TZrUInt32 observationMask = ZrLibrary_AotRuntime_DefaultObservationMask();
    TZrBool publishAllInstructions = ZR_FALSE;

    if (state != ZR_NULL) {
        if (state->hasAotObservationPolicyOverride) {
            observationMask = state->aotObservationMask;
            publishAllInstructions = state->aotPublishAllInstructions;
        }
        if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u) {
            publishAllInstructions = ZR_TRUE;
        }
    }

    if (outObservationMask != ZR_NULL) {
        *outObservationMask = observationMask;
    }
    if (outPublishAllInstructions != ZR_NULL) {
        *outPublishAllInstructions = publishAllInstructions;
    }
}

static TZrBool aot_runtime_value_is_truthy(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        return value->value.nativeObject.nativeInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble != 0.0;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_STRING(value->type)) {
        SZrString *stringValue = ZR_CAST_STRING(state, value->value.object);
        TZrSize length = 0;

        if (stringValue == ZR_NULL) {
            return ZR_FALSE;
        }
        length = (stringValue->shortStringLength < ZR_VM_LONG_STRING_FLAG)
                         ? stringValue->shortStringLength
                         : stringValue->longStringLength;
        return length > 0;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SetObservationPolicy(SZrState *state,
                                                  TZrUInt32 observationMask,
                                                  TZrBool publishAllInstructions) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    state->aotObservationMask = observationMask;
    state->aotPublishAllInstructions = publishAllInstructions;
    state->hasAotObservationPolicyOverride = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ResetObservationPolicy(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    state->aotObservationMask = 0;
    state->aotPublishAllInstructions = ZR_FALSE;
    state->hasAotObservationPolicyOverride = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetObservationPolicy(SZrState *state,
                                                  TZrUInt32 *outObservationMask,
                                                  TZrBool *outPublishAllInstructions) {
    if (state == ZR_NULL || outObservationMask == ZR_NULL || outPublishAllInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    aot_runtime_resolve_observation_policy(state, outObservationMask, outPublishAllInstructions);
    return ZR_TRUE;
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

    frameSlotCount = ZrCore_Function_GetGeneratedFrameSlotCount(metadataFunction);
    slotBase = ZrCore_Function_CheckStackAndGc(state, frameSlotCount, functionBase + 1);
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
    if (frameSlotCount < argumentCount) {
        frameSlotCount = argumentCount;
    }
    frameTop = slotBase + frameSlotCount;

    for (TZrUInt32 slot = (TZrUInt32)argumentCount; slot < (TZrUInt32)frameSlotCount; slot++) {
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
    frame->callInfo = callInfo;
    frame->slotBase = slotBase;
    frame->functionIndex = resolvedIndex;
    frame->currentInstructionIndex = 0;
    frame->lastObservedInstructionIndex = UINT32_MAX;
    frame->lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
    aot_runtime_resolve_observation_policy(state, &frame->observationMask, &frame->publishAllInstructions);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BeginInstruction(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 instructionIndex,
                                              TZrUInt32 stepFlags) {
    SZrCallInfo *callInfo;
    TZrBool publishAllInstructions;

    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    if (callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callInfo->functionBase.valuePointer != ZR_NULL) {
        if (!aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)) {
            return ZR_FALSE;
        }
        callInfo = frame->callInfo;
    } else {
        frame->callInfo = callInfo;
        state->callInfoList = callInfo;
    }
    frame->currentInstructionIndex = instructionIndex;
    publishAllInstructions = (TZrBool)(frame->publishAllInstructions ||
                                       ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u));
    if (!publishAllInstructions && (frame->observationMask & stepFlags) == 0u) {
        return ZR_TRUE;
    }

    callInfo->context.context.programCounter = frame->function->instructionsList + instructionIndex;
    frame->lastObservedInstructionIndex = instructionIndex;
    if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u) {
        TZrUInt32 sourceLine = ZrCore_Exception_FindSourceLine(frame->function, (TZrMemoryOffset)instructionIndex);

        if (sourceLine != ZR_RUNTIME_DEBUG_HOOK_LINE_NONE && sourceLine != frame->lastObservedLine) {
            frame->lastObservedLine = sourceLine;
            ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, sourceLine, 0, 0);
        }
    }

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
    return aot_runtime_materialize_callable_constant_with_context(state,
                                                                  record,
                                                                  source,
                                                                  frame,
                                                                  ZR_TRUE,
                                                                  ZrCore_Stack_GetValue(destinationPointer));
}

TZrBool ZrLibrary_AotRuntime_GetClosureValue(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 closureIndex) {
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *closureValue = ZR_NULL;

    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || destinationPointer == ZR_NULL ||
        !aot_runtime_resolve_current_closure_capture(state, frame, closureIndex, &closureValue, ZR_NULL) ||
        closureValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), closureValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SetClosureValue(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 closureIndex) {
    TZrStackValuePointer sourcePointer;
    SZrTypeValue *targetValue;
    SZrTypeValue *sourceValue;
    SZrRawObject *barrierObject = ZR_NULL;

    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || sourcePointer == ZR_NULL ||
        !aot_runtime_resolve_current_closure_capture(state, frame, closureIndex, &targetValue, &barrierObject) ||
        targetValue == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (targetValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, targetValue, sourceValue);
    if (barrierObject != ZR_NULL) {
        ZrCore_Value_Barrier(state, barrierObject, sourceValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CopyStack(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    ZrCore_Value_AssignMaterializedStackValue(state, destinationValue, sourceValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetGlobal(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_GLOBAL: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_GLOBAL: missing destination value");
        return ZR_FALSE;
    }

    if (state->global != ZR_NULL && state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_Copy(state, destinationValue, &state->global->zrObject);
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CreateObject(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrObject *objectValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_OBJECT: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_OBJECT: missing destination value");
        return ZR_FALSE;
    }

    objectValue = ZrCore_Object_New(state, ZR_NULL);
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    if (objectValue != ZR_NULL) {
        ZrCore_Object_Init(state, objectValue);
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(objectValue));
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CreateArray(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    SZrObject *arrayValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_ARRAY: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CREATE_ARRAY: missing destination value");
        return ZR_FALSE;
    }

    arrayValue = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    ZrCore_Ownership_ReleaseValue(state, destinationValue);
    if (arrayValue != ZR_NULL) {
        ZrCore_Object_Init(state, arrayValue);
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayValue));
        destinationValue->type = ZR_VALUE_TYPE_ARRAY;
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_TypeOf(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TYPEOF: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Reflection_TypeOfValue(state, sourceValue, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "TYPEOF: failed to materialize runtime type");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToObject(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot,
                                      TZrUInt32 typeNameConstantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    const SZrTypeValue *typeNameValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL ||
        typeNameConstantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "TO_OBJECT: invalid frame slot or type-name constant");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    typeNameValue = &function->constantValueList[typeNameConstantIndex];
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Execution_ToObject(state,
                                   frame != ZR_NULL ? frame->callInfo : state->callInfoList,
                                   destinationValue,
                                   sourceValue,
                                   typeNameValue)) {
        aot_runtime_fail(state, runtimeState, "TO_OBJECT: failed to materialize object conversion");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToStruct(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot,
                                      TZrUInt32 typeNameConstantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    const SZrTypeValue *typeNameValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL ||
        typeNameConstantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "TO_STRUCT: invalid frame slot or type-name constant");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    typeNameValue = &function->constantValueList[typeNameConstantIndex];
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Execution_ToStruct(state,
                                   frame != ZR_NULL ? frame->callInfo : state->callInfoList,
                                   destinationValue,
                                   sourceValue,
                                   typeNameValue)) {
        aot_runtime_fail(state, runtimeState, "TO_STRUCT: failed to materialize struct conversion");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MetaGet(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 receiverSlot,
                                     TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue stableReceiver;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(function, memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "META_GET: invalid member id");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || ZrCore_Stack_GetValue(receiverPointer) == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "META_GET: invalid stack slot");
        return ZR_FALSE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_InvokeMember(state, &stableReceiver, memberSymbol, ZR_NULL, 0, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "META_GET: receiver must define property getter");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MetaSet(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 receiverAndResultSlot,
                                     TZrUInt32 assignedValueSlot,
                                     TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverAndResultSlot);
    TZrStackValuePointer assignedPointer = aot_runtime_frame_slot(frame, assignedValueSlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrTypeValue ignoredResult;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || receiverPointer == ZR_NULL || assignedPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(function, memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "META_SET: invalid member id");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    assignedValue = ZrCore_Stack_GetValue(assignedPointer);
    if (receiverValue == ZR_NULL || assignedValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "META_SET: invalid stack slot");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableAssignedValue = *assignedValue;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (!ZrCore_Object_InvokeMember(state,
                                    &stableReceiver,
                                    memberSymbol,
                                    &stableAssignedValue,
                                    1,
                                    &ignoredResult)) {
        aot_runtime_fail(state, runtimeState, "META_SET: receiver must define property setter");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, receiverValue, &stableAssignedValue);
    return ZR_TRUE;
}

static TZrBool aot_runtime_meta_get_cached_internal(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 cacheIndex,
                                                    TZrUInt32 expectedKind,
                                                    TZrBool expectedStatic,
                                                    const TZrChar *failureLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue stableReceiver;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_cached_meta_symbol(function, cacheIndex, expectedKind, expectedStatic, &memberSymbol)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid call-site cache or member binding",
                         failureLabel != ZR_NULL ? failureLabel : "META_GET");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || ZrCore_Stack_GetValue(receiverPointer) == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid stack slot",
                         failureLabel != ZR_NULL ? failureLabel : "META_GET");
        return ZR_FALSE;
    }

    stableReceiver = *ZrCore_Stack_GetValue(receiverPointer);
    if (!ZrCore_Object_InvokeMember(state, &stableReceiver, memberSymbol, ZR_NULL, 0, destinationValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: receiver must define property getter",
                         failureLabel != ZR_NULL ? failureLabel : "META_GET");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_meta_set_cached_internal(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverAndResultSlot,
                                                    TZrUInt32 assignedValueSlot,
                                                    TZrUInt32 cacheIndex,
                                                    TZrUInt32 expectedKind,
                                                    TZrBool expectedStatic,
                                                    const TZrChar *failureLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverAndResultSlot);
    TZrStackValuePointer assignedPointer = aot_runtime_frame_slot(frame, assignedValueSlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrTypeValue ignoredResult;
    SZrString *memberSymbol = ZR_NULL;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || receiverPointer == ZR_NULL || assignedPointer == ZR_NULL ||
        !aot_runtime_resolve_cached_meta_symbol(function, cacheIndex, expectedKind, expectedStatic, &memberSymbol)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid call-site cache or member binding",
                         failureLabel != ZR_NULL ? failureLabel : "META_SET");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    assignedValue = ZrCore_Stack_GetValue(assignedPointer);
    if (receiverValue == ZR_NULL || assignedValue == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: invalid stack slot",
                         failureLabel != ZR_NULL ? failureLabel : "META_SET");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableAssignedValue = *assignedValue;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (!ZrCore_Object_InvokeMember(state,
                                    &stableReceiver,
                                    memberSymbol,
                                    &stableAssignedValue,
                                    1,
                                    &ignoredResult)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "%s: receiver must define property setter",
                         failureLabel != ZR_NULL ? failureLabel : "META_SET");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, receiverValue, &stableAssignedValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MetaGetCached(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 cacheIndex) {
    return aot_runtime_meta_get_cached_internal(state,
                                                frame,
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                ZR_FALSE,
                                                "SUPER_META_GET_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaSetCached(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 receiverAndResultSlot,
                                           TZrUInt32 assignedValueSlot,
                                           TZrUInt32 cacheIndex) {
    return aot_runtime_meta_set_cached_internal(state,
                                                frame,
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                ZR_FALSE,
                                                "SUPER_META_SET_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaGetStaticCached(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 receiverSlot,
                                                 TZrUInt32 cacheIndex) {
    return aot_runtime_meta_get_cached_internal(state,
                                                frame,
                                                destinationSlot,
                                                receiverSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC,
                                                ZR_TRUE,
                                                "SUPER_META_GET_STATIC_CACHED");
}

TZrBool ZrLibrary_AotRuntime_MetaSetStaticCached(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 receiverAndResultSlot,
                                                 TZrUInt32 assignedValueSlot,
                                                 TZrUInt32 cacheIndex) {
    return aot_runtime_meta_set_cached_internal(state,
                                                frame,
                                                receiverAndResultSlot,
                                                assignedValueSlot,
                                                cacheIndex,
                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC,
                                                ZR_TRUE,
                                                "SUPER_META_SET_STATIC_CACHED");
}

static TZrBool aot_runtime_own_value(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot,
                                     TZrBool (*operation)(SZrState *, SZrTypeValue *, SZrTypeValue *)) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL || operation == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!operation(state, destinationValue, sourceValue)) {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_OwnUnique(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_UniqueValue);
}

TZrBool ZrLibrary_AotRuntime_OwnBorrow(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_BorrowValue);
}

TZrBool ZrLibrary_AotRuntime_OwnLoan(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_LoanValue);
}

TZrBool ZrLibrary_AotRuntime_OwnShare(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_ShareValue);
}

TZrBool ZrLibrary_AotRuntime_OwnWeak(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_WeakValue);
}

TZrBool ZrLibrary_AotRuntime_OwnDetach(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_DetachValue);
}

TZrBool ZrLibrary_AotRuntime_OwnUpgrade(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    return aot_runtime_own_value(state, frame, destinationSlot, sourceSlot, ZrCore_Ownership_UpgradeValue);
}

TZrBool ZrLibrary_AotRuntime_OwnRelease(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Ownership_ReleaseValue(state, sourceValue);
    ZrCore_Value_ResetAsNull(destinationValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalEqual(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrBool equal;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    equal = ZrCore_Value_Equal(state, leftValue, rightValue);
    ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal ? ZR_TRUE : ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNotEqual(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (!ZrLibrary_AotRuntime_LogicalEqual(state, frame, destinationSlot, leftSlot, rightSlot) ||
        destinationPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
        return ZR_FALSE;
    }

    destinationValue->value.nativeObject.nativeBool =
            (TZrBool)!destinationValue->value.nativeObject.nativeBool;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessSigned(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftNumber = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftNumber = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightNumber = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightNumber = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber < rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterSigned(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber > rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualSigned(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber <= rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftNumber;
    TZrInt64 rightNumber;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: left operand is not integer-like");
        return ZR_FALSE;
    }
    if (!aot_runtime_extract_integer_like_value(rightValue, &rightNumber)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_SIGNED: right operand is not integer-like");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftNumber >= rightNumber ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_IsTruthy(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 sourceSlot,
                                      TZrBool *outTruthy) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || outTruthy == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "JUMP_IF: invalid condition slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "JUMP_IF: missing condition value");
        return ZR_FALSE;
    }

    *outTruthy = aot_runtime_value_is_truthy(state, sourceValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Add(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrCallInfo *callInfo;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL ||
        rightPointer == ZR_NULL || callInfo == ZR_NULL ||
        !ZrCore_Execution_Add(state,
                              callInfo,
                              ZrCore_Stack_GetValue(destinationPointer),
                              ZrCore_Stack_GetValue(leftPointer),
                              ZrCore_Stack_GetValue(rightPointer))) {
        aot_runtime_fail(state, runtimeState, "ADD: generated AOT helper failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    if (state->callInfoList != ZR_NULL && state->callInfoList->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
        state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Sub(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUB: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUB: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(leftValue->type) && ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          leftValue->value.nativeObject.nativeBool && rightValue->value.nativeObject.nativeBool,
                          ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt - rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt - rightUInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) && ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        leftDouble = leftValue->value.nativeObject.nativeDouble;
        rightDouble = rightValue->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble - rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SUB);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_Mul(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt * rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt * rightUInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt) &&
        !ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) &&
        !ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt * rightInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_numeric_double(leftValue, &leftDouble) &&
        aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble * rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_MUL);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_Div(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
        rightInt = rightValue->value.nativeObject.nativeInt64;
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt / rightInt);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        leftUInt = leftValue->value.nativeObject.nativeUInt64;
        rightUInt = rightValue->value.nativeObject.nativeUInt64;
        if (rightUInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt / rightUInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt) &&
        !ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) &&
        !ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt / rightInt);
        return ZR_TRUE;
    }

    if (aot_runtime_extract_numeric_double(leftValue, &leftDouble) &&
        aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        if (rightDouble == 0.0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble / rightDouble);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_DIV);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

static TZrBool aot_runtime_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1.0 : 0.0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool aot_runtime_extract_integer_like_value(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1 : 0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool aot_runtime_extract_unsigned_integer_like_value(const SZrTypeValue *value, TZrUInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1u : 0u;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool aot_runtime_eval_binary_numeric_float(EZrAotRuntimeFloatBinaryOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrFloat64 *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_AOT_RUNTIME_FLOAT_BINARY_ADD:
            *outResult = leftValue + rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_SUB:
            *outResult = leftValue - rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_MUL:
            *outResult = leftValue * rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_DIV:
            *outResult = leftValue / rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_MOD:
            *outResult = fmod(leftValue, rightValue);
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_FLOAT_BINARY_POW:
            *outResult = pow(leftValue, rightValue);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool aot_runtime_eval_binary_numeric_compare(EZrAotRuntimeCompareOp operation,
                                                       TZrFloat64 leftValue,
                                                       TZrFloat64 rightValue,
                                                       TZrBool *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_AOT_RUNTIME_COMPARE_GREATER:
            *outResult = leftValue > rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_LESS:
            *outResult = leftValue < rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL:
            *outResult = leftValue >= rightValue;
            return ZR_TRUE;
        case ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL:
            *outResult = leftValue <= rightValue;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrSize aot_runtime_close_scope_registrations(SZrState *state, TZrSize cleanupCount) {
    TZrSize closedCount = 0;
    TZrMemoryOffset savedStackTopOffset;
    SZrCallInfo *currentCallInfo;

    if (state == ZR_NULL || cleanupCount == 0) {
        return 0;
    }

    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    currentCallInfo = state->callInfoList;
    if (currentCallInfo != ZR_NULL &&
        state->stackTop.valuePointer < currentCallInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = currentCallInfo->functionTop.valuePointer;
    }

    while (closedCount < cleanupCount &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrCore_Closure_CloseStackValue(state, toBeClosed.valuePointer);
        ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);
        closedCount++;
    }

    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return closedCount;
}

static TZrBool aot_runtime_apply_float_binary_operation(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot,
                                                        EZrAotRuntimeFloatBinaryOp operation,
                                                        const TZrChar *instructionName) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrFloat64 resultValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: invalid stack slot", instructionName);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: missing value", instructionName);
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftNumber) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightNumber) ||
        !aot_runtime_eval_binary_numeric_float(operation, leftNumber, rightNumber, &resultValue)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, resultValue);
    return ZR_TRUE;
}

static TZrBool aot_runtime_apply_float_compare_operation(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         EZrAotRuntimeCompareOp operation,
                                                         const TZrChar *instructionName) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrBool resultValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: invalid stack slot", instructionName);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s: missing value", instructionName);
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftNumber) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightNumber) ||
        !aot_runtime_eval_binary_numeric_compare(operation, leftNumber, rightNumber, &resultValue)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, resultValue ? ZR_TRUE : ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

static TZrBool aot_runtime_reserve_temp_call_base(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 scratchSlotCount,
                                                  TZrStackValuePointer *outCallBase,
                                                  TZrUInt32 *outFunctionSlot) {
    SZrCallInfo *callerCallInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer reservedBase;

    if (outCallBase != ZR_NULL) {
        *outCallBase = ZR_NULL;
    }
    if (outFunctionSlot != ZR_NULL) {
        *outFunctionSlot = 0;
    }
    if (state == ZR_NULL || frame == ZR_NULL || scratchSlotCount == 0 || outCallBase == ZR_NULL ||
        outFunctionSlot == ZR_NULL || frame->slotBase == ZR_NULL) {
        return ZR_FALSE;
    }

    callerCallInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    callBase = callerCallInfo != ZR_NULL ? callerCallInfo->functionTop.valuePointer : state->stackTop.valuePointer;
    reservedBase = callBase != ZR_NULL ? ZrCore_Function_ReserveScratchSlots(state, scratchSlotCount, callBase) : ZR_NULL;
    if (reservedBase == ZR_NULL) {
        return ZR_FALSE;
    }

    callBase = callerCallInfo != ZR_NULL ? callerCallInfo->functionTop.valuePointer : reservedBase;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionBase.valuePointer != ZR_NULL) {
        frame->slotBase = frame->callInfo->functionBase.valuePointer + 1;
    }
    if (callBase == ZR_NULL || frame->slotBase == ZR_NULL || callBase < frame->slotBase) {
        return ZR_FALSE;
    }

    *outCallBase = callBase;
    *outFunctionSlot = (TZrUInt32)(callBase - frame->slotBase);
    return ZR_TRUE;
}

static TZrBool aot_runtime_call_temp_base_without_yield(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 destinationSlot,
                                                        TZrStackValuePointer callBase,
                                                        TZrUInt32 argumentCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || frame == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "META_CALL: invalid call target (callBase=%p destination=%p)",
                         (void *)callBase,
                         (void *)destinationPointer);
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    resultBase = ZrCore_Function_CallWithoutYieldAndRestoreAnchor(state, &callAnchor, 1);
    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (resultBase == ZR_NULL || destinationPointer == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "META_CALL: generic invoke failed (resultBase=%p destination=%p threadStatus=%u callInfo=%p)",
                         (void *)resultBase,
                         (void *)destinationPointer,
                         (unsigned)(state != ZR_NULL ? state->threadStatus : 0),
                         (void *)(state != ZR_NULL ? state->callInfoList : ZR_NULL));
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(resultBase));
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

static TZrBool aot_runtime_invoke_unary_meta(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             const SZrTypeValue *receiverValue,
                                             SZrFunction *metadataFunction) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase = ZR_NULL;
    TZrUInt32 functionSlot = 0;
    SZrTypeValue *functionValue;
    SZrTypeValue stableReceiver;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;
    ZrAotGeneratedDirectCall directCall;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || receiverValue == ZR_NULL || metadataFunction == ZR_NULL ||
        !aot_runtime_reserve_temp_call_base(state, frame, 2, &callBase, &functionSlot)) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (functionValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    ZrCore_Value_InitAsRawObject(state, functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction));
    ZrCore_Stack_CopyValue(state, callBase + 1, &stableReceiver);

    state->stackTop.valuePointer = callBase + 2;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    memset(&directCall, 0, sizeof(directCall));
    record = runtimeState != ZR_NULL ? aot_runtime_find_record_for_function(runtimeState, metadataFunction) : ZR_NULL;
    if (record != ZR_NULL && record->descriptor != ZR_NULL && record->descriptor->functionThunks != ZR_NULL) {
        functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
        if (functionIndex != UINT32_MAX && functionIndex < record->descriptor->functionThunkCount &&
            record->descriptor->functionThunks[functionIndex] != ZR_NULL) {
            if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                          frame,
                                                          destinationSlot,
                                                          functionSlot,
                                                          1,
                                                          metadataFunction,
                                                          functionIndex,
                                                          &directCall)) {
                return ZR_FALSE;
            }
            directCall.nativeFunction = record->descriptor->functionThunks[functionIndex];
            return ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,
                                                              frame,
                                                              &directCall,
                                                              destinationSlot,
                                                              functionSlot,
                                                              1,
                                                              1);
        }
    }

    return aot_runtime_call_temp_base_without_yield(state, frame, destinationSlot, callBase, 1);
}

static TZrBool aot_runtime_invoke_binary_meta(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              const SZrTypeValue *receiverValue,
                                              const SZrTypeValue *argumentValue,
                                              SZrFunction *metadataFunction) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase = ZR_NULL;
    TZrUInt32 functionSlot = 0;
    SZrTypeValue *functionValue;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableArgument;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;
    ZrAotGeneratedDirectCall directCall;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || receiverValue == ZR_NULL || argumentValue == ZR_NULL ||
        metadataFunction == ZR_NULL || !aot_runtime_reserve_temp_call_base(state, frame, 3, &callBase, &functionSlot)) {
        return ZR_FALSE;
    }

    functionValue = ZrCore_Stack_GetValue(callBase);
    if (functionValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    stableArgument = *argumentValue;
    ZrCore_Value_InitAsRawObject(state, functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction));
    ZrCore_Stack_CopyValue(state, callBase + 1, &stableReceiver);
    ZrCore_Stack_CopyValue(state, callBase + 2, &stableArgument);

    state->stackTop.valuePointer = callBase + 3;
    if (frame->callInfo != ZR_NULL && frame->callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        frame->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    memset(&directCall, 0, sizeof(directCall));
    record = runtimeState != ZR_NULL ? aot_runtime_find_record_for_function(runtimeState, metadataFunction) : ZR_NULL;
    if (record != ZR_NULL && record->descriptor != ZR_NULL && record->descriptor->functionThunks != ZR_NULL) {
        functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
        if (functionIndex != UINT32_MAX && functionIndex < record->descriptor->functionThunkCount &&
            record->descriptor->functionThunks[functionIndex] != ZR_NULL) {
            if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                          frame,
                                                          destinationSlot,
                                                          functionSlot,
                                                          2,
                                                          metadataFunction,
                                                          functionIndex,
                                                          &directCall)) {
                return ZR_FALSE;
            }
            directCall.nativeFunction = record->descriptor->functionThunks[functionIndex];
            return ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,
                                                              frame,
                                                              &directCall,
                                                              destinationSlot,
                                                              functionSlot,
                                                              2,
                                                              1);
        }
    }

    return aot_runtime_call_temp_base_without_yield(state, frame, destinationSlot, callBase, 2);
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

TZrBool ZrLibrary_AotRuntime_AddIntConst(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt + rightInt);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SubInt(SZrState *state,
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
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(destinationPointer),
                          nativeInt64,
                          leftInt - rightInt,
                          leftValue->type);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "SUB_INT requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble - rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SubIntConst(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(destinationPointer),
                          nativeInt64,
                          leftInt - rightInt,
                          leftValue->type);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "SUB_INT_CONST requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble - rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseXor(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_XOR: operands must be integer values");
        return ZR_FALSE;
    }

    leftInt = ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type) ? leftValue->value.nativeObject.nativeInt64
                                                           : (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    rightInt = ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type) ? rightValue->value.nativeObject.nativeInt64
                                                             : (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt ^ rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_DivSigned(SZrState *state,
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
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt / rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_SIGNED requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble / rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_DivSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        leftInt = leftValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        leftInt = (TZrInt64)leftValue->value.nativeObject.nativeUInt64;
    } else {
        leftInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue->type)) {
        rightInt = rightValue->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        rightInt = (TZrInt64)rightValue->value.nativeObject.nativeUInt64;
    } else {
        rightInt = 0;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "divide by zero");
        }
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(destinationPointer), leftInt / rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_SIGNED_CONST requires numeric operands");
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble / rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MulSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (leftValue == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state,
                               ZrCore_Stack_GetValue(destinationPointer),
                               leftValue->value.nativeObject.nativeInt64 * rightValue->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble * rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MulSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *leftValue;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        ZrCore_Value_InitAsInt(state,
                               ZrCore_Stack_GetValue(destinationPointer),
                               leftValue->value.nativeObject.nativeInt64 * rightValue->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(destinationPointer), leftDouble * rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Neg(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 sourceSlot) {
    SZrMeta *metaValue;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -sourceValue->value.nativeObject.nativeInt64,
                          sourceValue->type);
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, -(TZrInt64)sourceValue->value.nativeObject.nativeUInt64);
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          -sourceValue->value.nativeObject.nativeDouble,
                          sourceValue->type);
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_NEG);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_Mod(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD: missing value");
        return ZR_FALSE;
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(leftValue->type) || ZR_VALUE_IS_TYPE_BOOL(leftValue->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(rightValue->type) || ZR_VALUE_IS_TYPE_BOOL(rightValue->type))) {
        if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
                TZrUInt64 rightUnsigned = rightValue->value.nativeObject.nativeUInt64;

                if (rightUnsigned == 0u) {
                    ZrCore_Debug_RunError(state, "modulo by zero");
                }
                ZR_VALUE_FAST_SET(destinationValue,
                                  nativeUInt64,
                                  leftValue->value.nativeObject.nativeUInt64 % rightUnsigned,
                                  ZR_VALUE_TYPE_UINT64);
                return ZR_TRUE;
            }

            if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
                !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
                aot_runtime_fail(state, runtimeState, "MOD: integer-like extraction failed");
                return ZR_FALSE;
            }
            if (rightInt == 0) {
                ZrCore_Debug_RunError(state, "modulo by zero");
            }
            if (rightInt < 0) {
                rightInt = -rightInt;
            }
            ZrCore_Value_InitAsInt(state, destinationValue, leftInt % rightInt);
            return ZR_TRUE;
        }

        if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
            !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
            aot_runtime_fail(state, runtimeState, "MOD: numeric extraction failed");
            return ZR_FALSE;
        }
        if (rightDouble == 0.0) {
            ZrCore_Debug_RunError(state, "modulo by zero");
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
        return ZR_TRUE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_MOD);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_ModSignedConst(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 constantIndex) {
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    const SZrTypeValue *rightValue = aot_runtime_frame_constant(frame, constantIndex);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightValue == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type)) {
        if (!aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
            !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
            return ZR_FALSE;
        }
        if (rightInt == 0) {
            ZrCore_Debug_RunError(state, "modulo by zero");
        }
        if (rightInt < 0) {
            rightInt = -rightInt;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, leftInt % rightInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MOD_SIGNED_CONST requires numeric operands");
    }
    if (rightDouble == 0.0) {
        ZrCore_Debug_RunError(state, "modulo by zero");
    }
    ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToString(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    SZrString *resultString;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_STRING: missing value");
        return ZR_FALSE;
    }

    resultString = ZrCore_Value_ConvertToString(state, sourceValue);
    if (resultString != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString));
    } else {
        ZrCore_Value_ResetAsNull(destinationValue);
    }
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

TZrBool ZrLibrary_AotRuntime_SetMember(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 sourceSlot,
                                       TZrUInt32 receiverSlot,
                                       TZrUInt32 memberId) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    SZrString *memberSymbol = ZR_NULL;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL ||
        !aot_runtime_resolve_member_symbol(aot_runtime_frame_function(frame), memberId, &memberSymbol)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: invalid member id");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetMember(state, receiverValue, memberSymbol, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_MEMBER: receiver must be a writable object member");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetByIndex(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 receiverSlot,
                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue stableReceiver;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL || keyValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: invalid slot value");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    if (!ZrCore_Object_GetByIndex(state, &stableReceiver, keyValue, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "GET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SetByIndex(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 sourceSlot,
                                        TZrUInt32 receiverSlot,
                                        TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL ||
        !ZrCore_Object_SetByIndex(state, receiverValue, keyValue, sourceValue)) {
        aot_runtime_fail(state, runtimeState, "SET_BY_INDEX: receiver must be an object or array");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArrayGetInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue stableReceiver;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL || keyValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_GET_INT: invalid slot value");
        return ZR_FALSE;
    }

    stableReceiver = *receiverValue;
    if (!ZrCore_Object_SuperArrayGetInt(state, &stableReceiver, keyValue, destinationValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_GET_INT: receiver must be an array-like object with int index");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArraySetInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 keySlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer keyPointer = aot_runtime_frame_slot(frame, keySlot);
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL || receiverPointer == ZR_NULL || keyPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot");
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    keyValue = ZrCore_Stack_GetValue(keyPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (receiverValue == ZR_NULL || keyValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_SET_INT: invalid slot value");
        return ZR_FALSE;
    }

    if (!ZrCore_Object_SuperArraySetInt(state, receiverValue, keyValue, sourceValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_SET_INT: receiver must be an array-like object with int index");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || receiverPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    receiverValue = ZrCore_Stack_GetValue(receiverPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || receiverValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT: invalid slot value");
        return ZR_FALSE;
    }

    if (!ZrCore_Object_SuperArrayAddInt(state, receiverValue, sourceValue, destinationValue)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_ADD_INT: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 receiverBaseSlot,
                                               TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue ignoredResult;
    TZrUInt32 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid slot value");
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&ignoredResult);
    for (index = 0; index < 4; index++) {
        TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverBaseSlot + index);
        SZrTypeValue *receiverValue;

        if (receiverPointer == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid receiver slot");
            return ZR_FALSE;
        }

        receiverValue = ZrCore_Stack_GetValue(receiverPointer);
        if (receiverValue == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4: invalid receiver value");
            return ZR_FALSE;
        }

        if (!ZrCore_Object_SuperArrayAddInt(state, receiverValue, sourceValue, &ignoredResult)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "SUPER_ARRAY_ADD_INT4: receiver must be an array-like object with int payload");
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArrayAddInt4Const(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 receiverBaseSlot,
                                                    TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    const SZrTypeValue *sourceValue;
    SZrTypeValue ignoredResult;
    TZrUInt32 index;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid constant");
        return ZR_FALSE;
    }

    sourceValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: constant payload must be int");
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&ignoredResult);
    for (index = 0; index < 4; index++) {
        TZrStackValuePointer receiverPointer = aot_runtime_frame_slot(frame, receiverBaseSlot + index);
        SZrTypeValue *receiverValue;

        if (receiverPointer == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid receiver slot");
            return ZR_FALSE;
        }

        receiverValue = ZrCore_Stack_GetValue(receiverPointer);
        if (receiverValue == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_ADD_INT4_CONST: invalid receiver value");
            return ZR_FALSE;
        }

        if (!ZrCore_Object_SuperArrayAddInt(state, receiverValue, sourceValue, &ignoredResult)) {
            aot_runtime_fail(state,
                             runtimeState,
                             "SUPER_ARRAY_ADD_INT4_CONST: receiver must be an array-like object with int payload");
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SuperArrayFillInt4Const(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 receiverBaseSlot,
                                                     TZrUInt32 countSlot,
                                                     TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *function;
    TZrStackValuePointer countPointer;
    SZrTypeValue *countValue;
    const SZrTypeValue *sourceValue;
    TZrStackValuePointer receiverBase;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = aot_runtime_frame_function(frame);
    countPointer = aot_runtime_frame_slot(frame, countSlot);
    receiverBase = aot_runtime_frame_slot(frame, receiverBaseSlot);
    if (state == ZR_NULL || function == ZR_NULL || countPointer == ZR_NULL || receiverBase == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: invalid operand");
        return ZR_FALSE;
    }

    countValue = ZrCore_Stack_GetValue(countPointer);
    if (countValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(countValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: repeat count must be int");
        return ZR_FALSE;
    }

    sourceValue = &function->constantValueList[constantIndex];
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        aot_runtime_fail(state, runtimeState, "SUPER_ARRAY_FILL_INT4_CONST: constant payload must be int");
        return ZR_FALSE;
    }

    if (!ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(state,
                                                         receiverBase,
                                                         countValue->value.nativeObject.nativeInt64,
                                                         sourceValue->value.nativeObject.nativeInt64)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "SUPER_ARRAY_FILL_INT4_CONST: receiver must be an array-like object with int payload");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_IterInit(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 iterableSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iterablePointer = aot_runtime_frame_slot(frame, iterableSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iterableValue;
    SZrTypeValue stableIterable;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iterablePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iterableValue = ZrCore_Stack_GetValue(iterablePointer);
    if (destinationValue == ZR_NULL || iterableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterable = *iterableValue;
    if (!ZrCore_Object_IterInit(state, &stableIterable, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_INIT: receiver does not satisfy iterable contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_IterMoveNext(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 iteratorSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iteratorPointer = aot_runtime_frame_slot(frame, iteratorSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iteratorValue;
    SZrTypeValue stableIterator;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iteratorPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iteratorValue = ZrCore_Stack_GetValue(iteratorPointer);
    if (destinationValue == ZR_NULL || iteratorValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterator = *iteratorValue;
    if (!ZrCore_Object_IterMoveNext(state, &stableIterator, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_MOVE_NEXT: receiver does not satisfy iterator contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_IterCurrent(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 iteratorSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer iteratorPointer = aot_runtime_frame_slot(frame, iteratorSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *iteratorValue;
    SZrTypeValue stableIterator;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || iteratorPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: invalid slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    iteratorValue = ZrCore_Stack_GetValue(iteratorPointer);
    if (destinationValue == ZR_NULL || iteratorValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: invalid slot value");
        return ZR_FALSE;
    }

    stableIterator = *iteratorValue;
    if (!ZrCore_Object_IterCurrent(state, &stableIterator, destinationValue)) {
        aot_runtime_fail(state, runtimeState, "ITER_CURRENT: receiver does not satisfy iterator contract");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Call(SZrState *state,
                                  ZrAotGeneratedFrame *frame,
                                  TZrUInt32 destinationSlot,
                                  TZrUInt32 functionSlot,
                                  TZrUInt32 argumentCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callAnchor;
    SZrTypeValue *callableValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;

    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CALL: invalid frame");
        return ZR_FALSE;
    }

    callInfo = state->callInfoList;
    callBase = aot_runtime_frame_slot(frame, functionSlot);
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (callInfo == ZR_NULL || callBase == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "CALL: invalid call target (callInfo=%p callBase=%p destination=%p)",
                         (void *)callInfo,
                         (void *)callBase,
                         (void *)destinationPointer);
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(callBase);
    if (callableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "CALL: missing callable value");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callBase = ZrCore_Function_CallAndRestoreAnchor(state, &callAnchor, 1);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "CALL: generic invoke failed (callBase=%p threadStatus=%u callInfo=%p callableType=%u isNative=%u)",
                         (void *)callBase,
                         (unsigned)state->threadStatus,
                         (void *)state->callInfoList,
                         (unsigned)callableValue->type,
                         (unsigned)callableValue->isNative);
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), ZrCore_Stack_GetValue(callBase));
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CallPreparedOrGeneric(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   ZrAotGeneratedDirectCall *directCall,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 functionSlot,
                                                   TZrUInt32 argumentCount,
                                                   TZrUInt32 resultCount) {
    SZrLibraryAotRuntimeState *runtimeState;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!directCall->prepared) {
        return ZrLibrary_AotRuntime_Call(state, frame, destinationSlot, functionSlot, argumentCount);
    }

    if (directCall->nativeFunction == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call is missing native thunk");
        return ZR_FALSE;
    }

    if (!directCall->nativeFunction(state)) {
        return ZR_FALSE;
    }

    return ZrLibrary_AotRuntime_FinishDirectCall(state, frame, directCall, resultCount);
}

TZrBool ZrLibrary_AotRuntime_PrepareDirectCall(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount,
                                               ZrAotGeneratedDirectCall *directCall) {
    return aot_runtime_try_prepare_direct_native_call(state,
                                                      frame,
                                                      destinationSlot,
                                                      functionSlot,
                                                      argumentCount,
                                                      directCall);
}

TZrBool ZrLibrary_AotRuntime_PrepareMetaCall(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 argumentCount,
                                             ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    SZrFunction *metadataFunction = ZR_NULL;
    SZrLibraryAotLoadedModule *record;
    TZrUInt32 functionIndex;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callBase = aot_runtime_frame_slot(frame, receiverSlot);
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_meta_target(state, callBase, argumentCount, &metadataFunction)) {
        aot_runtime_fail(state, runtimeState, "generated AOT meta call target does not define @call");
        return ZR_FALSE;
    }

    record = aot_runtime_find_record_for_function(runtimeState, metadataFunction);
    if (record == ZR_NULL || record->descriptor == ZR_NULL || record->descriptor->functionThunks == ZR_NULL) {
        return ZR_TRUE;
    }

    functionIndex = aot_runtime_find_function_index_in_record(record, metadataFunction);
    if (functionIndex == UINT32_MAX || functionIndex >= record->descriptor->functionThunkCount ||
        record->descriptor->functionThunks[functionIndex] == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  receiverSlot,
                                                  argumentCount + 1,
                                                  metadataFunction,
                                                  functionIndex,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = record->descriptor->functionThunks[functionIndex];
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_PrepareStaticDirectCall(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 calleeFunctionIndex,
                                                     ZrAotGeneratedDirectCall *directCall) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrLibraryAotLoadedModule *record;
    SZrFunction *metadataFunction;

    if (directCall != ZR_NULL) {
        memset(directCall, 0, sizeof(*directCall));
    }

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    record = frame != ZR_NULL ? (SZrLibraryAotLoadedModule *)frame->recordHandle : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || runtimeState == ZR_NULL || record == ZR_NULL ||
        record->descriptor == ZR_NULL || record->functionTable == ZR_NULL || record->descriptor->functionThunks == ZR_NULL ||
        calleeFunctionIndex >= record->functionCount || calleeFunctionIndex >= record->descriptor->functionThunkCount ||
        record->descriptor->functionThunks[calleeFunctionIndex] == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call is missing callee thunk metadata");
        return ZR_FALSE;
    }

    metadataFunction = record->functionTable[calleeFunctionIndex];
    if (metadataFunction == ZR_NULL) {
        aot_runtime_fail(state,
                         runtimeState,
                         "generated AOT static direct call cannot resolve function index %u",
                         (unsigned)calleeFunctionIndex);
        return ZR_FALSE;
    }

    if (!aot_runtime_prepare_vm_direct_call_frame(state,
                                                  frame,
                                                  destinationSlot,
                                                  functionSlot,
                                                  argumentCount,
                                                  metadataFunction,
                                                  calleeFunctionIndex,
                                                  directCall)) {
        return ZR_FALSE;
    }

    directCall->nativeFunction = record->descriptor->functionThunks[calleeFunctionIndex];
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_FinishDirectCall(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              ZrAotGeneratedDirectCall *directCall,
                                              TZrUInt32 resultCount) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = directCall != ZR_NULL ? directCall->calleeCallInfo : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || directCall == ZR_NULL || !directCall->prepared || callInfo == ZR_NULL ||
        directCall->callerCallInfo == ZR_NULL || frame->callInfo != directCall->callerCallInfo ||
        state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call finish is missing active call info");
        return ZR_FALSE;
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE && callInfo->functionBase.valuePointer != ZR_NULL) {
        TZrStackValuePointer returnTop = state->stackTop.valuePointer;
        if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        }
        // Direct AOT thunk invocation bypasses ZrCore_Function_PreCall, so it must
        // still close any open upvalues before PostCall tears down the callee frame.
        ZrCore_Closure_CloseClosure(state,
                                    callInfo->functionBase.valuePointer + 1,
                                    ZR_THREAD_STATUS_INVALID,
                                    ZR_FALSE);
        state->stackTop.valuePointer = returnTop;
    }

    ZrCore_Function_PostCall(state, callInfo, resultCount);
    if (state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct call lost caller frame");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    memset(directCall, 0, sizeof(*directCall));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Try(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 handlerIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    const SZrFunction *function;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || callInfo == ZR_NULL || function == ZR_NULL || handlerIndex >= function->exceptionHandlerCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT TRY has invalid handler index");
        return ZR_FALSE;
    }

    if (!execution_push_exception_handler(state, callInfo, handlerIndex)) {
        aot_runtime_fail(state, runtimeState, "generated AOT TRY failed to push exception handler");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_EndTry(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 handlerIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrVmExceptionHandlerState *handlerState;
    const SZrFunction *function;
    const SZrFunctionExceptionHandlerInfo *handlerInfo;
    SZrCallInfo *callInfo;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    function = aot_runtime_frame_function(frame);
    if (state == ZR_NULL || callInfo == ZR_NULL || function == ZR_NULL || handlerIndex >= function->exceptionHandlerCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT END_TRY has invalid handler index");
        return ZR_FALSE;
    }

    handlerState = execution_find_handler_state(state, callInfo, handlerIndex);
    handlerInfo = &function->exceptionHandlerList[handlerIndex];
    if (handlerState != ZR_NULL) {
        if (handlerInfo->hasFinally) {
            handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
        } else {
            execution_pop_exception_handler(state, handlerState);
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_Throw(SZrState *state,
                                   ZrAotGeneratedFrame *frame,
                                   TZrUInt32 sourceSlot,
                                   TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;
    TZrStackValuePointer sourcePointer;
    SZrTypeValue payload;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || callInfo == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT THROW has invalid payload slot");
        return ZR_FALSE;
    }

    execution_clear_pending_control(state);
    payload = *ZrCore_Stack_GetValue(sourcePointer);
    if (!ZrCore_Exception_NormalizeThrownValue(state, &payload, callInfo, ZR_THREAD_STATUS_RUNTIME_ERROR)) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            aot_runtime_fail(state, runtimeState, "generated AOT THROW failed to normalize exception");
            return ZR_FALSE;
        }
    }

    return aot_runtime_resume_exception_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_Catch(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer;
    SZrTypeValue *destination;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT CATCH has invalid destination slot");
        return ZR_FALSE;
    }

    destination = ZrCore_Stack_GetValue(destinationPointer);
    if (destination == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT CATCH is missing destination value");
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Value_Copy(state, destination, &state->currentException);
        ZrCore_Exception_ClearCurrent(state);
    } else {
        ZrCore_Value_ResetAsNull(destination);
    }
    execution_clear_pending_control(state);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_EndFinally(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 handlerIndex,
                                        TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrVmExceptionHandlerState *handlerState;
    SZrCallInfo *resumeCallInfo;
    TZrStackValuePointer targetSlot;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT END_FINALLY is missing call frame");
        return ZR_FALSE;
    }

    handlerState = execution_find_handler_state(state, frame->callInfo, handlerIndex);
    if (handlerState != ZR_NULL) {
        execution_pop_exception_handler(state, handlerState);
    }

    switch (state->pendingControl.kind) {
        case ZR_VM_PENDING_CONTROL_NONE:
            return ZR_TRUE;
        case ZR_VM_PENDING_CONTROL_EXCEPTION:
            resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : frame->callInfo;
            if (!aot_runtime_refresh_frame_from_callinfo(state, frame, resumeCallInfo)) {
                return ZR_FALSE;
            }
            return aot_runtime_resume_exception_in_current_frame(state, frame, outResumeInstructionIndex);
        case ZR_VM_PENDING_CONTROL_RETURN:
        case ZR_VM_PENDING_CONTROL_BREAK:
        case ZR_VM_PENDING_CONTROL_CONTINUE:
            resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : frame->callInfo;
            if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN && state->pendingControl.hasValue &&
                resumeCallInfo != ZR_NULL && resumeCallInfo->functionBase.valuePointer != ZR_NULL) {
                targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;
                ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);
            }
            if (!aot_runtime_refresh_frame_from_callinfo(state, frame, resumeCallInfo)) {
                return ZR_FALSE;
            }
            if (aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex)) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
        default:
            execution_clear_pending_control(state);
            return ZR_TRUE;
    }
}

TZrBool ZrLibrary_AotRuntime_SetPendingReturn(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 sourceSlot,
                                              TZrUInt32 targetInstructionIndex,
                                              TZrUInt32 *outResumeInstructionIndex) {
    TZrStackValuePointer sourcePointer;
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_RETURN has invalid source slot");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_RETURN,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  sourceSlot,
                                  ZrCore_Stack_GetValue(sourcePointer));
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetPendingBreak(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 targetInstructionIndex,
                                             TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_BREAK is missing call frame");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_BREAK,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  0,
                                  ZR_NULL);
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetPendingContinue(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 targetInstructionIndex,
                                                TZrUInt32 *outResumeInstructionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo;

    if (outResumeInstructionIndex != ZR_NULL) {
        *outResumeInstructionIndex = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;
    }

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
    if (state == ZR_NULL || frame == ZR_NULL || callInfo == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT SET_PENDING_CONTINUE is missing call frame");
        return ZR_FALSE;
    }

    execution_set_pending_control(state,
                                  ZR_VM_PENDING_CONTROL_CONTINUE,
                                  callInfo,
                                  (TZrMemoryOffset)targetInstructionIndex,
                                  0,
                                  ZR_NULL);
    return aot_runtime_resume_pending_control_in_current_frame(state, frame, outResumeInstructionIndex);
}

TZrBool ZrLibrary_AotRuntime_SetConstant(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 sourceSlot,
                                         TZrUInt32 constantIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrFunction *function;
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    function = (SZrFunction *)aot_runtime_frame_function(frame);
    if (state == ZR_NULL || function == ZR_NULL || sourcePointer == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        aot_runtime_fail(state, runtimeState, "SET_CONSTANT: invalid operand");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SET_CONSTANT: missing source value");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, &function->constantValueList[constantIndex], sourceValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GetSubFunction(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 childFunctionIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrFunction *ownerFunction;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer functionBase;
    SZrTypeValue *destinationValue;
    SZrTypeValue *functionBaseValue = ZR_NULL;
    SZrClosure *currentClosure = ZR_NULL;
    SZrClosureValue **captureList = ZR_NULL;
    SZrFunction *childFunction;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    ownerFunction = aot_runtime_frame_function(frame);
    functionBase = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo->functionBase.valuePointer : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || ownerFunction == ZR_NULL || destinationPointer == ZR_NULL ||
        frame->slotBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION: invalid frame");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "GET_SUB_FUNCTION: invalid destination slot");
        return ZR_FALSE;
    }

    if (functionBase != ZR_NULL) {
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    }
    if (functionBaseValue != ZR_NULL && functionBaseValue->type == ZR_VALUE_TYPE_CLOSURE &&
        !functionBaseValue->isNative) {
        currentClosure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
        captureList = currentClosure != ZR_NULL ? currentClosure->closureValuesExtend : ZR_NULL;
    }

    if (childFunctionIndex >= ownerFunction->childFunctionLength) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    childFunction = &((SZrFunction *)ownerFunction)->childFunctionList[childFunctionIndex];
    ZrCore_Closure_PushToStack(state, childFunction, captureList, frame->slotBase, destinationPointer);
    destinationValue->type = ZR_VALUE_TYPE_CLOSURE;
    destinationValue->isGarbageCollectable = ZR_TRUE;
    destinationValue->isNative = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MarkToBeClosed(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer slotPointer = aot_runtime_frame_slot(frame, slotIndex);

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || slotPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MARK_TO_BE_CLOSED: invalid slot");
        return ZR_FALSE;
    }

    ZrCore_Closure_ToBeClosedValueClosureNew(state, slotPointer);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_CloseScope(SZrState *state, ZrAotGeneratedFrame *frame, TZrUInt32 cleanupCount) {
    ZR_UNUSED_PARAMETER(frame);

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    aot_runtime_close_scope_registrations(state, cleanupCount);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToBool(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_BOOL: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_BOOL: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_BOOL);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue = ZrCore_Stack_GetValue(destinationPointer);
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZR_VALUE_FAST_SET(destinationValue, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      aot_runtime_value_is_truthy(state, sourceValue) ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToInt(SZrState *state,
                                   ZrAotGeneratedFrame *frame,
                                   TZrUInt32 destinationSlot,
                                   TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_INT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_INT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_INT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue = ZrCore_Stack_GetValue(destinationPointer);
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, 0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_INT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, (TZrInt64)sourceValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, (TZrInt64)sourceValue->value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsInt(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1 : 0);
    } else {
        ZrCore_Value_InitAsInt(state, destinationValue, 0);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToUInt(SZrState *state,
                                    ZrAotGeneratedFrame *frame,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_UINT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_UINT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_UINT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue = ZrCore_Stack_GetValue(destinationPointer);
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, 0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, (TZrUInt64)sourceValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, (TZrUInt64)sourceValue->value.nativeObject.nativeDouble);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1u : 0u);
    } else {
        ZrCore_Value_InitAsUInt(state, destinationValue, 0u);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ToFloat(SZrState *state,
                                     ZrAotGeneratedFrame *frame,
                                     TZrUInt32 destinationSlot,
                                     TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *sourceValue;
    SZrTypeValue *destinationValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_FLOAT: invalid stack slot");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "TO_FLOAT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, sourceValue, ZR_META_TO_FLOAT);
    if (metaValue != ZR_NULL && metaValue->function != ZR_NULL) {
        if (!aot_runtime_invoke_unary_meta(state, frame, destinationSlot, sourceValue, metaValue->function)) {
            return ZR_FALSE;
        }
        destinationValue = ZrCore_Stack_GetValue(destinationPointer);
        if (destinationValue != ZR_NULL && ZR_VALUE_IS_TYPE_FLOAT(destinationValue->type)) {
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsFloat(state, destinationValue, 0.0);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, (TZrFloat64)sourceValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, (TZrFloat64)sourceValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZrCore_Value_InitAsFloat(state, destinationValue, sourceValue->value.nativeObject.nativeBool ? 1.0 : 0.0);
    } else {
        ZrCore_Value_InitAsFloat(state, destinationValue, 0.0);
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_AddFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_ADD,
                                                    "ADD_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_SubFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_SUB,
                                                    "SUB_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_MulUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MUL_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt * rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MUL_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble * rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_MulFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_MUL,
                                                    "MUL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_DivUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "DIV_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "divide by zero");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt / rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "DIV_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, leftDouble / rightDouble);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_DivFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_DIV,
                                                    "DIV_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_ModUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;
    TZrFloat64 leftDouble;
    TZrFloat64 rightDouble;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "MOD_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "modulo by zero");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, leftUInt % rightUInt);
        return ZR_TRUE;
    }

    if (!aot_runtime_extract_numeric_double(leftValue, &leftDouble) ||
        !aot_runtime_extract_numeric_double(rightValue, &rightDouble)) {
        ZrCore_Debug_RunError(state, "MOD_UNSIGNED requires numeric operands");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsFloat(state, destinationValue, fmod(leftDouble, rightDouble));
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ModFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_MOD,
                                                    "MOD_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_Pow(SZrState *state,
                                 ZrAotGeneratedFrame *frame,
                                 TZrUInt32 destinationSlot,
                                 TZrUInt32 leftSlot,
                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_PowSigned(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_SIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_SIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_integer_like_value(leftValue, &leftInt) &&
        aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        if ((leftInt == 0 && rightInt <= 0) || leftInt < 0) {
            ZrCore_Debug_RunError(state, "power domain error");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(state, destinationValue, ZrCore_Math_IntPower(leftInt, rightInt));
        return ZR_TRUE;
    }

    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_SIGNED");
}

TZrBool ZrLibrary_AotRuntime_PowUnsigned(SZrState *state,
                                         ZrAotGeneratedFrame *frame,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 leftSlot,
                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "POW_UNSIGNED: missing value");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(leftValue->type) && ZR_VALUE_IS_TYPE_INT(rightValue->type) &&
        aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) &&
        aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        if (leftUInt == 0u && rightUInt == 0u) {
            ZrCore_Debug_RunError(state, "power domain error");
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(state, destinationValue, ZrCore_Math_UIntPower(leftUInt, rightUInt));
        return ZR_TRUE;
    }

    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_UNSIGNED");
}

TZrBool ZrLibrary_AotRuntime_PowFloat(SZrState *state,
                                      ZrAotGeneratedFrame *frame,
                                      TZrUInt32 destinationSlot,
                                      TZrUInt32 leftSlot,
                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_binary_operation(state,
                                                    frame,
                                                    destinationSlot,
                                                    leftSlot,
                                                    rightSlot,
                                                    ZR_AOT_RUNTIME_FLOAT_BINARY_POW,
                                                    "POW_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_ShiftLeft(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SHIFT_LEFT);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_ShiftLeftInt(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "SHIFT_LEFT_INT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt << rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ShiftRight(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT: missing value");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_SHIFT_RIGHT);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    return aot_runtime_invoke_binary_meta(state, frame, destinationSlot, leftValue, rightValue, metaValue->function);
}

TZrBool ZrLibrary_AotRuntime_ShiftRightInt(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "SHIFT_RIGHT_INT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt >> rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalNot(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_NOT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_NOT: missing value");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      aot_runtime_value_is_truthy(state, sourceValue) ? ZR_FALSE : ZR_TRUE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalAnd(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_BOOL(leftValue->type) || !ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_AND: operands must be bool");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeBool && rightValue->value.nativeObject.nativeBool,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalOr(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_BOOL(leftValue->type) || !ZR_VALUE_IS_TYPE_BOOL(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_OR: operands must be bool");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftValue->value.nativeObject.nativeBool || rightValue->value.nativeObject.nativeBool,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterUnsigned(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt > rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterFloat(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_GREATER,
                                                     "LOGICAL_GREATER_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalLessUnsigned(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt < rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessFloat(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_LESS,
                                                     "LOGICAL_LESS_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_GREATER_EQUAL_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt >= rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_GREATER_EQUAL,
                                                     "LOGICAL_GREATER_EQUAL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "LOGICAL_LESS_EQUAL_UNSIGNED: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue,
                      nativeBool,
                      leftUInt <= rightUInt ? ZR_TRUE : ZR_FALSE,
                      ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_LogicalLessEqualFloat(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    return aot_runtime_apply_float_compare_operation(state,
                                                     frame,
                                                     destinationSlot,
                                                     leftSlot,
                                                     rightSlot,
                                                     ZR_AOT_RUNTIME_COMPARE_LESS_EQUAL,
                                                     "LOGICAL_LESS_EQUAL_FLOAT");
}

TZrBool ZrLibrary_AotRuntime_BitwiseNot(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;
    TZrInt64 sourceInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(sourceValue->type) || !aot_runtime_extract_integer_like_value(sourceValue, &sourceInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_NOT: operand must be integer");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, ~sourceInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseAnd(SZrState *state,
                                        ZrAotGeneratedFrame *frame,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_AND: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt & rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseOr(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 leftSlot,
                                       TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_OR: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt | rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseShiftLeft(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrInt64 leftInt;
    TZrInt64 rightInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_integer_like_value(leftValue, &leftInt) ||
        !aot_runtime_extract_integer_like_value(rightValue, &rightInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_LEFT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, leftInt << rightInt, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_BitwiseShiftRight(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    TZrUInt64 leftUInt;
    TZrUInt64 rightUInt;

    runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: missing value");
        return ZR_FALSE;
    }
    if (!ZR_VALUE_IS_TYPE_INT(leftValue->type) || !ZR_VALUE_IS_TYPE_INT(rightValue->type) ||
        !aot_runtime_extract_unsigned_integer_like_value(leftValue, &leftUInt) ||
        !aot_runtime_extract_unsigned_integer_like_value(rightValue, &rightUInt)) {
        aot_runtime_fail(state, runtimeState, "BITWISE_SHIFT_RIGHT: operands must be integer values");
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeInt64, (TZrInt64)(leftUInt >> rightUInt), ZR_VALUE_TYPE_INT64);
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
    callInfo = frame != ZR_NULL && frame->callInfo != ZR_NULL ? frame->callInfo : (state != ZR_NULL ? state->callInfoList : ZR_NULL);
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

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);

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

TZrInt64 ZrLibrary_AotRuntime_FailGeneratedFunction(SZrState *state, const ZrAotGeneratedFrame *frame) {
    SZrLibraryAotRuntimeState *runtimeState =
            state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    TZrUInt32 functionIndex = frame != ZR_NULL ? frame->functionIndex : UINT32_MAX;
    TZrUInt32 instructionIndex =
            frame != ZR_NULL ? frame->currentInstructionIndex : ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;

    if (runtimeState != ZR_NULL && runtimeState->lastError[0] != '\0') {
        return 0;
    }

    if (functionIndex == UINT32_MAX) {
        aot_runtime_fail(state, runtimeState, "generated AOT function failed before frame initialization");
        return 0;
    }

    aot_runtime_fail(state,
                     runtimeState,
                     "generated AOT function failed: functionIndex=%u instructionIndex=%u",
                     (unsigned)functionIndex,
                     instructionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ? UINT32_MAX : (unsigned)instructionIndex);
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
    SZrTypeValue *currentClosureValue;
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

    currentClosureValue = ZrCore_Stack_GetValue(sourceBase);
    if (!aot_runtime_project_closure_into_vm_shim(state, currentClosureValue, shimFunction, &closure)) {
        aot_runtime_fail(state,
                         runtimeState,
                         "AOT closure shim failed to project captures for module '%s'",
                         record->moduleName != ZR_NULL ? record->moduleName : "<unknown>");
        return 0;
    }

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
