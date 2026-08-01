#include "zr_vm_lib_testing/module.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#define ZR_TESTING_THREAD_LOCAL __declspec(thread)
#else
#define ZR_TESTING_THREAD_LOCAL _Thread_local
#endif

static ZR_TESTING_THREAD_LOCAL SZrTestingAssertionFailure g_last_failure;
static ZR_TESTING_THREAD_LOCAL TZrBool g_has_last_failure = ZR_FALSE;

typedef struct SZrTestingCallRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} SZrTestingCallRequest;

typedef struct SZrTestingFormatRequest {
    SZrTypeValue *value;
    SZrString *text;
} SZrTestingFormatRequest;

static EZrThreadStatus testing_try_run_preserving_frame(
        SZrState *state,
        FZrTryFunction function,
        TZrPtr arguments) {
    SZrCallInfo *savedCallInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    TZrBool hasSavedCallInfoBase = ZR_FALSE;
    TZrBool hasSavedCallInfoTop = ZR_FALSE;
    TZrBool hasSavedCallInfoReturn = ZR_FALSE;
    TZrUInt32 savedExceptionHandlerStackLength;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    savedExceptionHandlerStackLength = state->exceptionHandlerStackLength;
    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionBase.valuePointer, &savedCallInfoBaseAnchor);
        hasSavedCallInfoBase = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionTop.valuePointer, &savedCallInfoTopAnchor);
        hasSavedCallInfoTop = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->hasReturnDestination &&
        savedCallInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->returnDestination, &savedCallInfoReturnAnchor);
        hasSavedCallInfoReturn = ZR_TRUE;
    }

    status = ZrCore_Exception_TryRun(state, function, arguments);
    state->stackTop.valuePointer =
            ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    state->callInfoList = savedCallInfo;
    if (savedCallInfo != ZR_NULL) {
        if (hasSavedCallInfoBase) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(
                    state, &savedCallInfoReturnAnchor);
        }
    }
    state->exceptionHandlerStackLength = savedExceptionHandlerStackLength;
    return status;
}

static void testing_copy_bounded(TZrChar *destination,
                                 TZrSize capacity,
                                 const TZrChar *source,
                                 TZrBool *truncated) {
    TZrSize length = source != ZR_NULL ? strlen(source) : 0U;
    TZrSize copyLength = length < capacity - 1U ? length : capacity - 1U;

    if (capacity == 0U) {
        return;
    }
    if (copyLength > 0U) {
        memcpy(destination, source, copyLength);
    }
    destination[copyLength] = '\0';
    if (truncated != ZR_NULL) {
        *truncated = length > copyLength ? ZR_TRUE : ZR_FALSE;
    }
}

static void testing_format_try(SZrState *state, TZrPtr arguments) {
    SZrTestingFormatRequest *request = (SZrTestingFormatRequest *)arguments;
    request->text = ZrCore_Value_ToDebugString(state, request->value);
}

static void testing_snapshot(SZrState *state,
                             const SZrTypeValue *value,
                             SZrTestingValueSnapshot *snapshot) {
    SZrTestingFormatRequest request;
    EZrThreadStatus status;
    const TZrChar *text;

    memset(snapshot, 0, sizeof(*snapshot));
    if (state == ZR_NULL || value == ZR_NULL) {
        testing_copy_bounded(snapshot->text, sizeof(snapshot->text), "<null>", ZR_NULL);
        return;
    }

    request.value = (SZrTypeValue *)value;
    request.text = ZR_NULL;
    status = testing_try_run_preserving_frame(state, testing_format_try, &request);
    if (status != ZR_THREAD_STATUS_FINE || request.text == ZR_NULL) {
        snapshot->formatterFaulted = ZR_TRUE;
        testing_copy_bounded(snapshot->text, sizeof(snapshot->text), "<format-error>", ZR_NULL);
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        return;
    }
    text = ZrCore_String_GetNativeString(request.text);
    testing_copy_bounded(snapshot->text, sizeof(snapshot->text), text, &snapshot->truncated);
}

static TZrUInt32 testing_source_line(const ZrLibCallContext *context) {
    SZrCallInfo *callInfo;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return 0U;
    }
    callInfo = context->state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->metadataFunction == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->metadataFunction->instructionsList == ZR_NULL) {
        return 0U;
    }
    return ZrCore_Exception_FindSourceLine(
            callInfo->metadataFunction,
            (TZrMemoryOffset)(callInfo->context.context.programCounter -
                              callInfo->metadataFunction->instructionsList));
}

static void testing_set_object_int(SZrState *state,
                                   SZrObject *object,
                                   const TZrChar *field,
                                   TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

static void testing_set_object_string(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *field,
                                      const TZrChar *value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetString(state, &fieldValue, value != ZR_NULL ? value : "");
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

static ZR_NO_RETURN void testing_raise_failure(ZrLibCallContext *context) {
    SZrState *state = context != ZR_NULL ? context->state : ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrTypeValue payload;

    if (state != ZR_NULL) {
        prototype = ZrLib_Type_FindPrototype(state, "AssertionFailure");
        object = prototype != ZR_NULL
                 ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                 : ZR_NULL;
        if (object != ZR_NULL) {
            testing_set_object_int(state, object, "assertionKind", g_last_failure.assertionKind);
            testing_set_object_int(state, object, "sourceLine", g_last_failure.sourceLine);
            testing_set_object_string(state, object, "message", g_last_failure.message);
            testing_set_object_string(state, object, "expected", g_last_failure.expected.text);
            testing_set_object_string(state, object, "actual", g_last_failure.actual.text);
            testing_set_object_string(state, object, "exception", g_last_failure.exception.text);
            ZrLib_Value_SetObject(state, &payload, object, ZR_VALUE_TYPE_OBJECT);
            if (ZrCore_Exception_NormalizeThrownValue(
                        state, &payload, state->callInfoList, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
            }
        }
        ZrCore_Debug_RunError(
                state,
                "AssertionFailure: %s",
                g_last_failure.message[0] != '\0' ? g_last_failure.message : "assertion failed");
    }
    ZR_ABORT();
}

static void testing_begin_failure(ZrLibCallContext *context,
                                  EZrTestingAssertionKind kind,
                                  const TZrChar *message) {
    memset(&g_last_failure, 0, sizeof(g_last_failure));
    g_has_last_failure = ZR_TRUE;
    g_last_failure.assertionKind = kind;
    g_last_failure.sourceLine = testing_source_line(context);
    testing_copy_bounded(
            g_last_failure.message,
            sizeof(g_last_failure.message),
            message != ZR_NULL ? message : "",
            ZR_NULL);
}

void ZrVmLibTesting_ClearLastFailure(void) {
    memset(&g_last_failure, 0, sizeof(g_last_failure));
    g_has_last_failure = ZR_FALSE;
}

TZrBool ZrVmLibTesting_GetLastFailure(SZrTestingAssertionFailure *outFailure) {
    if (!g_has_last_failure || outFailure == ZR_NULL) {
        return ZR_FALSE;
    }
    *outFailure = g_last_failure;
    return ZR_TRUE;
}

TZrBool ZrVmLibTesting_Assert(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrBool condition;
    SZrString *message = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 1U, 2U) ||
        !ZrLib_CallContext_ReadBool(context, 0U, &condition)) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 1U &&
        !ZrLib_CallContext_ReadString(context, 1U, &message)) {
        return ZR_FALSE;
    }
    if (!condition) {
        testing_begin_failure(
                context,
                ZR_TESTING_ASSERTION_KIND_ASSERT,
                message != ZR_NULL ? ZrCore_String_GetNativeString(message) : "");
        testing_raise_failure(context);
    }
    ZrVmLibTesting_ClearLastFailure();
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrVmLibTesting_Equal(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *actual;
    SZrTypeValue *expected;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2U, 2U)) {
        return ZR_FALSE;
    }
    actual = ZrLib_CallContext_Argument(context, 0U);
    expected = ZrLib_CallContext_Argument(context, 1U);
    if (actual == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_Value_CompareDirectly(context->state, actual, expected) &&
        !ZrCore_Value_Equal(context->state, actual, expected)) {
        testing_begin_failure(context, ZR_TESTING_ASSERTION_KIND_EQUAL, "values are not equal");
        testing_snapshot(context->state, expected, &g_last_failure.expected);
        testing_snapshot(context->state, actual, &g_last_failure.actual);
        testing_raise_failure(context);
    }
    ZrVmLibTesting_ClearLastFailure();
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static void testing_call_try(SZrState *state, TZrPtr arguments) {
    SZrTestingCallRequest *request = (SZrTestingCallRequest *)arguments;
    request->completed = ZrLib_CallValue(
            state, request->callable, ZR_NULL, ZR_NULL, 0U, &request->result);
}

TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *callable;
    SZrTestingCallRequest request;
    EZrThreadStatus status;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 1U, 1U) ||
        !ZrLib_CallContext_ReadFunction(context, 0U, &callable)) {
        return ZR_FALSE;
    }
    memset(&request, 0, sizeof(request));
    request.callable = callable;
    status = testing_try_run_preserving_frame(
            context->state, testing_call_try, &request);
    if (status == ZR_THREAD_STATUS_FINE && request.completed) {
        testing_begin_failure(
                context, ZR_TESTING_ASSERTION_KIND_THROWS, "action completed without throwing");
        testing_raise_failure(context);
    }
    if (!context->state->hasCurrentException) {
        if (!ZrCore_Exception_NormalizeStatus(
                    context->state,
                    status != ZR_THREAD_STATUS_FINE
                            ? status
                            : ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            return ZR_FALSE;
        }
    }
    context->state->threadStatus = ZR_THREAD_STATUS_FINE;
    testing_snapshot(
            context->state,
            &context->state->currentException,
            &g_last_failure.exception);
    ZrCore_Value_Copy(context->state, result, &context->state->currentException);
    ZrCore_Exception_ClearCurrent(context->state);
    context->state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrVmLibTesting_ClearLastFailure();
    return ZR_TRUE;
}
