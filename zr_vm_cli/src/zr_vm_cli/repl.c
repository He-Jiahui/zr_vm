#include "zr_vm_cli/repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_cli/conf.h"
#include "zr_vm_cli/project.h"
#include "zr_vm_cli/runtime.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/compiler.h"

static void zr_cli_repl_prepare_stdio(void) {
    static TZrBool prepared = ZR_FALSE;

    if (prepared) {
        return;
    }

    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0);
    (void)setvbuf(stderr, ZR_NULL, _IONBF, 0);
    prepared = ZR_TRUE;
}

static void zr_cli_repl_write_help(void) {
    ZrCore_Log_Helpf(ZR_NULL,
                     "Available commands:\n"
                     "  :help   Show this help text.\n"
                     "  :reset  Clear the pending input buffer.\n"
                     "  :quit   Exit the REPL.\n");
}

typedef struct ZrCliReplExecuteRequest {
    TZrStackValuePointer callBase;
    TZrStackValuePointer resultBase;
} ZrCliReplExecuteRequest;

static void zr_cli_repl_execute_body(SZrState *state, TZrPtr arguments) {
    ZrCliReplExecuteRequest *request = (ZrCliReplExecuteRequest *) arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->callBase == ZR_NULL) {
        return;
    }

    request->resultBase = ZrCore_Function_CallAndRestore(state, request->callBase, 1);
}

static TZrBool zr_cli_repl_handle_failure(SZrState *state, EZrThreadStatus status) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!state->hasCurrentException &&
        !ZrCore_Exception_NormalizeStatus(state,
                                          state->threadStatus != ZR_THREAD_STATUS_FINE ? state->threadStatus : status)) {
        ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
        ZrCore_State_ResetThread(state, status);
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Exception_LogUnhandled(state, &state->currentException);
        ZrCore_State_ResetThread(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
    ZrCore_State_ResetThread(state, status);
    return ZR_FALSE;
}

static TZrBool zr_cli_repl_execute(SZrState *state, TZrStackValuePointer callBase, SZrTypeValue *result) {
    ZrCliReplExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || callBase == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&request, 0, sizeof(request));
    request.callBase = callBase;
    ZrCore_Value_ResetAsNull(result);
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    status = ZrCore_Exception_TryRun(state, zr_cli_repl_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        return zr_cli_repl_handle_failure(state, status);
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE || request.resultBase == ZR_NULL) {
        return state->threadStatus == ZR_THREAD_STATUS_FINE ? ZR_FALSE :
               zr_cli_repl_handle_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    return ZR_TRUE;
}

static void zr_cli_repl_free_global(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return;
    }

    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_GlobalState_Free(global);
}

static int zr_cli_repl_submit(const TZrChar *code) {
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer callBase;
    SZrTypeValue *closureValue;
    SZrTypeValue result;
    SZrString *resultString;
    TZrBool ignoredFunction = ZR_FALSE;
    TZrBool ignoredClosure = ZR_FALSE;
    int exitCode = 1;

    if (code == ZR_NULL || code[0] == '\0') {
        return 0;
    }

    global = ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to initialize REPL runtime\n");
        return 1;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        ZrCore_Log_Error(global->mainThreadState, "failed to register REPL standard modules\n");
        zr_cli_repl_free_global(global);
        return 1;
    }

    state = global->mainThreadState;
    if (!ZrCli_Runtime_InjectProcessArguments(state, "<repl>", ZR_NULL, 0)) {
        ZrCore_Log_Error(state, "failed to initialize REPL process arguments\n");
        zr_cli_repl_free_global(global);
        return 1;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "<repl>");
    function = ZrParser_Source_Compile(state, code, strlen(code), sourceName);
    if (function == ZR_NULL) {
        zr_cli_repl_free_global(global);
        return 1;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        zr_cli_repl_free_global(global);
        return 1;
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(state, function->stackSize + 1, callBase);
    closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer = callBase + 1;
    if (!zr_cli_repl_execute(state, callBase, &result)) {
        goto zr_cli_repl_cleanup;
    }

    resultString = ZrCore_Value_ConvertToString(state, &result);
    if (resultString != ZR_NULL) {
        ZrCore_Log_Resultf(state, "%s\n", ZrCore_String_GetNativeString(resultString));
    } else {
        ZrCore_Log_Error(state, "failed to stringify REPL result\n");
    }

    exitCode = 0;

zr_cli_repl_cleanup:
    if (closure != ZR_NULL) {
        closure->function = ZR_NULL;
    }
    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    zr_cli_repl_free_global(global);
    return exitCode;
}

int ZrCli_Repl_Run(void) {
    TZrChar line[ZR_CLI_REPL_LINE_BUFFER_LENGTH];
    TZrChar *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    TZrSize bufferCapacity = 0;

    zr_cli_repl_prepare_stdio();
    ZrCore_Log_Helpf(ZR_NULL,
                     "ZR VM REPL\n"
                     "Enter code, then submit with an empty line. Type :help for commands.\n");
    ZrCore_Log_FlushDefaultSinks();

    for (;;) {
        TZrSize lineLength;

        ZrCore_Log_FlushDefaultSinks();
        if (fgets(line, sizeof(line), stdin) == ZR_NULL) {
            break;
        }

        lineLength = strlen(line);
        while (lineLength > 0 && (line[lineLength - 1] == '\n' || line[lineLength - 1] == '\r')) {
            line[--lineLength] = '\0';
        }

        if (bufferLength == 0 && line[0] == ':') {
            if (strcmp(line, ":help") == 0) {
                zr_cli_repl_write_help();
            } else if (strcmp(line, ":quit") == 0) {
                ZrCore_Log_FlushDefaultSinks();
                free(buffer);
                return 0;
            } else if (strcmp(line, ":reset") == 0) {
                bufferLength = 0;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
            } else {
                ZrCore_Log_Error(ZR_NULL, "unknown REPL command: %s\n", line);
            }
            ZrCore_Log_FlushDefaultSinks();
            continue;
        }

        if (lineLength == 0) {
            if (bufferLength > 0) {
                (void) zr_cli_repl_submit(buffer);
                bufferLength = 0;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
                ZrCore_Log_FlushDefaultSinks();
            }
            continue;
        }

        if (bufferLength + lineLength + 2 > bufferCapacity) {
            TZrSize newCapacity = bufferCapacity == 0 ? ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY
                                                      : bufferCapacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
            TZrChar *newBuffer;

            while (newCapacity < bufferLength + lineLength + 2) {
                newCapacity *= 2;
            }

            newBuffer = (TZrChar *) realloc(buffer, newCapacity);
            if (newBuffer == ZR_NULL) {
                free(buffer);
                ZrCore_Log_Error(ZR_NULL, "out of memory\n");
                return 1;
            }

            buffer = newBuffer;
            bufferCapacity = newCapacity;
        }

        memcpy(buffer + bufferLength, line, lineLength);
        bufferLength += lineLength;
        buffer[bufferLength++] = '\n';
        buffer[bufferLength] = '\0';
    }

    ZrCore_Log_FlushDefaultSinks();
    free(buffer);
    return 0;
}
