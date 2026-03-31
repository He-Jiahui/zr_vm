#include "zr_vm_cli/runtime.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_cli/project.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"

typedef struct ZrCliExecuteRequest {
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrCliExecuteRequest;

static void zr_cli_runtime_execute_body(SZrState *state, TZrPtr arguments) {
    ZrCliExecuteRequest *request = (ZrCliExecuteRequest *) arguments;
    SZrClosure *closure;
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

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    request->callCompleted = (TZrBool) (state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static TZrBool zr_cli_runtime_handle_failure(SZrState *state, EZrThreadStatus status) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
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

static TZrBool zr_cli_runtime_execute_function(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrCliExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    status = ZrCore_Exception_TryRun(state, zr_cli_runtime_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return zr_cli_runtime_handle_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        return state->threadStatus == ZR_THREAD_STATUS_FINE ? ZR_FALSE :
               zr_cli_runtime_handle_failure(state, state->threadStatus);
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

static int zr_cli_runtime_run_project(const TZrChar *projectPath, TZrBool binaryFirst) {
    SZrGlobalState *global;
    SZrCliProjectContext project;
    SZrState *state;
    SZrFunction *entryFunction = ZR_NULL;
    SZrTypeValue result;
    TZrChar loadedEntryPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    global = ZrCli_Project_CreateProjectGlobal(projectPath);
    if (global == ZR_NULL) {
        fprintf(stderr, "failed to load project: %s\n", projectPath);
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        fprintf(stderr, "failed to register standard modules\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    state = global->mainThreadState;
    if (!binaryFirst) {
        EZrThreadStatus status = ZrLibrary_Project_Run(state, &result);
        if (status != ZR_THREAD_STATUS_FINE || !zr_cli_runtime_print_result(state, &result)) {
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return 1;
        }
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 0;
    }

    if (!ZrCli_ProjectContext_FromGlobal(&project, global, projectPath)) {
        fprintf(stderr, "failed to resolve project context: %s\n", projectPath);
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

    if (entryFunction != ZR_NULL) {
        SZrObjectModule *projectModule = ZrCore_Module_Create(state);
        if (projectModule != ZR_NULL) {
            SZrString *modulePath = ZrCore_String_CreateFromNative(state, loadedEntryPath);
            TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, modulePath);
            ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, modulePath);
            ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, entryFunction);
        }
    }

    if (!zr_cli_runtime_execute_function(state, entryFunction, &result) || !zr_cli_runtime_print_result(state, &result)) {
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return 1;
    }

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    return 0;
}

int ZrCli_Runtime_RunProjectSourceFirst(const TZrChar *projectPath) {
    return zr_cli_runtime_run_project(projectPath, ZR_FALSE);
}

int ZrCli_Runtime_RunProjectBinaryFirst(const TZrChar *projectPath) {
    return zr_cli_runtime_run_project(projectPath, ZR_TRUE);
}
