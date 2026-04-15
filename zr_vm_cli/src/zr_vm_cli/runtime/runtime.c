#include "runtime/runtime.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "project/project.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"

#if defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
#include "zr_vm_lib_debug/debug.h"
#endif

typedef struct ZrCliExecuteRequest {
    SZrFunction *function;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrCliExecuteRequest;

static TZrBool zr_cli_runtime_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void zr_cli_runtime_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!zr_cli_runtime_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-cli-runtime] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

static void zr_cli_runtime_execute_body(SZrState *state, TZrPtr arguments) {
    ZrCliExecuteRequest *request = (ZrCliExecuteRequest *) arguments;
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
    request->callCompleted = (TZrBool) (state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static TZrBool zr_cli_runtime_capture_failure(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus effectiveStatus;

    if (state == ZR_NULL) {
        return ZR_FALSE;
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
    return ZR_FALSE;
}

static TZrBool zr_cli_runtime_handle_failure(SZrState *state, EZrThreadStatus status) {
    const TZrChar *aotError;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    aotError = state->global != ZR_NULL ? ZrLibrary_AotRuntime_GetLastError(state->global) : ZR_NULL;
    if (aotError != ZR_NULL && aotError[0] != '\0') {
        ZrCore_Log_Error(state, "%s\n", aotError);
    }

    if (state->hasCurrentException) {
        ZrCore_Exception_LogUnhandled(state, &state->currentException);
        ZrCore_State_ResetThread(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    ZrCore_Log_Error(state, "project execution failed with status %d\n", (int)status);
    ZrCore_State_ResetThread(state, status);
    return ZR_FALSE;
}

static const TZrChar *zr_cli_runtime_mode_name(EZrCliExecutionMode executionMode) {
    switch (executionMode) {
        case ZR_CLI_EXECUTION_MODE_BINARY:
            return "binary";
        case ZR_CLI_EXECUTION_MODE_AOT_C:
            return "aot_c";
        case ZR_CLI_EXECUTION_MODE_AOT_LLVM:
            return "aot_llvm";
        case ZR_CLI_EXECUTION_MODE_INTERP:
        default:
            return "interp";
    }
}

static void zr_cli_runtime_emit_executed_via(SZrState *state,
                                             const SZrCliCommand *command,
                                             const TZrChar *executedVia) {
    if (command != ZR_NULL && command->emitExecutedVia && executedVia != ZR_NULL) {
        ZrCore_Log_Metaf(state, "executed_via=%s\n", executedVia);
    }
}

static TZrBool zr_cli_runtime_resolve_effective_entry_module(const SZrCliCommand *command,
                                                             const SZrCliProjectContext *project,
                                                             TZrChar *normalizedBuffer,
                                                             TZrSize normalizedBufferSize,
                                                             const TZrChar **outEffectiveEntryModule,
                                                             const TZrChar **outEntryIdentifier) {
    TZrChar moduleBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize index;

    if (command == ZR_NULL || project == ZR_NULL || normalizedBuffer == ZR_NULL || normalizedBufferSize == 0 ||
        outEffectiveEntryModule == ZR_NULL || outEntryIdentifier == ZR_NULL) {
        return ZR_FALSE;
    }

    if (command->mode != ZR_CLI_MODE_RUN_PROJECT_MODULE) {
        *outEffectiveEntryModule = project->entryModule;
        *outEntryIdentifier = command->projectPath;
        return ZR_TRUE;
    }

    if (command->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; command->moduleName[index] != '\0' && index + 1 < sizeof(moduleBuffer); index++) {
        moduleBuffer[index] = command->moduleName[index] == '.' ? '/' : command->moduleName[index];
    }
    moduleBuffer[index] = '\0';
    if (command->moduleName[index] != '\0') {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_NormalizeModuleName(moduleBuffer, normalizedBuffer, normalizedBufferSize)) {
        return ZR_FALSE;
    }

    *outEffectiveEntryModule = normalizedBuffer;
    *outEntryIdentifier = command->moduleName;
    return ZR_TRUE;
}

TZrBool ZrCli_Runtime_InjectProcessArguments(SZrState *state,
                                             const TZrChar *entryIdentifier,
                                             const TZrChar *const *programArgs,
                                             TZrSize programArgCount) {
    static const TZrChar processModuleName[] = "zr.system.process";
    static const TZrChar argumentsExportName[] = "arguments";
    SZrString *processModuleString;
    SZrObjectModule *processModule;
    SZrString *argumentsName;
    SZrObject *argumentsArray;
    SZrTypeValue argumentsValue;
    TZrBool ignoredArray = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || entryIdentifier == ZR_NULL) {
        return ZR_FALSE;
    }

    processModuleString = ZrCore_String_CreateFromNative(state, processModuleName);
    if (processModuleString == ZR_NULL) {
        return ZR_FALSE;
    }

    processModule = ZrCore_Module_ImportByPath(state, processModuleString);
    if (processModule == ZR_NULL) {
        return ZR_FALSE;
    }

    argumentsArray = ZrLib_Array_New(state);
    if (argumentsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    ignoredArray = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray));
    ZrLib_Value_SetString(state, &argumentsValue, entryIdentifier);
    if (!ZrLib_Array_PushValue(state, argumentsArray, &argumentsValue)) {
        if (ignoredArray) {
            ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray));
        }
        return ZR_FALSE;
    }

    for (index = 0; index < programArgCount; index++) {
        if (programArgs == ZR_NULL || programArgs[index] == ZR_NULL) {
            continue;
        }

        ZrLib_Value_SetString(state, &argumentsValue, programArgs[index]);
        if (!ZrLib_Array_PushValue(state, argumentsArray, &argumentsValue)) {
            if (ignoredArray) {
                ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray));
            }
            return ZR_FALSE;
        }
    }

    argumentsName = ZrCore_String_CreateFromNative(state, argumentsExportName);
    if (argumentsName == ZR_NULL) {
        if (ignoredArray) {
            ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray));
        }
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &argumentsValue, argumentsArray, ZR_VALUE_TYPE_ARRAY);
    ZrCore_Module_AddPubExport(state, processModule, argumentsName, &argumentsValue);
    if (ignoredArray) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray));
    }
    return ZR_TRUE;
}

#if defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
static void zr_cli_runtime_emit_debug_endpoint(SZrState *state,
                                               const SZrCliCommand *command,
                                               ZrDebugAgent *agent) {
    TZrChar endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];

    if (command == ZR_NULL || !command->debugPrintEndpoint || agent == ZR_NULL) {
        return;
    }

    if (ZrDebug_AgentGetEndpoint(agent, endpoint, sizeof(endpoint))) {
        ZrCore_Log_Metaf(state, "debug_endpoint=%s\n", endpoint);
    }
}
#endif

static TZrBool zr_cli_runtime_execute_function(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrCliExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    status = ZrCore_Exception_TryRun(state, zr_cli_runtime_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        return zr_cli_runtime_capture_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        return zr_cli_runtime_capture_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    return ZR_TRUE;
}

static TZrBool zr_cli_runtime_print_result(SZrState *state, SZrTypeValue *result) {
    SZrString *resultString;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    resultString = ZrCore_Value_ConvertToString(state, result);
    if (resultString == ZR_NULL) {
        ZrCore_Log_Error(state, "failed to stringify project result\n");
        return ZR_FALSE;
    }

    ZrCore_Log_Resultf(state, "%s\n", ZrCore_String_GetNativeString(resultString));
    return ZR_TRUE;
}

static void zr_cli_runtime_prepare_entry_module(SZrState *state,
                                                SZrFunction *entryFunction,
                                                const TZrChar *loadedEntryPath) {
    SZrObjectModule *projectModule;
    SZrString *modulePath;
    TZrUInt64 pathHash;

    if (state == ZR_NULL || entryFunction == ZR_NULL || loadedEntryPath == ZR_NULL || loadedEntryPath[0] == '\0') {
        return;
    }

    projectModule = ZrCore_Module_Create(state);
    if (projectModule == ZR_NULL) {
        return;
    }

    modulePath = ZrCore_String_CreateFromNative(state, (TZrNativeString)loadedEntryPath);
    pathHash = ZrCore_Module_CalculatePathHash(state, modulePath);
    ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, modulePath);
    ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, entryFunction);
}

static TZrBool zr_cli_runtime_prepare_and_execute_entry(SZrState *state,
                                                        SZrFunction *entryFunction,
                                                        const TZrChar *loadedEntryPath,
                                                        SZrTypeValue *result) {
    TZrBool ignoredFunction = ZR_FALSE;
    TZrBool success;

    if (state == ZR_NULL || entryFunction == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
    zr_cli_runtime_prepare_entry_module(state, entryFunction, loadedEntryPath);
    success = zr_cli_runtime_execute_function(state, entryFunction, result);
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
    }
    return success;
}

static TZrBool zr_cli_runtime_load_entry_function(SZrState *state,
                                                  const SZrCliProjectContext *project,
                                                  const TZrChar *entryModule,
                                                  TZrBool preferBinary,
                                                  SZrFunction **outFunction,
                                                  TZrChar *loadedPathBuffer,
                                                  TZrSize loadedPathBufferSize) {
    TZrChar entryBinaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entrySourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || project == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZR_NULL;
    ZrCli_Project_ResolveBinaryPath(project, entryModule, entryBinaryPath, sizeof(entryBinaryPath));
    ZrCli_Project_ResolveSourcePath(project, entryModule, entrySourcePath, sizeof(entrySourcePath));

    if (preferBinary && ZrLibrary_File_Exist(entryBinaryPath) == ZR_LIBRARY_FILE_IS_FILE) {
        SZrIo io;
        SZrIoSource *ioSource;

        if (!ZrCli_Project_OpenFileIo(state, entryBinaryPath, ZR_TRUE, &io)) {
            return ZR_FALSE;
        }

        ioSource = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            return ZR_FALSE;
        }

        *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
        ZrCore_Io_ReadSourceFree(state->global, ioSource);
        if (*outFunction == ZR_NULL) {
            return ZR_FALSE;
        }

        if (loadedPathBuffer != ZR_NULL && loadedPathBufferSize > 0) {
            snprintf(loadedPathBuffer, loadedPathBufferSize, "%s", entryBinaryPath);
            return ZR_TRUE;
        }

        return ZR_TRUE;
    }

    if (ZrLibrary_File_Exist(entrySourcePath) == ZR_LIBRARY_FILE_IS_FILE) {
        TZrChar *source = ZR_NULL;
        TZrSize sourceLength = 0;
        SZrString *sourceName;

        if (!ZrCli_Project_ReadTextFile(entrySourcePath, &source, &sourceLength)) {
            return ZR_FALSE;
        }

        sourceName = ZrCore_String_CreateFromNative(state, entrySourcePath);
        *outFunction = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
        free(source);
        if (*outFunction != ZR_NULL && loadedPathBuffer != ZR_NULL && loadedPathBufferSize > 0) {
            snprintf(loadedPathBuffer, loadedPathBufferSize, "%s", entrySourcePath);
        }
        return *outFunction != ZR_NULL;
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_runtime_source_first_loader(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    SZrCliProjectContext project;
    SZrLibrary_Project *libraryProject;
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->userData == ZR_NULL || path == ZR_NULL ||
        io == ZR_NULL) {
        return ZR_FALSE;
    }

    libraryProject = (SZrLibrary_Project *) state->global->userData;
    if (!ZrCli_ProjectContext_FromGlobal(&project,
                                         state->global,
                                         ZrCore_String_GetNativeString(libraryProject->file))) {
        return ZR_FALSE;
    }

    if (ZrCli_Project_ResolveSourcePath(&project, path, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return ZrCli_Project_OpenFileIo(state, resolvedPath, ZR_FALSE, io);
    }

    if (ZrCli_Project_ResolveBinaryPath(&project, path, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return ZrCli_Project_OpenFileIo(state, resolvedPath, ZR_TRUE, io);
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_runtime_binary_first_loader(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    SZrCliProjectContext project;
    SZrLibrary_Project *libraryProject;
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->userData == ZR_NULL || path == ZR_NULL ||
        io == ZR_NULL) {
        return ZR_FALSE;
    }

    libraryProject = (SZrLibrary_Project *) state->global->userData;
    if (!ZrCli_ProjectContext_FromGlobal(&project,
                                         state->global,
                                         ZrCore_String_GetNativeString(libraryProject->file))) {
        return ZR_FALSE;
    }

    if (ZrCli_Project_ResolveBinaryPath(&project, path, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return ZrCli_Project_OpenFileIo(state, resolvedPath, ZR_TRUE, io);
    }

    if (ZrCli_Project_ResolveSourcePath(&project, path, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return ZrCli_Project_OpenFileIo(state, resolvedPath, ZR_FALSE, io);
    }

    return ZR_FALSE;
}

static int zr_cli_runtime_run_project(const SZrCliCommand *command) {
    SZrGlobalState *global;
    SZrCliProjectContext project;
    SZrState *state;
    TZrChar normalizedEntryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *effectiveEntryModule = ZR_NULL;
    const TZrChar *entryIdentifier = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    SZrTypeValue result;
    TZrChar loadedEntryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool success = ZR_FALSE;
    const TZrChar *executedVia = ZR_NULL;

    if (command == ZR_NULL || command->projectPath == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "run mode requires a project path\n");
        return 1;
    }

#if ZR_VM_BUILD_AOT == 0
    if (command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_C ||
        command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_LLVM) {
        ZrCore_Log_Error(ZR_NULL,
                         "AOT execution modes require a zr_vm build with -DZR_VM_BUILD_AOT=ON\n");
        return 1;
    }
#endif

    zr_cli_runtime_trace("create project global '%s'", command->projectPath);
    global = ZrCli_Project_CreateProjectGlobal(command->projectPath);
    zr_cli_runtime_trace("global=%p", (void *)global);
    if (global == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to load project: %s\n", command->projectPath);
        return 1;
    }

    zr_cli_runtime_trace("register standard modules");
    if (!ZrCli_Project_RegisterStandardModules(global)) {
        ZrCore_Log_Error(global->mainThreadState, "failed to register standard modules\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    state = global->mainThreadState;
    zr_cli_runtime_trace("state=%p", (void *)state);
    if (!ZrCli_ProjectContext_FromGlobal(&project, global, command->projectPath)) {
        ZrCore_Log_Error(state, "failed to resolve project context: %s\n", command->projectPath);
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
    zr_cli_runtime_trace("project context entry='%s'", project.entryModule);

    if (!zr_cli_runtime_resolve_effective_entry_module(command,
                                                       &project,
                                                       normalizedEntryModule,
                                                       sizeof(normalizedEntryModule),
                                                       &effectiveEntryModule,
                                                       &entryIdentifier)) {
        ZrCore_Log_Error(state, "failed to resolve project entry target\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
    zr_cli_runtime_trace("effective entry='%s' executionMode=%d debug=%d",
                         effectiveEntryModule != ZR_NULL ? effectiveEntryModule : "<null>",
                         (int)command->executionMode,
                         (int)command->debugEnabled);

    if (!ZrLibrary_AotRuntime_ConfigureGlobal(global,
                                              (EZrLibraryProjectExecutionMode)command->executionMode,
                                              command->requireAotPath)) {
        ZrCore_Log_Error(state, "failed to configure AOT runtime\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    if (command->executionMode == ZR_CLI_EXECUTION_MODE_INTERP) {
        global->sourceLoader = zr_cli_runtime_source_first_loader;
    }

    if (!ZrCli_Runtime_InjectProcessArguments(state,
                                              entryIdentifier,
                                              command->programArgs,
                                              command->programArgCount)) {
        ZrCore_Log_Error(state, "failed to initialize zr.system.process.arguments\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
    zr_cli_runtime_trace("process arguments injected");

#if !defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
    if (command->debugEnabled) {
        ZrCore_Log_Error(state, "debug agent support is not built into this CLI\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
#else
    if (command->debugEnabled &&
        (command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_C ||
         command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_LLVM)) {
        ZrCore_Log_Error(state, "zr debugger v1 only supports interp and binary execution modes\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    if (command->debugEnabled) {
        ZrDebugAgentConfig debugConfig;
        ZrDebugAgent *debugAgent = ZR_NULL;
        TZrChar debugError[256];
        TZrBool ignoredFunction = ZR_FALSE;

        if (command->executionMode == ZR_CLI_EXECUTION_MODE_BINARY) {
            global->sourceLoader = zr_cli_runtime_binary_first_loader;
        }

        if (!zr_cli_runtime_load_entry_function(state,
                                                &project,
                                                effectiveEntryModule,
                                                command->executionMode == ZR_CLI_EXECUTION_MODE_BINARY,
                                                &entryFunction,
                                                loadedEntryPath,
                                                sizeof(loadedEntryPath))) {
            ZrCore_Log_Error(state, "failed to load project entry: %s\n", effectiveEntryModule);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }
        zr_cli_runtime_trace("debug entry loaded '%s'", loadedEntryPath);

        ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
        zr_cli_runtime_prepare_entry_module(state, entryFunction, loadedEntryPath);

        memset(&debugConfig, 0, sizeof(debugConfig));
        debugConfig.address = command->debugAddress;
        debugConfig.suspend_on_start = command->debugWait;
        debugConfig.wait_for_client = command->debugWait;
        debugConfig.auth_token = ZR_NULL;
        debugConfig.stop_on_uncaught_exception = ZR_TRUE;

        if (!ZrDebug_AgentStart(state,
                                entryFunction,
                                effectiveEntryModule,
                                &debugConfig,
                                &debugAgent,
                                debugError,
                                sizeof(debugError))) {
            ZrCore_Log_Error(state, "failed to start debug agent: %s\n", debugError);
            if (ignoredFunction) {
                ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
            }
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }

        zr_cli_runtime_emit_debug_endpoint(state, command, debugAgent);
        success = zr_cli_runtime_execute_function(state, entryFunction, &result);
        if (!success) {
            ZrDebug_NotifyException(debugAgent);
        }
        ZrDebug_NotifyTerminated(debugAgent, success);
        ZrDebug_AgentStop(debugAgent);
        if (ignoredFunction) {
            ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
        }
        executedVia = zr_cli_runtime_mode_name(command->executionMode);

        if (!success) {
            zr_cli_runtime_handle_failure(state, state->threadStatus);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }

        goto zr_cli_runtime_finish;
    }
#endif

    switch (command->executionMode) {
        case ZR_CLI_EXECUTION_MODE_INTERP: {
            if (command->mode == ZR_CLI_MODE_RUN_PROJECT_MODULE) {
                if (!zr_cli_runtime_load_entry_function(state,
                                                        &project,
                                                        effectiveEntryModule,
                                                        ZR_FALSE,
                                                        &entryFunction,
                                                        loadedEntryPath,
                                                        sizeof(loadedEntryPath))) {
                    ZrCore_Log_Error(state, "failed to load project entry: %s\n", effectiveEntryModule);
                    ZrLibrary_CommonState_CommonGlobalState_Free(global);
                    return 1;
                }

                zr_cli_runtime_trace("interp module entry loaded '%s'", loadedEntryPath);
                success = zr_cli_runtime_prepare_and_execute_entry(state, entryFunction, loadedEntryPath, &result);
            } else {
                zr_cli_runtime_trace("run project via library");
                EZrThreadStatus status = ZrLibrary_Project_Run(state, &result);
                success = status == ZR_THREAD_STATUS_FINE;
            }
            executedVia = "interp";
        } break;

        case ZR_CLI_EXECUTION_MODE_BINARY:
            global->sourceLoader = zr_cli_runtime_binary_first_loader;
            if (!zr_cli_runtime_load_entry_function(state, &project, effectiveEntryModule, ZR_TRUE, &entryFunction, loadedEntryPath,
                                                    sizeof(loadedEntryPath))) {
                ZrCore_Log_Error(state, "failed to load project entry: %s\n", effectiveEntryModule);
                ZrLibrary_CommonState_CommonGlobalState_Free(global);
                return 1;
            }

            zr_cli_runtime_trace("binary entry loaded '%s'", loadedEntryPath);
            success = zr_cli_runtime_prepare_and_execute_entry(state, entryFunction, loadedEntryPath, &result);
            executedVia = "binary";
            break;

#if ZR_VM_BUILD_AOT
        case ZR_CLI_EXECUTION_MODE_AOT_C:
            success = ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result);
            executedVia = ZrLibrary_AotRuntime_ExecutedViaName(ZrLibrary_AotRuntime_GetExecutedVia(global));
            break;

        case ZR_CLI_EXECUTION_MODE_AOT_LLVM:
            success = ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_LLVM, &result);
            executedVia = ZrLibrary_AotRuntime_ExecutedViaName(ZrLibrary_AotRuntime_GetExecutedVia(global));
            break;
#endif

        default:
            ZrCore_Log_Error(state, "unknown execution mode: %d\n", (int)command->executionMode);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
    }

    if (!success) {
        zr_cli_runtime_handle_failure(state, state->threadStatus);
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

zr_cli_runtime_finish:

    if ((command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_C ||
         command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_LLVM) &&
        strcmp(executedVia != ZR_NULL ? executedVia : "none", zr_cli_runtime_mode_name(command->executionMode)) != 0) {
        ZrCore_Log_Error(state,
                         "requested %s but runtime reported %s\n",
                         zr_cli_runtime_mode_name(command->executionMode),
                         executedVia != ZR_NULL ? executedVia : "none");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    if (!zr_cli_runtime_print_result(state, &result)) {
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
    zr_cli_runtime_emit_executed_via(state,
                                     command,
                                     executedVia != ZR_NULL ? executedVia : zr_cli_runtime_mode_name(command->executionMode));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return 0;
}

int ZrCli_Runtime_RunInline(const SZrCliCommand *command) {
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    const TZrChar *sourceLabel;
    TZrBool success;

    if (command == ZR_NULL || command->inlineCode == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "inline mode requires source code\n");
        return 1;
    }

    global = ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to initialize inline runtime\n");
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        ZrCore_Log_Error(global->mainThreadState, "failed to register standard modules\n");
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    state = global->mainThreadState;
    sourceLabel = command->inlineModeAlias != ZR_NULL ? command->inlineModeAlias : "<inline>";
    if (!ZrCli_Runtime_InjectProcessArguments(state,
                                              sourceLabel,
                                              command->programArgs,
                                              command->programArgCount)) {
        ZrCore_Log_Error(state, "failed to initialize zr.system.process.arguments\n");
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    sourceName = ZrCore_String_CreateFromNative(state, sourceLabel);
    entryFunction = ZrParser_Source_Compile(state, command->inlineCode, strlen(command->inlineCode), sourceName);
    if (entryFunction == ZR_NULL) {
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    success = zr_cli_runtime_prepare_and_execute_entry(state, entryFunction, sourceLabel, &result);
    if (!success) {
        zr_cli_runtime_handle_failure(state, state->threadStatus);
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    if (!zr_cli_runtime_print_result(state, &result)) {
        ZrLibrary_NativeRegistry_Free(global);
        ZrCore_GlobalState_Free(global);
        return 1;
    }

    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
    return 0;
}

int ZrCli_Runtime_RunProject(const SZrCliCommand *command) {
    return zr_cli_runtime_run_project(command);
}
