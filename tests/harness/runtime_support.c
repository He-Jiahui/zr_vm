#include "runtime_support.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/stack.h"

typedef struct ZrTestsRuntimeExecuteRequest {
    SZrFunction *function;
    SZrClosure *closure;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrTestsRuntimeExecuteRequest;

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

static void zr_tests_runtime_execute_body(SZrState *state, TZrPtr arguments) {
    ZrTestsRuntimeExecuteRequest *request = (ZrTestsRuntimeExecuteRequest *)arguments;
    SZrClosure *closure;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;
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
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &baseAnchor);

    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

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
        global->panicHandlingFunction = panicHandler;
    }

    return mainState;
}

void ZrTests_Runtime_State_Destroy(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    ZrCore_GlobalState_Free(state->global);
}

TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrTestsRuntimeExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    request.closure = ZR_NULL;
    request.resultBase = ZR_NULL;
    request.callCompleted = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, zr_tests_runtime_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        state->threadStatus = status;
        return zr_tests_runtime_handle_unhandled_exception(state, result);
    }
    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        if (request.closure != ZR_NULL) {
            request.closure->function = ZR_NULL;
        }
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return zr_tests_runtime_handle_unhandled_exception(state, result);
        }
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    if (request.closure != ZR_NULL) {
        request.closure->function = ZR_NULL;
    }
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
