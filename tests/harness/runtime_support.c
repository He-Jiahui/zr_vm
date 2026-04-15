#include "runtime_support.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/stack.h"

#if defined(_MSC_VER)
    #define ZR_TESTS_THREAD_LOCAL __declspec(thread)
#else
    #define ZR_TESTS_THREAD_LOCAL _Thread_local
#endif

#define ZR_TESTS_CRASH_SCOPE_STACK_CAPACITY 16u

typedef struct ZrTestsRuntimeExecuteRequest {
    SZrFunction *function;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrTestsRuntimeExecuteRequest;

typedef struct ZrTestsRuntimePanicHandlerRegistration {
    SZrGlobalState *global;
    FZrPanicHandlingFunction userHandler;
    struct ZrTestsRuntimePanicHandlerRegistration *next;
} ZrTestsRuntimePanicHandlerRegistration;

static ZrTestsRuntimePanicHandlerRegistration *g_zr_tests_runtime_panic_handlers = ZR_NULL;
static ZR_TESTS_THREAD_LOCAL SZrState *g_zr_tests_runtime_crash_scope_stack[ZR_TESTS_CRASH_SCOPE_STACK_CAPACITY];
static ZR_TESTS_THREAD_LOCAL TZrUInt32 g_zr_tests_runtime_crash_scope_depth = 0;
static ZR_TESTS_THREAD_LOCAL SZrState *g_zr_tests_runtime_last_panic_state = ZR_NULL;
static FZrTestsRuntimeFatalCrashHook g_zr_tests_runtime_fatal_crash_hook = ZR_NULL;

static FZrPanicHandlingFunction zr_tests_runtime_lookup_user_panic_handler(SZrGlobalState *global) {
    ZrTestsRuntimePanicHandlerRegistration *registration = g_zr_tests_runtime_panic_handlers;

    while (registration != ZR_NULL) {
        if (registration->global == global) {
            return registration->userHandler;
        }
        registration = registration->next;
    }

    return ZR_NULL;
}

static void zr_tests_runtime_register_panic_handler(SZrGlobalState *global, FZrPanicHandlingFunction handler) {
    ZrTestsRuntimePanicHandlerRegistration *registration;

    if (global == ZR_NULL) {
        return;
    }

    registration = (ZrTestsRuntimePanicHandlerRegistration *)malloc(sizeof(*registration));
    if (registration == ZR_NULL) {
        return;
    }

    registration->global = global;
    registration->userHandler = handler;
    registration->next = g_zr_tests_runtime_panic_handlers;
    g_zr_tests_runtime_panic_handlers = registration;
}

static void zr_tests_runtime_unregister_panic_handler(SZrGlobalState *global) {
    ZrTestsRuntimePanicHandlerRegistration *current = g_zr_tests_runtime_panic_handlers;
    ZrTestsRuntimePanicHandlerRegistration *previous = ZR_NULL;

    while (current != ZR_NULL) {
        if (current->global == global) {
            if (previous == ZR_NULL) {
                g_zr_tests_runtime_panic_handlers = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

static void zr_tests_runtime_forget_state_from_crash_scopes(SZrState *state) {
    TZrUInt32 writeIndex = 0;
    TZrUInt32 readIndex;

    if (state == ZR_NULL) {
        return;
    }

    for (readIndex = 0; readIndex < g_zr_tests_runtime_crash_scope_depth; ++readIndex) {
        if (g_zr_tests_runtime_crash_scope_stack[readIndex] == state) {
            continue;
        }
        g_zr_tests_runtime_crash_scope_stack[writeIndex++] = g_zr_tests_runtime_crash_scope_stack[readIndex];
    }

    g_zr_tests_runtime_crash_scope_depth = writeIndex;
    if (g_zr_tests_runtime_last_panic_state == state) {
        g_zr_tests_runtime_last_panic_state = ZR_NULL;
    }
}

static void zr_tests_runtime_panic_handler_dispatch(SZrState *state) {
    FZrPanicHandlingFunction userHandler = ZR_NULL;

    if (state != ZR_NULL) {
        g_zr_tests_runtime_last_panic_state = state;
        if (g_zr_tests_runtime_crash_scope_depth == 0) {
            g_zr_tests_runtime_crash_scope_stack[0] = state;
            g_zr_tests_runtime_crash_scope_depth = 1;
        }
        if (state->global != ZR_NULL) {
            userHandler = zr_tests_runtime_lookup_user_panic_handler(state->global);
        }
    }

    if (userHandler != ZR_NULL) {
        userHandler(state);
    }

    if (g_zr_tests_runtime_fatal_crash_hook != ZR_NULL) {
        g_zr_tests_runtime_fatal_crash_hook(state);
    }
}

void ZrTests_Runtime_SetFatalCrashHook(FZrTestsRuntimeFatalCrashHook hook) {
    g_zr_tests_runtime_fatal_crash_hook = hook;
}

void ZrTests_Runtime_CrashScope_Begin(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    if (g_zr_tests_runtime_crash_scope_depth < ZR_TESTS_CRASH_SCOPE_STACK_CAPACITY) {
        g_zr_tests_runtime_crash_scope_stack[g_zr_tests_runtime_crash_scope_depth++] = state;
    } else {
        g_zr_tests_runtime_crash_scope_stack[ZR_TESTS_CRASH_SCOPE_STACK_CAPACITY - 1u] = state;
    }
}

void ZrTests_Runtime_CrashScope_End(SZrState *state) {
    TZrUInt32 index;

    if (state == ZR_NULL || g_zr_tests_runtime_crash_scope_depth == 0) {
        if (g_zr_tests_runtime_last_panic_state == state) {
            g_zr_tests_runtime_last_panic_state = ZR_NULL;
        }
        return;
    }

    for (index = g_zr_tests_runtime_crash_scope_depth; index > 0; --index) {
        if (g_zr_tests_runtime_crash_scope_stack[index - 1u] == state) {
            TZrUInt32 moveIndex;
            for (moveIndex = index; moveIndex < g_zr_tests_runtime_crash_scope_depth; ++moveIndex) {
                g_zr_tests_runtime_crash_scope_stack[moveIndex - 1u] = g_zr_tests_runtime_crash_scope_stack[moveIndex];
            }
            g_zr_tests_runtime_crash_scope_depth--;
            break;
        }
    }

    if (g_zr_tests_runtime_last_panic_state == state) {
        g_zr_tests_runtime_last_panic_state = ZR_NULL;
    }
}

SZrState *ZrTests_Runtime_CrashState_Current(void) {
    if (g_zr_tests_runtime_crash_scope_depth > 0) {
        return g_zr_tests_runtime_crash_scope_stack[g_zr_tests_runtime_crash_scope_depth - 1u];
    }

    return g_zr_tests_runtime_last_panic_state;
}

void ZrTests_Runtime_ClearCrashState(void) {
    g_zr_tests_runtime_crash_scope_depth = 0;
    g_zr_tests_runtime_last_panic_state = ZR_NULL;
}

TZrBool ZrTests_Runtime_ReportCrashState(FILE *stream, TZrBool *printedExceptionStack) {
    SZrState *state = ZrTests_Runtime_CrashState_Current();
    FILE *output = stream != ZR_NULL ? stream : stderr;

    if (printedExceptionStack != ZR_NULL) {
        *printedExceptionStack = ZR_FALSE;
    }

    if (state == ZR_NULL || output == ZR_NULL) {
        return ZR_FALSE;
    }

    fputs("[zr-tests] active zr vm state detected during fatal crash.\n", output);
    if (state->hasCurrentException) {
        if (printedExceptionStack != ZR_NULL) {
            *printedExceptionStack = ZR_TRUE;
        }
        ZrCore_Exception_PrintUnhandled(state, &state->currentException, output);
    } else {
        fputs("[zr-tests] active zr vm state had no normalized current exception to print.\n", output);
        fflush(output);
    }

    return ZR_TRUE;
}

static TZrBool zr_tests_runtime_try_invoke_unhandled_handler(SZrState *state, TZrBool *handled) {
    SZrGlobalState *global;
    TZrStackValuePointer callBase;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *returnValue;

    if (handled != ZR_NULL) {
        *handled = ZR_FALSE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    if (!global->hasUnhandledExceptionHandler || !state->hasCurrentException) {
        return ZR_FALSE;
    }

    callBase = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndAnchor(state, 3, callBase, callBase, &anchor);
    ZrCore_Value_Copy(state, &callBase->value, &global->unhandledExceptionHandler);
    ZrCore_Value_Copy(state, &(callBase + 1)->value, &state->currentException);
    state->stackTop.valuePointer = callBase + 2;

    callBase = ZrCore_Function_CallWithoutYieldAndRestoreAnchor(state, &anchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || callBase == ZR_NULL) {
        return ZR_TRUE;
    }

    returnValue = ZrCore_Stack_GetValue(callBase);
    if (handled != ZR_NULL && returnValue != ZR_NULL && ZR_VALUE_IS_TYPE_BOOL(returnValue->type)) {
        *handled = (TZrBool)(returnValue->value.nativeObject.nativeBool != 0);
    }
    return ZR_TRUE;
}

static TZrBool zr_tests_runtime_handle_unhandled_exception(SZrState *state, SZrTypeValue *result) {
    TZrBool handlerHandled = ZR_FALSE;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!state->hasCurrentException &&
        !ZrCore_Exception_NormalizeStatus(state, state->threadStatus != ZR_THREAD_STATUS_FINE
                                                     ? state->threadStatus
                                                     : ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_State_ResetThread(state, state->threadStatus);
        return ZR_FALSE;
    }

    zr_tests_runtime_try_invoke_unhandled_handler(state, &handlerHandled);
    if (handlerHandled) {
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
    ZrCore_State_ResetThread(state, state->currentExceptionStatus);
    return ZR_FALSE;
}

static TZrBool zr_tests_runtime_capture_failure(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus effectiveStatus;
    SZrTypeValue preservedException;
    EZrThreadStatus preservedExceptionStatus = ZR_THREAD_STATUS_FINE;
    TZrBool preservedHasException = ZR_FALSE;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&preservedException);
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
    preservedHasException = state->hasCurrentException;
    if (preservedHasException) {
        preservedException = state->currentException;
        preservedExceptionStatus = state->currentExceptionStatus;
    }

    ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    if (preservedHasException) {
        state->currentException = preservedException;
        state->currentExceptionStatus = preservedExceptionStatus;
        state->hasCurrentException = ZR_TRUE;
    }
    state->threadStatus = effectiveStatus;
    return ZR_FALSE;
}

static void zr_tests_runtime_execute_body(SZrState *state, TZrPtr arguments) {
    ZrTestsRuntimeExecuteRequest *request = (ZrTestsRuntimeExecuteRequest *)arguments;
    SZrClosure *closure;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;
    SZrTypeValue *closureValue;
    TZrBool ignoredFunction = ZR_FALSE;
    TZrBool ignoredClosure = ZR_FALSE;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(state,
                                                           ZR_CAST_RAW_OBJECT_AS_SUPER(request->function));
    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        if (ignoredFunction) {
            ZrCore_GarbageCollector_UnignoreObject(state->global,
                                                   ZR_CAST_RAW_OBJECT_AS_SUPER(request->function));
        }
        return;
    }

    ignoredClosure = ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->function = request->function;
    ZrCore_Closure_InitValue(state, closure);

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &baseAnchor);

    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    if (ignoredClosure) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(state->global,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(request->function));
    }

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

TZrPtr ZrTests_Runtime_Allocator_Default(TZrPtr userData,
                                         TZrPtr pointer,
                                         TZrSize originalSize,
                                         TZrSize newSize,
                                         TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

SZrState *ZrTests_Runtime_State_Create(FZrPanicHandlingFunction panicHandler) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(ZrTests_Runtime_Allocator_Default, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    SZrState *mainState = global->mainThreadState;
    if (mainState != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        zr_tests_runtime_register_panic_handler(global, panicHandler);
        global->panicHandlingFunction = zr_tests_runtime_panic_handler_dispatch;
    }

    return mainState;
}

void ZrTests_Runtime_State_Destroy(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    zr_tests_runtime_forget_state_from_crash_scopes(state);
    zr_tests_runtime_unregister_panic_handler(state->global);
    ZrCore_GlobalState_Free(state->global);
}

TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (!ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, result)) {
        return zr_tests_runtime_handle_unhandled_exception(state, result);
    }

    return ZR_TRUE;
}

TZrBool ZrTests_Runtime_Function_ExecuteCaptureFailure(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrTestsRuntimeExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    request.resultBase = ZR_NULL;
    request.callCompleted = ZR_FALSE;
    ZrTests_Runtime_CrashScope_Begin(state);
    status = ZrCore_Exception_TryRun(state, zr_tests_runtime_execute_body, &request);
    ZrTests_Runtime_CrashScope_End(state);
    if (status != ZR_THREAD_STATUS_FINE) {
        return zr_tests_runtime_capture_failure(state, status);
    }
    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return zr_tests_runtime_capture_failure(state, state->threadStatus);
        }
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    return ZR_TRUE;
}

TZrBool ZrTests_Runtime_Function_ExecuteExpectInt64(SZrState *state, SZrFunction *function, TZrInt64 *result) {
    SZrTypeValue returnValue;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Runtime_Function_Execute(state, function, &returnValue)) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_INT(returnValue.type)) {
        return ZR_FALSE;
    }

    *result = returnValue.value.nativeObject.nativeInt64;
    return ZR_TRUE;
}
