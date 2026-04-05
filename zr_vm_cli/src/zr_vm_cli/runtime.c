#include "zr_vm_cli/runtime.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_cli/project.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"

#if defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
#include "zr_vm_lib_debug/debug.h"
#endif

typedef struct ZrCliExecuteRequest {
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrCliExecuteRequest;

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
        fprintf(stderr, "%s\n", aotError);
    }

    if (state->hasCurrentException) {
        ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        ZrCore_State_ResetThread(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    fprintf(stderr, "project execution failed with status %d\n", (int) status);
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

static void zr_cli_runtime_emit_executed_via(const SZrCliCommand *command, const TZrChar *executedVia) {
    if (command != ZR_NULL && command->emitExecutedVia && executedVia != ZR_NULL) {
        printf("executed_via=%s\n", executedVia);
    }
}

#if defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
static void zr_cli_runtime_emit_debug_endpoint(const SZrCliCommand *command, ZrDebugAgent *agent) {
    TZrChar endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];

    if (command == ZR_NULL || !command->debugPrintEndpoint || agent == ZR_NULL) {
        return;
    }

    if (ZrDebug_AgentGetEndpoint(agent, endpoint, sizeof(endpoint))) {
        printf("debug_endpoint=%s\n", endpoint);
        fflush(stdout);
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
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return zr_cli_runtime_capture_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return zr_cli_runtime_capture_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    if (request.closure != ZR_NULL) {
        request.closure->function = ZR_NULL;
    }
    return ZR_TRUE;
}

static TZrBool zr_cli_runtime_print_result(SZrState *state, SZrTypeValue *result) {
    SZrString *resultString;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    resultString = ZrCore_Value_ConvertToString(state, result);
    if (resultString == ZR_NULL) {
        fprintf(stderr, "failed to stringify project result\n");
        return ZR_FALSE;
    }

    printf("%s\n", ZrCore_String_GetNativeString(resultString));
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
    ZrCli_Project_ResolveBinaryPath(project, project->entryModule, entryBinaryPath, sizeof(entryBinaryPath));
    ZrCli_Project_ResolveSourcePath(project, project->entryModule, entrySourcePath, sizeof(entrySourcePath));

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
    SZrFunction *entryFunction = ZR_NULL;
    SZrTypeValue result;
    TZrChar loadedEntryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool success = ZR_FALSE;
    const TZrChar *executedVia = ZR_NULL;

    if (command == ZR_NULL || command->projectPath == ZR_NULL) {
        fprintf(stderr, "run mode requires a project path\n");
        return 1;
    }

    global = ZrCli_Project_CreateProjectGlobal(command->projectPath);
    if (global == ZR_NULL) {
        fprintf(stderr, "failed to load project: %s\n", command->projectPath);
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        fprintf(stderr, "failed to register standard modules\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    state = global->mainThreadState;
    if (!ZrLibrary_AotRuntime_ConfigureGlobal(global,
                                              (EZrLibraryProjectExecutionMode)command->executionMode,
                                              command->requireAotPath)) {
        fprintf(stderr, "failed to configure AOT runtime\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

#if !defined(ZR_VM_CLI_HAS_DEBUG_AGENT)
    if (command->debugEnabled) {
        fprintf(stderr, "debug agent support is not built into this CLI\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
#else
    if (command->debugEnabled &&
        (command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_C ||
         command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_LLVM)) {
        fprintf(stderr, "zr debugger v1 only supports interp and binary execution modes\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    if (command->debugEnabled) {
        ZrDebugAgentConfig debugConfig;
        ZrDebugAgent *debugAgent = ZR_NULL;
        TZrChar debugError[256];
        TZrBool ignoredFunction = ZR_FALSE;

        if (!ZrCli_ProjectContext_FromGlobal(&project, global, command->projectPath)) {
            fprintf(stderr, "failed to resolve project context: %s\n", command->projectPath);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }

        if (command->executionMode == ZR_CLI_EXECUTION_MODE_BINARY) {
            global->sourceLoader = zr_cli_runtime_binary_first_loader;
        }

        if (!zr_cli_runtime_load_entry_function(state,
                                                &project,
                                                command->executionMode == ZR_CLI_EXECUTION_MODE_BINARY,
                                                &entryFunction,
                                                loadedEntryPath,
                                                sizeof(loadedEntryPath))) {
            fprintf(stderr, "failed to load project entry: %s\n", project.entryModule);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }

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
                                project.entryModule,
                                &debugConfig,
                                &debugAgent,
                                debugError,
                                sizeof(debugError))) {
            fprintf(stderr, "failed to start debug agent: %s\n", debugError);
            if (ignoredFunction) {
                ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
            }
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }

        zr_cli_runtime_emit_debug_endpoint(command, debugAgent);
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
            EZrThreadStatus status = ZrLibrary_Project_Run(state, &result);
            success = status == ZR_THREAD_STATUS_FINE;
            executedVia = "interp";
        } break;

        case ZR_CLI_EXECUTION_MODE_BINARY:
            if (!ZrCli_ProjectContext_FromGlobal(&project, global, command->projectPath)) {
                fprintf(stderr, "failed to resolve project context: %s\n", command->projectPath);
                ZrLibrary_CommonState_CommonGlobalState_Free(global);
                return 1;
            }

            global->sourceLoader = zr_cli_runtime_binary_first_loader;
            if (!zr_cli_runtime_load_entry_function(state, &project, ZR_TRUE, &entryFunction, loadedEntryPath,
                                                    sizeof(loadedEntryPath))) {
                fprintf(stderr, "failed to load project entry: %s\n", project.entryModule);
                ZrLibrary_CommonState_CommonGlobalState_Free(global);
                return 1;
            }

            success = zr_cli_runtime_prepare_and_execute_entry(state, entryFunction, loadedEntryPath, &result);
            executedVia = "binary";
            break;

        case ZR_CLI_EXECUTION_MODE_AOT_C:
            success = ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result);
            executedVia = ZrLibrary_AotRuntime_ExecutedViaName(ZrLibrary_AotRuntime_GetExecutedVia(global));
            break;

        case ZR_CLI_EXECUTION_MODE_AOT_LLVM:
            success = ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_LLVM, &result);
            executedVia = ZrLibrary_AotRuntime_ExecutedViaName(ZrLibrary_AotRuntime_GetExecutedVia(global));
            break;

        default:
            fprintf(stderr, "unknown execution mode: %d\n", (int)command->executionMode);
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
        fprintf(stderr, "requested %s but runtime reported %s\n",
                zr_cli_runtime_mode_name(command->executionMode),
                executedVia != ZR_NULL ? executedVia : "none");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    if (!zr_cli_runtime_print_result(state, &result)) {
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }
    zr_cli_runtime_emit_executed_via(command, executedVia != ZR_NULL ? executedVia : zr_cli_runtime_mode_name(command->executionMode));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return 0;
}

int ZrCli_Runtime_RunProject(const SZrCliCommand *command) {
    return zr_cli_runtime_run_project(command);
}
