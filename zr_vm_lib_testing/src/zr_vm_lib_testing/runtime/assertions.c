#include "zr_vm_lib_testing/module.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/reflection.h"
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
    TZrSize length;
    TZrSize copyLength;

    if (capacity == 0U) {
        return;
    }
    length = source != ZR_NULL ? strlen(source) : 0U;
    copyLength = length < capacity - 1U ? length : capacity - 1U;
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
        testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "null", ZR_NULL);
        testing_copy_bounded(snapshot->text, sizeof(snapshot->text), "<null>", ZR_NULL);
        return;
    }

    snapshot->hasValue = ZR_TRUE;
    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "null", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_BOOL:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "bool", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "int", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "float", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_STRING:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "string", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE_VALUE:
        case ZR_VALUE_TYPE_CLOSURE:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "function", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_ARRAY:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "array", ZR_NULL);
            break;
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = value->value.object != ZR_NULL
                                      ? ZR_CAST_OBJECT(state, value->value.object)
                                      : ZR_NULL;
            const TZrChar *typeName = object != ZR_NULL && object->prototype != ZR_NULL &&
                                              object->prototype->name != ZR_NULL
                                              ? ZrCore_String_GetNativeString(object->prototype->name)
                                              : "object";
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), typeName, ZR_NULL);
            break;
        }
        default:
            testing_copy_bounded(snapshot->typeName, sizeof(snapshot->typeName), "value", ZR_NULL);
            break;
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

static void testing_source_span(const ZrLibCallContext *context,
                                SZrTestingSourceSpan *outSpan) {
    SZrCallInfo *callInfo;

    if (outSpan == ZR_NULL) {
        return;
    }
    memset(outSpan, 0, sizeof(*outSpan));
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    callInfo = context->state->callInfoList;
    while (callInfo != ZR_NULL && !ZR_CALL_INFO_IS_VM(callInfo)) {
        callInfo = callInfo->previous;
    }
    if (callInfo == ZR_NULL || callInfo->metadataFunction == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->metadataFunction->instructionsList == ZR_NULL) {
        return;
    }

    SZrFunction *function = callInfo->metadataFunction;
    TZrMemoryOffset instructionOffset =
            (TZrMemoryOffset)(callInfo->context.context.programCounter - function->instructionsList);
    const SZrFunctionExecutionLocationInfo *best = ZR_NULL;
    for (TZrUInt32 index = 0U; index < function->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *location =
                &function->executionLocationInfoList[index];
        if (location->currentInstructionOffset > instructionOffset) {
            break;
        }
        best = location;
    }
    if (function->sourceCodeList != ZR_NULL) {
        testing_copy_bounded(outSpan->sourceFile,
                             sizeof(outSpan->sourceFile),
                             ZrCore_String_GetNativeString(function->sourceCodeList),
                             ZR_NULL);
    }
    if (best != ZR_NULL) {
        outSpan->startLine = best->lineInSource;
        outSpan->startColumn = best->columnInSourceStart;
        outSpan->endLine = best->lineInSourceEnd;
        outSpan->endColumn = best->columnInSourceEnd;
    }
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

static void testing_set_object_bool(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *field,
                                    TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

static SZrObject *testing_new_record(SZrState *state, const TZrChar *typeName) {
    SZrObjectPrototype *prototype = ZrLib_Type_FindPrototype(state, typeName);
    return prototype != ZR_NULL
                   ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                   : ZR_NULL;
}

static void testing_set_object_record(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *field,
                                      SZrObject *record) {
    SZrTypeValue fieldValue;
    if (record == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetObject(state, &fieldValue, record, ZR_VALUE_TYPE_OBJECT);
    }
    ZrLib_Object_SetFieldCString(state, object, field, &fieldValue);
}

static SZrObject *testing_build_source_span_object(SZrState *state,
                                                   const SZrTestingSourceSpan *span) {
    SZrObject *object = testing_new_record(state, "SourceSpan");
    if (object == ZR_NULL || span == ZR_NULL) {
        return object;
    }
    testing_set_object_string(state, object, "sourceFile", span->sourceFile);
    testing_set_object_int(state, object, "startLine", span->startLine);
    testing_set_object_int(state, object, "startColumn", span->startColumn);
    testing_set_object_int(state, object, "endLine", span->endLine);
    testing_set_object_int(state, object, "endColumn", span->endColumn);
    return object;
}

static SZrObject *testing_build_snapshot_object(SZrState *state,
                                                const SZrTestingValueSnapshot *snapshot) {
    SZrObject *object = testing_new_record(state, "ValueSnapshot");
    if (object == ZR_NULL || snapshot == ZR_NULL) {
        return object;
    }
    testing_set_object_string(state, object, "typeName", snapshot->typeName);
    testing_set_object_string(state, object, "text", snapshot->text);
    testing_set_object_bool(state, object, "hasValue", snapshot->hasValue);
    testing_set_object_bool(state, object, "truncated", snapshot->truncated);
    testing_set_object_bool(state, object, "formatterFaulted", snapshot->formatterFaulted);
    return object;
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
            testing_set_object_string(state, object, "message", g_last_failure.message);
            testing_set_object_record(state, object, "sourceSpan",
                                      testing_build_source_span_object(state, &g_last_failure.sourceSpan));
            testing_set_object_record(state, object, "expected",
                                      testing_build_snapshot_object(state, &g_last_failure.expected));
            testing_set_object_record(state, object, "actual",
                                      testing_build_snapshot_object(state, &g_last_failure.actual));
            testing_set_object_record(state, object, "exception",
                                      testing_build_snapshot_object(state, &g_last_failure.exception));
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
    testing_source_span(context, &g_last_failure.sourceSpan);
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

static TZrBool testing_read_expected_type_name(ZrLibCallContext *context,
                                               SZrString **outTypeName) {
    SZrTypeValue *typeValue;
    SZrObject *typeObject;
    SZrReflectionTypeIdentity identity;
    SZrObjectPrototype *prototype;
    SZrObjectPrototype *errorPrototype;
    const TZrChar *typeNameText;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (context == ZR_NULL || outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    typeValue = ZrLib_CallContext_Argument(context, 1U);
    if (typeValue == ZR_NULL || typeValue->type != ZR_VALUE_TYPE_OBJECT ||
        typeValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    typeObject = ZR_CAST_OBJECT(context->state, typeValue->value.object);
    memset(&identity, 0, sizeof(identity));
    if (!ZrCore_Reflection_ReadTypeIdObject(
                context->state, typeObject, &identity, outTypeName) ||
        *outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    typeNameText = ZrCore_String_GetNativeString(*outTypeName);
    prototype = typeNameText != ZR_NULL
                        ? ZrLib_Type_FindPrototype(context->state, typeNameText)
                        : ZR_NULL;
    errorPrototype = context->state->global->errorPrototype;
    if (errorPrototype == ZR_NULL) {
        errorPrototype = ZrLib_Type_FindPrototype(context->state, "Error");
    }
    if (errorPrototype == ZR_NULL) {
        return ZR_FALSE;
    }
    while (prototype != ZR_NULL && prototype != errorPrototype) {
        prototype = prototype->superPrototype;
    }
    return prototype == errorPrototype;
}

TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *callable;
    SZrString *expectedTypeName;
    SZrTestingCallRequest request;
    EZrThreadStatus status;

    if (context == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_CheckArity(context, 2U, 2U) ||
        !ZrLib_CallContext_ReadFunction(context, 0U, &callable)) {
        return ZR_FALSE;
    }
    if (!testing_read_expected_type_name(context, &expectedTypeName)) {
        return ZR_FALSE;
    }
    memset(&request, 0, sizeof(request));
    request.callable = callable;
    status = testing_try_run_preserving_frame(
            context->state, testing_call_try, &request);
    /* The callback may collect or relocate managed arguments. */
    if (!testing_read_expected_type_name(context, &expectedTypeName)) {
        return ZR_FALSE;
    }
    if (status == ZR_THREAD_STATUS_FINE && request.completed) {
        testing_begin_failure(
                context, ZR_TESTING_ASSERTION_KIND_THROWS, "action completed without throwing");
        g_last_failure.expected.hasValue = ZR_TRUE;
        testing_copy_bounded(g_last_failure.expected.typeName,
                             sizeof(g_last_failure.expected.typeName),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
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
    if (!ZrCore_Exception_CatchMatchesTypeName(context->state,
                                               &context->state->currentException,
                                               expectedTypeName)) {
        SZrTestingValueSnapshot exceptionSnapshot = g_last_failure.exception;
        testing_begin_failure(context,
                              ZR_TESTING_ASSERTION_KIND_THROWS,
                              "thrown exception type does not match E");
        g_last_failure.expected.hasValue = ZR_TRUE;
        testing_copy_bounded(g_last_failure.expected.typeName,
                             sizeof(g_last_failure.expected.typeName),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
        testing_copy_bounded(g_last_failure.expected.text,
                             sizeof(g_last_failure.expected.text),
                             ZrCore_String_GetNativeString(expectedTypeName),
                             ZR_NULL);
        g_last_failure.exception = exceptionSnapshot;
        ZrCore_Exception_ClearCurrent(context->state);
        context->state->threadStatus = ZR_THREAD_STATUS_FINE;
        testing_raise_failure(context);
    }
    ZrCore_Value_Copy(context->state, result, &context->state->currentException);
    ZrCore_Exception_ClearCurrent(context->state);
    context->state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrVmLibTesting_ClearLastFailure();
    return ZR_TRUE;
}
