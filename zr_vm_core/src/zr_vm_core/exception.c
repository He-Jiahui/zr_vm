//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/exception.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#if defined(__cplusplus) && !defined(ZR_EXCEPTION_WITH_LONG_JUMP)
#define ZR_EXCEPTION_NATIVE_THROW(state, context) throw(context)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    try {                                                                                                              \
        block                                                                                                          \
    } catch (...) {                                                                                                    \
        if ((context)->status == 0) {                                                                                  \
            (context)->status = -1;                                                                                    \
        }                                                                                                              \
    }
#elif defined(ZR_PLATFORM_UNIX)
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    if (setjmp((context)->jumpBuffer) == 0) {                                                                          \
        block                                                                                                          \
    }
#else
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    if (setjmp((context)->jumpBuffer) == 0) {                                                                          \
        block                                                                                                          \
    }
#endif

static void exception_set_object_field_cstring(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldString = ZrCore_String_CreateFromNative(state, fieldName);
    if (fieldString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const SZrTypeValue *exception_get_object_field_cstring(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static SZrObject *exception_new_array(SZrState *state) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }

    return array;
}

static TZrBool exception_array_push_value(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)array->nodeMap.elementCount);
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

static SZrObjectPrototype *exception_lookup_prototype(SZrState *state, SZrString *typeName) {
    SZrObject *zrObject;
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, zrObject, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZR_CAST_OBJECT(state, value->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObjectPrototype *exception_lookup_prototype_cstring(SZrState *state, const TZrChar *typeName) {
    SZrString *typeString;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeString = ZrCore_String_CreateFromNative(state, typeName);
    if (typeString == ZR_NULL) {
        return ZR_NULL;
    }

    return exception_lookup_prototype(state, typeString);
}

static TZrBool exception_prototype_inherits(SZrObjectPrototype *prototype, SZrObjectPrototype *target) {
    while (prototype != ZR_NULL) {
        if (prototype == target) {
            return ZR_TRUE;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_FALSE;
}

static TZrBool exception_value_is_error_object(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || object->prototype == ZR_NULL || state->global == ZR_NULL ||
        state->global->errorPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return exception_prototype_inherits(object->prototype, state->global->errorPrototype);
}

static SZrObjectPrototype *exception_status_prototype(SZrState *state, EZrThreadStatus status) {
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    switch (status) {
        case ZR_THREAD_STATUS_MEMORY_ERROR:
            prototype = exception_lookup_prototype_cstring(state, "MemoryError");
            break;
        case ZR_THREAD_STATUS_EXCEPTION_ERROR:
            prototype = exception_lookup_prototype_cstring(state, "ExceptionError");
            break;
        case ZR_THREAD_STATUS_RUNTIME_ERROR:
        default:
            prototype = exception_lookup_prototype_cstring(state, "RuntimeError");
            break;
    }

    return prototype != ZR_NULL ? prototype : state->global->errorPrototype;
}

static TZrMemoryOffset exception_compute_instruction_offset(const SZrCallInfo *callInfo, const SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL) {
        return 0;
    }

    if (callInfo->context.context.programCounter < function->instructionsList) {
        return 0;
    }

    return (TZrMemoryOffset)(callInfo->context.context.programCounter - function->instructionsList);
}

static SZrFunction *exception_call_info_function(SZrState *state, SZrCallInfo *callInfo) {
    SZrTypeValue *baseValue;

    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    baseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (baseValue == ZR_NULL || baseValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (baseValue->type == ZR_VALUE_TYPE_CLOSURE) {
        if (baseValue->isNative) {
            return ZR_NULL;
        }
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, baseValue->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }
    if (baseValue->type == ZR_VALUE_TYPE_FUNCTION) {
        if (baseValue->isNative) {
            return ZR_NULL;
        }
        return ZR_CAST_FUNCTION(state, baseValue->value.object);
    }

    return ZR_NULL;
}

static void exception_set_message_field_from_value(SZrState *state, SZrObject *object, const SZrTypeValue *value) {
    SZrTypeValue messageValue;
    SZrTypeValue stableValue;
    SZrString *messageString;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&messageValue);
    } else {
        stableValue = *value;
        messageString = ZrCore_Value_ConvertToString(state, &stableValue);
        if (messageString == ZR_NULL) {
            ZrCore_Value_ResetAsNull(&messageValue);
        } else {
            ZrCore_Value_InitAsRawObject(state, &messageValue, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
            messageValue.type = ZR_VALUE_TYPE_STRING;
        }
    }

    exception_set_object_field_cstring(state, object, "message", &messageValue);
}

static SZrObject *exception_capture_stack_frames(SZrState *state, SZrCallInfo *throwCallInfo) {
    SZrObject *frames;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->stackFramePrototype == ZR_NULL) {
        return ZR_NULL;
    }

    frames = exception_new_array(state);
    if (frames == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = throwCallInfo != ZR_NULL ? throwCallInfo : state->callInfoList;
    while (callInfo != ZR_NULL) {
        SZrFunction *function = exception_call_info_function(state, callInfo);
        TZrMemoryOffset instructionOffset;
        TZrUInt32 sourceLine;
        SZrObject *frameObject;
        SZrTypeValue frameValue;
        SZrTypeValue stringValue;
        SZrTypeValue intValue;

        if (function == ZR_NULL) {
            callInfo = callInfo->previous;
            continue;
        }

        instructionOffset = exception_compute_instruction_offset(callInfo, function);
        sourceLine = ZrCore_Exception_FindSourceLine(function, instructionOffset);
        frameObject = ZrCore_Object_New(state, state->global->stackFramePrototype);
        if (frameObject == ZR_NULL) {
            break;
        }
        ZrCore_Object_Init(state, frameObject);

        if (function->functionName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->functionName));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, frameObject, "functionName", &stringValue);

        if (function->sourceCodeList != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceCodeList));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, frameObject, "sourceFile", &stringValue);

        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)sourceLine);
        exception_set_object_field_cstring(state, frameObject, "sourceLine", &intValue);
        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)instructionOffset);
        exception_set_object_field_cstring(state, frameObject, "instructionOffset", &intValue);

        ZrCore_Value_InitAsRawObject(state, &frameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(frameObject));
        frameValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!exception_array_push_value(state, frames, &frameValue)) {
            break;
        }

        callInfo = callInfo->previous;
    }

    return frames;
}

static TZrBool exception_apply_error_fields(SZrState *state,
                                            SZrObject *errorObject,
                                            const SZrTypeValue *messageSource,
                                            const SZrTypeValue *exceptionValue,
                                            SZrCallInfo *throwCallInfo) {
    SZrObject *frames;
    SZrTypeValue value;

    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    frames = exception_capture_stack_frames(state, throwCallInfo);
    if (frames == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(frames));
    value.type = ZR_VALUE_TYPE_ARRAY;
    exception_set_object_field_cstring(state, errorObject, "stacks", &value);
    exception_set_message_field_from_value(state, errorObject, messageSource);

    if (exceptionValue != ZR_NULL) {
        exception_set_object_field_cstring(state, errorObject, "exception", exceptionValue);
    } else {
        ZrCore_Value_ResetAsNull(&value);
        exception_set_object_field_cstring(state, errorObject, "exception", &value);
    }

    return ZR_TRUE;
}

static const SZrTypeValue *exception_error_message_source(SZrState *state,
                                                          SZrObject *errorObject,
                                                          const SZrTypeValue *fallback) {
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return fallback;
    }

    messageValue = exception_get_object_field_cstring(state, errorObject, "message");
    if (messageValue == ZR_NULL || messageValue->type == ZR_VALUE_TYPE_NULL) {
        return fallback;
    }

    return messageValue;
}

static TZrBool exception_set_current_error_object(SZrState *state, SZrObject *errorObject, EZrThreadStatus status) {
    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &state->currentException, ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject));
    state->currentException.type = ZR_VALUE_TYPE_OBJECT;
    state->currentExceptionStatus = status;
    state->hasCurrentException = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool exception_create_status_error(SZrState *state,
                                             EZrThreadStatus status,
                                             const SZrTypeValue *payload,
                                             SZrCallInfo *throwCallInfo) {
    SZrObjectPrototype *prototype;
    SZrObject *errorObject;
    SZrTypeValue selfValue;
    SZrTypeValue memoryMessageValue;
    const SZrTypeValue *messageSource;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = exception_status_prototype(state, status);
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, errorObject);

    ZrCore_Value_InitAsRawObject(state, &selfValue, ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject));
    selfValue.type = ZR_VALUE_TYPE_OBJECT;
    messageSource = payload;
    if (messageSource == ZR_NULL && status == ZR_THREAD_STATUS_MEMORY_ERROR && state->global != ZR_NULL &&
        state->global->memoryErrorMessage != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &memoryMessageValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
        memoryMessageValue.type = ZR_VALUE_TYPE_STRING;
        messageSource = &memoryMessageValue;
    }
    if (!exception_apply_error_fields(state,
                                      errorObject,
                                      messageSource != ZR_NULL ? messageSource : &selfValue,
                                      payload != ZR_NULL ? payload : &selfValue,
                                      throwCallInfo)) {
        return ZR_FALSE;
    }

    return exception_set_current_error_object(state, errorObject, status);
}

EZrThreadStatus ZrCore_Exception_TryRun(SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments) {
    TZrUInt32 prevNestedNativeCalls = state->nestedNativeCalls;
    SZrExceptionLongJump exceptionLongJump;

    exceptionLongJump.status = ZR_THREAD_STATUS_FINE;
    exceptionLongJump.previous = state->exceptionRecoverPoint;
    state->exceptionRecoverPoint = &exceptionLongJump;
    ZR_EXCEPTION_NATIVE_TRY(state, &exceptionLongJump, { tryFunction(state, arguments); });
    state->exceptionRecoverPoint = exceptionLongJump.previous;
    state->nestedNativeCalls = prevNestedNativeCalls;
    return exceptionLongJump.status;
}

void ZrCore_Exception_Throw(SZrState *state, EZrThreadStatus errorCode) {
    if (state->exceptionRecoverPoint != ZR_NULL) {
        state->exceptionRecoverPoint->status = errorCode;
        ZR_EXCEPTION_NATIVE_THROW(state, state->exceptionRecoverPoint);
    }

    if (state == ZR_NULL || state->global == ZR_NULL) {
        ZR_ABORT();
    }

    if (state != state->global->mainThreadState) {
        errorCode = ZrCore_State_ResetThread(state, errorCode);
        state->threadStatus = errorCode;
        if (state->global->mainThreadState != ZR_NULL && state->global->mainThreadState->exceptionRecoverPoint != ZR_NULL) {
            state->global->mainThreadState->currentException = state->currentException;
            state->global->mainThreadState->currentExceptionStatus = state->currentExceptionStatus;
            state->global->mainThreadState->hasCurrentException = state->hasCurrentException;
            ZrCore_Exception_Throw(state->global->mainThreadState, errorCode);
        }
    }

    if (state->global->panicHandlingFunction != ZR_NULL) {
        ZR_THREAD_UNLOCK(state);
        state->global->panicHandlingFunction(state);
    }
    ZR_ABORT();
}

EZrThreadStatus ZrCore_Exception_TryStop(SZrState *state, TZrMemoryOffset level, EZrThreadStatus status) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(level);
    return status;
}

void ZrCore_Exception_MarkError(SZrState *state, EZrThreadStatus errorCode, TZrStackValuePointer previousTop) {
    if (state == ZR_NULL || previousTop == ZR_NULL) {
        return;
    }

    switch (errorCode) {
        case ZR_THREAD_STATUS_FINE:
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            break;
        case ZR_THREAD_STATUS_MEMORY_ERROR:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else {
                ZrCore_Stack_SetRawObjectValue(state, previousTop,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
            }
            break;
        default:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else if (state->stackTop.valuePointer > state->stackBase.valuePointer) {
                ZrCore_Stack_CopyValue(state, previousTop, &(state->stackTop.valuePointer - 1)->value);
            } else {
                ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            }
            break;
    }

    state->stackTop.valuePointer = previousTop - 1;
}

void ZrCore_Exception_ClearCurrent(struct SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
}

TZrBool ZrCore_Exception_NormalizeThrownValue(struct SZrState *state,
                                              const SZrTypeValue *payload,
                                              struct SZrCallInfo *throwCallInfo,
                                              EZrThreadStatus status) {
    SZrObject *errorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue payloadCopy;
    SZrTypeValue selfValue;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->errorPrototype == ZR_NULL || payload == ZR_NULL) {
        return ZR_FALSE;
    }

    payloadCopy = *payload;
    if (exception_value_is_error_object(state, &payloadCopy)) {
        errorObject = ZR_CAST_OBJECT(state, payloadCopy.value.object);
        selfValue = payloadCopy;
        if (!exception_apply_error_fields(state,
                                          errorObject,
                                          exception_error_message_source(state, errorObject, &payloadCopy),
                                          &selfValue,
                                          throwCallInfo)) {
            return ZR_FALSE;
        }
        return exception_set_current_error_object(state, errorObject, status);
    }

    prototype = state->global->errorPrototype;
    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, errorObject);

    if (!exception_apply_error_fields(state, errorObject, &payloadCopy, &payloadCopy, throwCallInfo)) {
        return ZR_FALSE;
    }

    return exception_set_current_error_object(state, errorObject, status);
}

TZrBool ZrCore_Exception_NormalizeStatus(struct SZrState *state, EZrThreadStatus status) {
    const SZrTypeValue *payload = ZR_NULL;
    SZrCallInfo *throwCallInfo;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        state->currentExceptionStatus = status;
        return ZR_TRUE;
    }

    throwCallInfo = state->callInfoList;
    if (state->stackTop.valuePointer != ZR_NULL && state->stackBase.valuePointer != ZR_NULL &&
        state->stackTop.valuePointer > state->stackBase.valuePointer) {
        payload = &(state->stackTop.valuePointer - 1)->value;
    }

    return exception_create_status_error(state, status, payload, throwCallInfo);
}

TZrBool ZrCore_Exception_CatchMatchesTypeName(struct SZrState *state,
                                              const SZrTypeValue *errorValue,
                                              struct SZrString *typeName) {
    SZrObjectPrototype *expectedPrototype;
    SZrObject *errorObject;

    if (state == ZR_NULL || errorValue == ZR_NULL || !exception_value_is_error_object(state, errorValue)) {
        return ZR_FALSE;
    }

    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject == ZR_NULL || errorObject->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedPrototype = typeName != ZR_NULL ? exception_lookup_prototype(state, typeName) : state->global->errorPrototype;
    if (expectedPrototype == ZR_NULL) {
        expectedPrototype = state->global->errorPrototype;
    }

    return exception_prototype_inherits(errorObject->prototype, expectedPrototype);
}

TZrUInt32 ZrCore_Exception_FindSourceLine(struct SZrFunction *function, TZrMemoryOffset instructionOffset) {
    TZrUInt32 bestLine = 0;

    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL || function->executionLocationInfoLength == 0) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
        SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if (info->currentInstructionOffset > instructionOffset) {
            break;
        }
        bestLine = info->lineInSource;
    }

    return bestLine;
}

void ZrCore_Exception_PrintUnhandled(struct SZrState *state, const SZrTypeValue *errorValue, FILE *stream) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;
    const SZrTypeValue *payloadValue;
    const SZrTypeValue *stacksValue;
    const TZrChar *typeName = "Error";
    SZrString *payloadSummary = ZR_NULL;
    FILE *output = stream != ZR_NULL ? stream : stderr;

    if (state == ZR_NULL || errorValue == ZR_NULL || output == ZR_NULL) {
        return;
    }

    if (!exception_value_is_error_object(state, errorValue)) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)errorValue);
        fprintf(output, "Unhandled exception: %s\n", payloadSummary != ZR_NULL ? ZrCore_String_GetNativeString(payloadSummary) : "<unknown>");
        fflush(output);
        return;
    }

    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject != ZR_NULL && errorObject->prototype != ZR_NULL && errorObject->prototype->name != ZR_NULL) {
        typeName = ZrCore_String_GetNativeString(errorObject->prototype->name);
    }

    messageValue = exception_get_object_field_cstring(state, errorObject, "message");
    payloadValue = exception_get_object_field_cstring(state, errorObject, "exception");
    stacksValue = exception_get_object_field_cstring(state, errorObject, "stacks");
    if (payloadValue != ZR_NULL) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)payloadValue);
    }

    fprintf(output, "%s", typeName != ZR_NULL ? typeName : "Error");
    if (messageValue != ZR_NULL && messageValue->type == ZR_VALUE_TYPE_STRING && messageValue->value.object != ZR_NULL) {
        fprintf(output, ": %s", ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object)));
    }
    fprintf(output, "\n");
    if (payloadSummary != ZR_NULL) {
        fprintf(output, "payload: %s\n", ZrCore_String_GetNativeString(payloadSummary));
    }

    if (stacksValue != ZR_NULL && stacksValue->type == ZR_VALUE_TYPE_ARRAY && stacksValue->value.object != ZR_NULL) {
        SZrObject *frames = ZR_CAST_OBJECT(state, stacksValue->value.object);
        TZrSize frameCount = frames->nodeMap.elementCount;
        for (TZrSize index = 0; index < frameCount; index++) {
            SZrTypeValue key;
            const SZrTypeValue *frameValue;
            const SZrTypeValue *functionNameValue;
            const SZrTypeValue *sourceFileValue;
            const SZrTypeValue *sourceLineValue;
            const SZrTypeValue *instructionOffsetValue;
            const TZrChar *functionName = "<anonymous>";
            const TZrChar *sourceFile = "<unknown>";
            TZrInt64 sourceLine = 0;
            TZrInt64 instructionOffset = 0;

            ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
            frameValue = ZrCore_Object_GetValue(state, frames, &key);
            if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
                continue;
            }

            functionNameValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "functionName");
            sourceFileValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceFile");
            sourceLineValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceLine");
            instructionOffsetValue =
                    exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "instructionOffset");

            if (functionNameValue != ZR_NULL && functionNameValue->type == ZR_VALUE_TYPE_STRING && functionNameValue->value.object != ZR_NULL) {
                functionName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, functionNameValue->value.object));
            }
            if (sourceFileValue != ZR_NULL && sourceFileValue->type == ZR_VALUE_TYPE_STRING && sourceFileValue->value.object != ZR_NULL) {
                sourceFile = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, sourceFileValue->value.object));
            }
            if (sourceLineValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(sourceLineValue->type)) {
                sourceLine = sourceLineValue->value.nativeObject.nativeInt64;
            }
            if (instructionOffsetValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(instructionOffsetValue->type)) {
                instructionOffset = instructionOffsetValue->value.nativeObject.nativeInt64;
            }

            fprintf(output, "  at %s (%s:%lld, ip=%lld)\n",
                    functionName != ZR_NULL ? functionName : "<anonymous>",
                    sourceFile != ZR_NULL ? sourceFile : "<unknown>",
                    (long long)sourceLine,
                    (long long)instructionOffset);
        }
    }

    fflush(output);
}
