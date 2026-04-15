#include "ffi_runtime/ffi_runtime_internal.h"

TZrBool zr_ffi_extract_numeric_value(const SZrTypeValue *value, double *outDouble) {
    if (value == ZR_NULL || outDouble == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outDouble = (double) value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outDouble = (double) value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outDouble = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool zr_ffi_build_struct_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            unsigned char *buffer, char *errorBuffer, TZrSize errorBufferSize) {
    TZrSize index;
    SZrObject *object;

    if (!zr_ffi_value_is_object(value, &object)) {
        snprintf(errorBuffer, errorBufferSize, "expected object value for aggregate argument");
        return ZR_FALSE;
    }

    memset(buffer, 0, type->size);
    for (index = 0; index < type->as.aggregate.fieldCount; index++) {
        const ZrFfiFieldLayout *field = &type->as.aggregate.fields[index];
        const SZrTypeValue *fieldValue = zr_ffi_find_field_raw(state, object, field->name);
        if (fieldValue == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "missing field '%s' in aggregate value", field->name);
            return ZR_FALSE;
        }
        if (!zr_ffi_build_scalar_argument(state, fieldValue, field->type, buffer + field->offset, errorBuffer,
                                          errorBufferSize)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_ffi_object_uses_pointer_lowering(SZrState *state, SZrObject *object) {
    const char *loweringKind = ZR_NULL;

    if (state == ZR_NULL || object == ZR_NULL || object->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_ffi_read_object_string_field(state, &object->prototype->super, "__zr_ffiLoweringKind", &loweringKind) &&
           loweringKind != ZR_NULL &&
           strcmp(loweringKind, "pointer") == 0;
}

static TZrBool zr_ffi_object_uses_handle_id_lowering(SZrState *state,
                                                     SZrObject *object,
                                                     const char **outUnderlyingTypeName) {
    const char *loweringKind = ZR_NULL;

    if (outUnderlyingTypeName != ZR_NULL) {
        *outUnderlyingTypeName = ZR_NULL;
    }

    if (state == ZR_NULL || object == ZR_NULL || object->prototype == ZR_NULL ||
        !zr_ffi_read_object_string_field(state, &object->prototype->super, "__zr_ffiLoweringKind", &loweringKind) ||
        loweringKind == ZR_NULL || strcmp(loweringKind, "handle_id") != 0) {
        return ZR_FALSE;
    }

    return outUnderlyingTypeName == ZR_NULL ||
           zr_ffi_read_object_string_field(state,
                                           &object->prototype->super,
                                           "__zr_ffiUnderlyingTypeName",
                                           outUnderlyingTypeName);
}

static TZrBool zr_ffi_try_lower_pointer_wrapper(SZrState *state, const SZrTypeValue *value, void **outPointer) {
    SZrObject *wrapperObject = ZR_NULL;
    ZrFfiHandleData *handleData = ZR_NULL;

    if (outPointer != ZR_NULL) {
        *outPointer = ZR_NULL;
    }

    if (state == ZR_NULL || value == ZR_NULL || outPointer == ZR_NULL || !zr_ffi_value_is_object(value, &wrapperObject) ||
        !zr_ffi_object_uses_pointer_lowering(state, wrapperObject)) {
        return ZR_FALSE;
    }

    handleData = zr_ffi_get_handle_data(state, wrapperObject);
    if (handleData == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (handleData->kind) {
        case ZR_FFI_HANDLE_POINTER: {
            ZrFfiPointerData *pointerData = (ZrFfiPointerData *)handleData;
            if (pointerData->closed) {
                return ZR_FALSE;
            }
            *outPointer = pointerData->address;
            return ZR_TRUE;
        }
        case ZR_FFI_HANDLE_BUFFER: {
            ZrFfiBufferData *bufferData = (ZrFfiBufferData *)handleData;
            if (bufferData->closeRequested) {
                return ZR_FALSE;
            }
            *outPointer = bufferData->bytes;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static const char *zr_ffi_integer_type_name_for_layout(const ZrFfiTypeLayout *type) {
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    switch (type->kind) {
        case ZR_FFI_TYPE_I8:
            return "i8";
        case ZR_FFI_TYPE_U8:
            return "u8";
        case ZR_FFI_TYPE_I16:
            return "i16";
        case ZR_FFI_TYPE_U16:
            return "u16";
        case ZR_FFI_TYPE_I32:
            return "i32";
        case ZR_FFI_TYPE_U32:
            return "u32";
        case ZR_FFI_TYPE_I64:
            return "i64";
        case ZR_FFI_TYPE_U64:
            return "u64";
        default:
            return ZR_NULL;
    }
}

static TZrBool zr_ffi_try_read_handle_id_field(SZrState *state, SZrObject *wrapperObject, SZrTypeValue *outValue) {
    const SZrTypeValue *fieldValue;
    TZrBool closed = ZR_FALSE;

    if (outValue != ZR_NULL) {
        ZrLib_Value_SetNull(outValue);
    }

    if (state == ZR_NULL || wrapperObject == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_ffi_read_object_bool_field(state, wrapperObject, "closed", &closed) && closed) {
        return ZR_FALSE;
    }

    fieldValue = zr_ffi_find_field_raw(state, wrapperObject, ZR_FFI_HIDDEN_HANDLE_ID_FIELD);
    if (fieldValue == ZR_NULL) {
        fieldValue = zr_ffi_find_field_raw(state, wrapperObject, "handleId");
    }
    if (fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *outValue = *fieldValue;
    return ZR_TRUE;
}

static TZrBool zr_ffi_try_lower_handle_id_wrapper(SZrState *state,
                                                  const SZrTypeValue *value,
                                                  const ZrFfiTypeLayout *type,
                                                  SZrTypeValue *outLoweredValue) {
    SZrObject *wrapperObject = ZR_NULL;
    const char *underlyingTypeName = ZR_NULL;
    const char *targetTypeName;

    if (outLoweredValue != ZR_NULL) {
        ZrLib_Value_SetNull(outLoweredValue);
    }

    targetTypeName = zr_ffi_integer_type_name_for_layout(type);
    if (state == ZR_NULL || value == ZR_NULL || outLoweredValue == ZR_NULL || targetTypeName == ZR_NULL ||
        !zr_ffi_value_is_object(value, &wrapperObject) ||
        !zr_ffi_object_uses_handle_id_lowering(state, wrapperObject, &underlyingTypeName) ||
        underlyingTypeName == ZR_NULL || strcmp(targetTypeName, underlyingTypeName) != 0) {
        return ZR_FALSE;
    }

    return zr_ffi_try_read_handle_id_field(state, wrapperObject, outLoweredValue);
}

TZrBool zr_ffi_build_scalar_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            void *buffer, char *errorBuffer, TZrSize errorBufferSize) {
    double numericValue = 0.0;
    SZrTypeValue loweredValue;
    const SZrTypeValue *effectiveValue = value;

    if (type == ZR_NULL || buffer == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "invalid scalar argument target");
        return ZR_FALSE;
    }

    if (zr_ffi_try_lower_handle_id_wrapper(state, value, type, &loweredValue)) {
        effectiveValue = &loweredValue;
    }

    switch (type->kind) {
        case ZR_FFI_TYPE_BOOL:
            if (!zr_ffi_read_bool_value(effectiveValue, (TZrBool *) buffer)) {
                snprintf(errorBuffer, errorBufferSize, "expected bool-compatible value");
                return ZR_FALSE;
            }
            return ZR_TRUE;
        case ZR_FFI_TYPE_I8:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(int8_t *) buffer = (int8_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U8:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(uint8_t *) buffer = (uint8_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I16:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(int16_t *) buffer = (int16_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U16:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(uint16_t *) buffer = (uint16_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I32:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(int32_t *) buffer = (int32_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U32:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(uint32_t *) buffer = (uint32_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I64:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(int64_t *) buffer = (int64_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U64:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(uint64_t *) buffer = (uint64_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_F32:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(float *) buffer = (float) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_F64:
            if (!zr_ffi_extract_numeric_value(effectiveValue, &numericValue)) {
                break;
            }
            *(double *) buffer = (double) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_STRING: {
            const char *text = ZR_NULL;
            if (!zr_ffi_read_string_value(state, effectiveValue, &text)) {
                break;
            }
            *(const char **) buffer = text;
            return ZR_TRUE;
        }
        case ZR_FFI_TYPE_POINTER: {
            SZrObject *pointerObject = ZR_NULL;
            ZrFfiPointerData *pointerData = ZR_NULL;
            void *loweredPointer = ZR_NULL;
            if (value->type == ZR_VALUE_TYPE_NULL) {
                *(void **) buffer = ZR_NULL;
                return ZR_TRUE;
            }
            if (zr_ffi_try_lower_pointer_wrapper(state, value, &loweredPointer)) {
                *(void **) buffer = loweredPointer;
                return ZR_TRUE;
            }
            if (!zr_ffi_value_is_object(value, &pointerObject)) {
                break;
            }
            pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(state, pointerObject);
            if (pointerData == ZR_NULL || pointerData->base.kind != ZR_FFI_HANDLE_POINTER || pointerData->closed) {
                break;
            }
            *(void **) buffer = pointerData->address;
            return ZR_TRUE;
        }
        case ZR_FFI_TYPE_ENUM:
            return zr_ffi_build_scalar_argument(state, value, type->as.enumType.underlying, buffer, errorBuffer,
                                                errorBufferSize);
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            return zr_ffi_build_struct_argument(state, value, type, (unsigned char *) buffer, errorBuffer,
                                                errorBufferSize);
        default:
            break;
    }

    snprintf(errorBuffer, errorBufferSize, "unsupported value for ffi argument");
    return ZR_FALSE;
}

void zr_ffi_callback_try_invoke(SZrState *state, TZrPtr arguments) {
    ZrFfiCallbackInvokeArgs *invokeArgs = (ZrFfiCallbackInvokeArgs *)arguments;

    if (state == ZR_NULL || invokeArgs == ZR_NULL || invokeArgs->result == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNull(invokeArgs->result);
    invokeArgs->succeeded =
            ZrLib_CallValue(state, invokeArgs->callbackValue, ZR_NULL, invokeArgs->argumentValues, invokeArgs->argumentCount, invokeArgs->result);
}

TZrBool zr_ffi_struct_to_object(SZrState *state, ZrFfiTypeLayout *type, const unsigned char *bytes,
                                       SZrTypeValue *result) {
    TZrSize index;
    SZrObject *object;
    ZrLibTempValueRoot objectRoot;

    if (!ZrLib_TempValueRoot_Begin(state, &objectRoot)) {
        return ZR_FALSE;
    }
    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }
    ZrLib_TempValueRoot_SetObject(&objectRoot, object, ZR_VALUE_TYPE_OBJECT);

    for (index = 0; index < type->as.aggregate.fieldCount; index++) {
        SZrTypeValue fieldValue;
        const ZrFfiFieldLayout *field = &type->as.aggregate.fields[index];
        if (!zr_ffi_set_result_from_scalar(state, field->type, bytes + field->offset, &fieldValue)) {
            ZrLib_TempValueRoot_End(&objectRoot);
            return ZR_FALSE;
        }
        ZrLib_Object_SetFieldCString(state, object, field->name, &fieldValue);
    }

    ZrLib_Value_SetObject(state, result, object, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&objectRoot);
    return ZR_TRUE;
}

TZrBool zr_ffi_set_result_from_scalar(SZrState *state, ZrFfiTypeLayout *type, const void *value,
                                             SZrTypeValue *result) {
    switch (type->kind) {
        case ZR_FFI_TYPE_VOID:
            ZrLib_Value_SetNull(result);
            return ZR_TRUE;
        case ZR_FFI_TYPE_BOOL:
            ZrLib_Value_SetBool(state, result, (*(const unsigned char *) value) != 0 ? ZR_TRUE : ZR_FALSE);
            return ZR_TRUE;
        case ZR_FFI_TYPE_I8:
            ZrLib_Value_SetInt(state, result, *(const int8_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_U8:
            ZrLib_Value_SetInt(state, result, *(const uint8_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_I16:
            ZrLib_Value_SetInt(state, result, *(const int16_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_U16:
            ZrLib_Value_SetInt(state, result, *(const uint16_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_I32:
            ZrLib_Value_SetInt(state, result, *(const int32_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_U32:
            ZrLib_Value_SetInt(state, result, (TZrInt64) * (const uint32_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_I64:
            ZrLib_Value_SetInt(state, result, *(const int64_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_U64:
            ZrLib_Value_SetInt(state, result, (TZrInt64) * (const uint64_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_F32:
            ZrLib_Value_SetFloat(state, result, *(const float *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_F64:
            ZrLib_Value_SetFloat(state, result, *(const double *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_STRING:
            ZrLib_Value_SetString(state, result,
                                  *(const char *const *) value != ZR_NULL ? *(const char *const *) value : "");
            return ZR_TRUE;
        case ZR_FFI_TYPE_ENUM:
            return zr_ffi_set_result_from_scalar(state, type->as.enumType.underlying, value, result);
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            return zr_ffi_struct_to_object(state, type, (const unsigned char *) value, result);
        default:
            return ZR_FALSE;
    }
}

void zr_ffi_symbol_release_owner(SZrState *state, SZrObject *object) {
    const SZrTypeValue *ownerValue = zr_ffi_find_field_raw(state, object, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue != ZR_NULL && ownerValue->type == ZR_VALUE_TYPE_OBJECT && ownerValue->value.object != ZR_NULL) {
        SZrObject *ownerObject = ZR_CAST_OBJECT(state, ownerValue->value.object);
        ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(state, ownerObject);
        if (libraryData != ZR_NULL && libraryData->base.kind == ZR_FFI_HANDLE_LIBRARY) {
            if (libraryData->openSymbolCount > 0) {
                libraryData->openSymbolCount--;
            }
            if (libraryData->closeRequested && libraryData->openSymbolCount == 0 &&
                libraryData->libraryHandle != ZR_NULL) {
                zr_ffi_close_dynamic_library(libraryData->libraryHandle);
                libraryData->libraryHandle = ZR_NULL;
            }
        }
    }
}

void zr_ffi_pointer_release_owner(SZrState *state, SZrObject *object) {
    const SZrTypeValue *ownerValue = zr_ffi_find_field_raw(state, object, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue != ZR_NULL && ownerValue->type == ZR_VALUE_TYPE_OBJECT && ownerValue->value.object != ZR_NULL) {
        SZrObject *ownerObject = ZR_CAST_OBJECT(state, ownerValue->value.object);
        ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(state, ownerObject);
        if (bufferData != ZR_NULL && bufferData->base.kind == ZR_FFI_HANDLE_BUFFER) {
            if (bufferData->pinCount > 0) {
                bufferData->pinCount--;
            }
            if (bufferData->closeRequested && bufferData->pinCount == 0 && bufferData->bytes != ZR_NULL) {
                free(bufferData->bytes);
                bufferData->bytes = ZR_NULL;
                bufferData->size = 0;
            }
        }
    }
}

void zr_ffi_handle_finalize(SZrState *state, SZrRawObject *rawObject) {
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    ZrFfiHandleData *handleData = zr_ffi_get_handle_data(state, object);

    if (handleData == ZR_NULL || handleData->finalized) {
        return;
    }
    handleData->finalized = ZR_TRUE;

    switch (handleData->kind) {
        case ZR_FFI_HANDLE_LIBRARY: {
            ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) handleData;
            if (libraryData->libraryHandle != ZR_NULL) {
                zr_ffi_close_dynamic_library(libraryData->libraryHandle);
            }
            free(libraryData->libraryPath);
            free(libraryData);
            break;
        }
        case ZR_FFI_HANDLE_SYMBOL: {
            ZrFfiSymbolData *symbolData = (ZrFfiSymbolData *) handleData;
            zr_ffi_symbol_release_owner(state, object);
            zr_ffi_destroy_signature(symbolData->signature);
            free(symbolData->symbolName);
            free(symbolData);
            break;
        }
        case ZR_FFI_HANDLE_CALLBACK: {
            ZrFfiCallbackData *callbackData = (ZrFfiCallbackData *) handleData;
#if ZR_VM_HAS_LIBFFI
            if (callbackData->closure != ZR_NULL) {
                ffi_closure_free(callbackData->closure);
            }
#endif
            zr_ffi_destroy_signature(callbackData->signature);
            free(callbackData);
            break;
        }
        case ZR_FFI_HANDLE_POINTER: {
            ZrFfiPointerData *pointerData = (ZrFfiPointerData *) handleData;
            zr_ffi_pointer_release_owner(state, object);
            zr_ffi_destroy_type(pointerData->type);
            free(pointerData);
            break;
        }
        case ZR_FFI_HANDLE_BUFFER: {
            ZrFfiBufferData *bufferData = (ZrFfiBufferData *) handleData;
            free(bufferData->bytes);
            free(bufferData);
            break;
        }
        default:
            break;
    }
}

#if ZR_VM_HAS_LIBFFI
void zr_ffi_callback_trampoline(ffi_cif *cif, void *returnValue, void **arguments, void *userData) {
    ZrFfiCallbackData *callbackData = (ZrFfiCallbackData *) userData;
    const SZrTypeValue *callbackValue;
    SZrTypeValue *argumentValues = ZR_NULL;
    SZrTypeValue callResult;
    ZrFfiCallbackInvokeArgs invokeArgs;
    SZrCallInfo *savedCallInfo = ZR_NULL;
    TZrStackValuePointer savedStackTop;
    EZrThreadStatus callbackStatus = ZR_THREAD_STATUS_FINE;
    TZrSize index;

    ZR_UNUSED_PARAMETER(cif);

    if (callbackData == ZR_NULL || callbackData->state == ZR_NULL || callbackData->ownerObject == ZR_NULL) {
        return;
    }

    if (callbackData->closed) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage, sizeof(callbackData->lastErrorMessage), "callback handle is closed");
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

#if defined(ZR_PLATFORM_WIN)
    if (GetCurrentThreadId() != callbackData->ownerThreadId) {
#else
    if (!pthread_equal(pthread_self(), callbackData->ownerThreadId)) {
#endif
        callbackData->lastError = ZR_FFI_ERROR_CALLBACK_THREAD;
        snprintf(callbackData->lastErrorMessage, sizeof(callbackData->lastErrorMessage),
                 "callback invoked from a foreign thread");
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

    callbackValue = zr_ffi_find_field_raw(callbackData->state, callbackData->ownerObject, ZR_FFI_HIDDEN_CALLBACK_FIELD);
    if (callbackValue == ZR_NULL) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage, sizeof(callbackData->lastErrorMessage), "callback root is missing");
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }
    if (callbackValue->type != ZR_VALUE_TYPE_FUNCTION &&
        callbackValue->type != ZR_VALUE_TYPE_CLOSURE &&
        callbackValue->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage,
                 sizeof(callbackData->lastErrorMessage),
                 "callback root is not callable (type=%d)",
                 (int)callbackValue->type);
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

    if (callbackData->signature->parameterCount > 0) {
        argumentValues = (SZrTypeValue *) calloc(callbackData->signature->parameterCount, sizeof(SZrTypeValue));
    }
    if (callbackData->signature->parameterCount > 0 && argumentValues == ZR_NULL) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage, sizeof(callbackData->lastErrorMessage),
                 "callback argument allocation failed");
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

    for (index = 0; index < callbackData->signature->parameterCount; index++) {
        if (!zr_ffi_set_result_from_scalar(callbackData->state, callbackData->signature->parameters[index].type,
                                           arguments[index], &argumentValues[index])) {
            callbackData->lastError = ZR_FFI_ERROR_MARSHAL;
            snprintf(callbackData->lastErrorMessage, sizeof(callbackData->lastErrorMessage),
                     "failed to marshal callback argument %llu", (unsigned long long) (index + 1));
            free(argumentValues);
            zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
            return;
        }
    }

    savedCallInfo = callbackData->state->callInfoList;
    savedStackTop = callbackData->state->stackTop.valuePointer;
    invokeArgs.callbackValue = callbackValue;
    invokeArgs.argumentValues = argumentValues;
    invokeArgs.argumentCount = callbackData->signature->parameterCount;
    invokeArgs.result = &callResult;
    invokeArgs.succeeded = ZR_FALSE;
    callbackStatus = ZrCore_Exception_TryRun(callbackData->state, zr_ffi_callback_try_invoke, &invokeArgs);
    if (callbackStatus != ZR_THREAD_STATUS_FINE || !invokeArgs.succeeded) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage,
                 sizeof(callbackData->lastErrorMessage),
                 "zr callback execution failed%s",
                 callbackStatus != ZR_THREAD_STATUS_FINE ? " with VM exception" : "");
        free(argumentValues);
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }
    if (callbackData->state->callInfoList != savedCallInfo ||
        callbackData->state->stackTop.valuePointer != savedStackTop) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage,
                 sizeof(callbackData->lastErrorMessage),
                 "zr callback corrupted VM call stack state");
        free(argumentValues);
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

    zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
    if (!zr_ffi_build_scalar_argument(callbackData->state, &callResult, callbackData->signature->returnType,
                                      returnValue, callbackData->lastErrorMessage,
                                      sizeof(callbackData->lastErrorMessage))) {
        callbackData->lastError = ZR_FFI_ERROR_MARSHAL;
        free(argumentValues);
        zr_ffi_zero_call_storage(callbackData->signature->returnType, returnValue);
        return;
    }

    callbackData->lastError = ZR_FFI_ERROR_NONE;
    callbackData->lastErrorMessage[0] = '\0';
    free(argumentValues);
}
#endif
