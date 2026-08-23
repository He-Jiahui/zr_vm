#include "ffi_runtime/ffi_runtime_internal.h"

static void zr_ffi_pointer_set_length_field(
        SZrState *state,
        SZrObject *pointerObject,
        TZrSize byteLength) {
    SZrTypeValue lengthValue;

    if (state == ZR_NULL || pointerObject == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetInt(state, &lengthValue, (TZrInt64)byteLength);
    ZrLib_Object_SetFieldCString(state, pointerObject, "length", &lengthValue);
}

TZrBool ZrFfi_LoadLibrary(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrString *pathString = ZR_NULL;
    const char *pathText = ZR_NULL;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    void *libraryHandle;
    ZrFfiLibraryData *libraryData;
    SZrObject *libraryObject;

    if (!ZrLib_CallContext_ReadString(context, 0, &pathString) || pathString == ZR_NULL) {
        return ZR_FALSE;
    }

    pathText = ZrCore_String_GetNativeString(pathString);
    libraryHandle = zr_ffi_open_dynamic_library(pathText, errorBuffer, sizeof(errorBuffer));
    if (libraryHandle == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_LOAD, "failed to load '%s': %s", pathText, errorBuffer);
        return ZR_FALSE;
    }

    libraryData = (ZrFfiLibraryData *) calloc(1, sizeof(ZrFfiLibraryData));
    if (libraryData == ZR_NULL) {
        zr_ffi_close_dynamic_library(libraryHandle);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_LOAD, "out of memory while creating LibraryHandle");
        return ZR_FALSE;
    }
    libraryData->base.kind = ZR_FFI_HANDLE_LIBRARY;
    libraryData->libraryHandle = libraryHandle;
    libraryData->libraryPath = zr_ffi_strdup(pathText);

    libraryObject = zr_ffi_new_handle_object_with_finalizer(context->state, "LibraryHandle", &libraryData->base,
                                                            ZR_NULL, ZR_NULL);
    if (libraryObject == ZR_NULL) {
        zr_ffi_close_dynamic_library(libraryHandle);
        free(libraryData->libraryPath);
        free(libraryData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_LOAD, "failed to instantiate LibraryHandle");
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, libraryObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_SizeOf(ZrLibCallContext *context, SZrTypeValue *result) {
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    ZrFfiTypeLayout *type = zr_ffi_parse_type_descriptor(context->state, ZrLib_CallContext_Argument(context, 0),
                                                         errorBuffer, sizeof(errorBuffer));
    if (type == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, (TZrInt64) type->size);
    zr_ffi_destroy_type(type);
    return ZR_TRUE;
}

TZrBool ZrFfi_AlignOf(ZrLibCallContext *context, SZrTypeValue *result) {
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    ZrFfiTypeLayout *type = zr_ffi_parse_type_descriptor(context->state, ZrLib_CallContext_Argument(context, 0),
                                                         errorBuffer, sizeof(errorBuffer));
    if (type == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    ZrLib_Value_SetInt(context->state, result, (TZrInt64) type->align);
    zr_ffi_destroy_type(type);
    return ZR_TRUE;
}

TZrBool ZrFfi_Library_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY) {
        return ZR_FALSE;
    }
    libraryData->closeRequested = ZR_TRUE;
    if (libraryData->openSymbolCount == 0 && libraryData->libraryHandle != ZR_NULL) {
        zr_ffi_close_dynamic_library(libraryData->libraryHandle);
        libraryData->libraryHandle = ZR_NULL;
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrFfi_Library_IsClosed(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetBool(context->state, result, libraryData->closeRequested || libraryData->libraryHandle == ZR_NULL);
    return ZR_TRUE;
}

TZrBool ZrFfi_Library_GetVersion(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(context->state, selfObject);
    const char *symbolName = "zr_ffi_version_string";
    SZrString *symbolNameString = ZR_NULL;
    const char *(*versionProc)(void);
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

    if (selfObject == ZR_NULL || libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 0) {
        if (!ZrLib_CallContext_ReadString(context, 0, &symbolNameString) || symbolNameString == ZR_NULL) {
            return ZR_FALSE;
        }
        symbolName = ZrCore_String_GetNativeString(symbolNameString);
    }
    if (libraryData->libraryHandle == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }
    {
        void *symbolPointer =
                zr_ffi_lookup_symbol(libraryData->libraryHandle, symbolName, errorBuffer, sizeof(errorBuffer));
        versionProc = ZR_NULL;
        if (symbolPointer != ZR_NULL) {
            memcpy(&versionProc, &symbolPointer, sizeof(versionProc));
        }
    }
    if (versionProc == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }
    ZrLib_Value_SetString(context->state, result, versionProc());
    return ZR_TRUE;
}

TZrBool ZrFfi_Callback_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiCallbackData *callbackData = (ZrFfiCallbackData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || callbackData == ZR_NULL || callbackData->base.kind != ZR_FFI_HANDLE_CALLBACK) {
        return ZR_FALSE;
    }
    callbackData->closed = ZR_TRUE;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrFfi_CreateCallback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *signatureObject = ZR_NULL;
    SZrTypeValue *callbackValue = ZR_NULL;
    ZrFfiSignature *signature;
    ZrFfiCallbackData *callbackData;
    SZrObject *callbackObject;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

    if (!ZrLib_CallContext_ReadObject(context, 0, &signatureObject) ||
        !ZrLib_CallContext_ReadFunction(context, 1, &callbackValue) || signatureObject == ZR_NULL ||
        callbackValue == ZR_NULL) {
        return ZR_FALSE;
    }

    signature = zr_ffi_parse_signature(context->state, signatureObject, errorBuffer, sizeof(errorBuffer));
    if (signature == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }

#if !ZR_VM_HAS_LIBFFI
    zr_ffi_destroy_signature(signature);
    zr_ffi_raise_error(context->state, ZR_FFI_ERROR_ABI_MISMATCH, "this build does not include libffi");
    return ZR_FALSE;
#else
    callbackData = (ZrFfiCallbackData *) calloc(1, sizeof(ZrFfiCallbackData));
    if (callbackData == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while creating CallbackHandle");
        return ZR_FALSE;
    }
    callbackData->base.kind = ZR_FFI_HANDLE_CALLBACK;
    callbackData->state = context->state;
    callbackData->signature = signature;
#if defined(ZR_PLATFORM_WIN)
    callbackData->ownerThreadId = GetCurrentThreadId();
#else
    callbackData->ownerThreadId = pthread_self();
#endif
    callbackData->closure = ffi_closure_alloc(sizeof(ffi_closure), &callbackData->codePointer);
    if (callbackData->closure == ZR_NULL || !signature->cifPrepared ||
        ffi_prep_closure_loc(callbackData->closure, &signature->cif, zr_ffi_callback_trampoline, callbackData,
                             callbackData->codePointer) != FFI_OK) {
        if (callbackData->closure != ZR_NULL) {
            ffi_closure_free(callbackData->closure);
        }
        zr_ffi_destroy_signature(signature);
        free(callbackData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_ABI_MISMATCH, "ffi callback trampoline creation failed");
        return ZR_FALSE;
    }
    callbackObject = zr_ffi_new_handle_object_with_finalizer(context->state, "CallbackHandle", &callbackData->base,
                                                             ZR_NULL, callbackValue);
    if (callbackObject == ZR_NULL) {
        ffi_closure_free(callbackData->closure);
        zr_ffi_destroy_signature(signature);
        free(callbackData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate CallbackHandle");
        return ZR_FALSE;
    }
    callbackData->ownerObject = callbackObject;
    ZrLib_Value_SetObject(context->state, result, callbackObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
#endif
}

TZrBool ZrFfi_NullPointer(ZrLibCallContext *context, SZrTypeValue *result) {
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    ZrFfiTypeLayout *type = zr_ffi_parse_type_descriptor(context->state, ZrLib_CallContext_Argument(context, 0),
                                                         errorBuffer, sizeof(errorBuffer));
    ZrFfiPointerData *pointerData;
    SZrObject *pointerObject;

    if (type == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    if (type->kind != ZR_FFI_TYPE_POINTER) {
        ZrFfiTypeLayout *wrapped = zr_ffi_pointer_type_from_target(type);
        zr_ffi_destroy_type(type);
        type = wrapped;
    }
    if (type == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to wrap pointee type into pointer");
        return ZR_FALSE;
    }

    pointerData = (ZrFfiPointerData *) calloc(1, sizeof(ZrFfiPointerData));
    if (pointerData == ZR_NULL) {
        zr_ffi_destroy_type(type);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while creating PointerHandle");
        return ZR_FALSE;
    }
    pointerData->base.kind = ZR_FFI_HANDLE_POINTER;
    pointerData->type = type;

    pointerObject = zr_ffi_new_handle_object_with_finalizer(context->state, "PointerHandle", &pointerData->base,
                                                            ZR_NULL, ZR_NULL);
    if (pointerObject == ZR_NULL) {
        zr_ffi_destroy_type(type);
        free(pointerData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate PointerHandle");
        return ZR_FALSE;
    }
    zr_ffi_pointer_set_length_field(context->state, pointerObject, 0u);
    ZrLib_Value_SetObject(context->state, result, pointerObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrBool zr_ffi_library_create_symbol(
        ZrLibCallContext *context,
        SZrObject *selfObject,
        ZrFfiLibraryData *libraryData,
        const char *symbolName,
        ZrFfiSignature *signature,
        SZrTypeValue *result) {
    ZrFfiSymbolData *symbolData;
    SZrObject *symbolObject;
    SZrTypeValue ownerValue;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    void *symbolAddress;

    if (context == ZR_NULL || selfObject == ZR_NULL || libraryData == ZR_NULL ||
        libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY ||
        symbolName == ZR_NULL || signature == ZR_NULL || result == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        return ZR_FALSE;
    }
    if (libraryData->closeRequested || libraryData->libraryHandle == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_LOAD, "library handle is closed");
        return ZR_FALSE;
    }

    symbolAddress = zr_ffi_lookup_symbol(libraryData->libraryHandle, symbolName, errorBuffer, sizeof(errorBuffer));
    if (symbolAddress == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_SYMBOL, "failed to resolve '%s': %s", symbolName, errorBuffer);
        return ZR_FALSE;
    }

    symbolData = (ZrFfiSymbolData *) calloc(1, sizeof(ZrFfiSymbolData));
    if (symbolData == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_SYMBOL, "out of memory while creating SymbolHandle");
        return ZR_FALSE;
    }
    symbolData->base.kind = ZR_FFI_HANDLE_SYMBOL;
    symbolData->symbolAddress = symbolAddress;
    symbolData->symbolName = zr_ffi_strdup(symbolName);
    symbolData->signature = signature;
    libraryData->openSymbolCount++;

    ZrLib_Value_SetObject(context->state, &ownerValue, selfObject, ZR_VALUE_TYPE_OBJECT);
    symbolObject = zr_ffi_new_handle_object_with_finalizer(context->state, "SymbolHandle", &symbolData->base,
                                                           &ownerValue, ZR_NULL);
    if (symbolObject == ZR_NULL) {
        libraryData->openSymbolCount--;
        zr_ffi_destroy_signature(signature);
        free(symbolData->symbolName);
        free(symbolData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_SYMBOL, "failed to instantiate SymbolHandle");
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, symbolObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Library_GetSymbol(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(context->state, selfObject);
    SZrString *symbolNameString = ZR_NULL;
    SZrObject *signatureObject = ZR_NULL;
    ZrFfiSignature *signature;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

    if (selfObject == ZR_NULL || libraryData == ZR_NULL ||
        libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadString(context, 0, &symbolNameString) ||
        !ZrLib_CallContext_ReadObject(context, 1, &signatureObject) ||
        signatureObject == ZR_NULL) {
        return ZR_FALSE;
    }
    signature = zr_ffi_parse_signature(
            context->state, signatureObject, errorBuffer, sizeof(errorBuffer));
    if (signature == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    return zr_ffi_library_create_symbol(
            context,
            selfObject,
            libraryData,
            ZrCore_String_GetNativeString(symbolNameString),
            signature,
            result);
}

TZrBool ZrFfi_Library_GetContractSymbol(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *)
            zr_ffi_get_handle_data(context->state, selfObject);
    const SZrNativeImportContract *contract = ZR_NULL;
    SZrCallInfo *callInfo;
    ZrFfiSignature *signature;
    TZrInt64 contractIndex = -1;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

    if (selfObject == ZR_NULL || libraryData == ZR_NULL ||
        libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY ||
        !ZrLib_CallContext_ReadInt(context, 0, &contractIndex) ||
        contractIndex < 0) {
        return ZR_FALSE;
    }
    for (callInfo = context->state->callInfoList;
         callInfo != ZR_NULL;
         callInfo = callInfo->previous) {
        const SZrFunction *candidate =
                ZrCore_Closure_GetMetadataFunctionFromCallInfo(
                        context->state, callInfo);

        contract = candidate != ZR_NULL
                ? ZrLibrary_AotRuntime_FindNativeImportContract(
                          context->state,
                          candidate,
                          (TZrUInt32)contractIndex)
                : ZR_NULL;
        if (contract == ZR_NULL && candidate != ZR_NULL &&
            candidate->nativeImportContracts != ZR_NULL &&
            (TZrUInt64)contractIndex < candidate->nativeImportContractLength) {
            contract = &candidate->nativeImportContracts[contractIndex];
        }
        if (contract != ZR_NULL && libraryData->libraryPath != ZR_NULL &&
            strcmp(libraryData->libraryPath, contract->libraryLocator) == 0) {
            break;
        }
        contract = ZR_NULL;
    }
    if (contract == ZR_NULL) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_ABI_MISMATCH,
                "native import contract index is not retained by an active caller");
        return ZR_FALSE;
    }
    if (libraryData->libraryPath == ZR_NULL ||
        strcmp(libraryData->libraryPath, contract->libraryLocator) != 0) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_ABI_MISMATCH,
                "native import contract library does not match the loaded handle");
        return ZR_FALSE;
    }
    signature = zr_ffi_signature_from_contract(
            contract, errorBuffer, sizeof(errorBuffer));
    if (signature == ZR_NULL) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_ABI_MISMATCH,
                "%s (%s:%d:%d)",
                errorBuffer,
                contract->sourceMapping.document,
                (int)contract->sourceMapping.startLine,
                (int)contract->sourceMapping.startColumn);
        return ZR_FALSE;
    }
    return zr_ffi_library_create_symbol(
            context,
            selfObject,
            libraryData,
            contract->entryPoint,
            signature,
            result);
}

TZrBool ZrFfi_Pointer_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || pointerData == ZR_NULL || pointerData->base.kind != ZR_FFI_HANDLE_POINTER) {
        return ZR_FALSE;
    }
    if (!pointerData->closed) {
        pointerData->closed = ZR_TRUE;
        pointerData->address = ZR_NULL;
        pointerData->byteLength = 0u;
        zr_ffi_pointer_set_length_field(context->state, selfObject, 0u);
        zr_ffi_pointer_release_owner(context->state, selfObject);
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrFfi_Pointer_As(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(context->state, selfObject);
    ZrFfiTypeLayout *newType;
    ZrFfiPointerData *newPointerData;
    SZrObject *pointerObject;
    const SZrTypeValue *ownerValue;
    ZrFfiBufferData *ownerBufferData = ZR_NULL;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

    if (selfObject == ZR_NULL || pointerData == ZR_NULL || pointerData->base.kind != ZR_FFI_HANDLE_POINTER) {
        return ZR_FALSE;
    }
    newType = zr_ffi_parse_type_descriptor(context->state, ZrLib_CallContext_Argument(context, 0), errorBuffer,
                                           sizeof(errorBuffer));
    if (newType == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    if (newType->kind != ZR_FFI_TYPE_POINTER) {
        ZrFfiTypeLayout *wrapped = zr_ffi_pointer_type_from_target(newType);
        zr_ffi_destroy_type(newType);
        newType = wrapped;
    }
    if (newType == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to wrap pointer target");
        return ZR_FALSE;
    }
    newPointerData = (ZrFfiPointerData *) calloc(1, sizeof(ZrFfiPointerData));
    if (newPointerData == ZR_NULL) {
        zr_ffi_destroy_type(newType);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while creating PointerHandle");
        return ZR_FALSE;
    }
    newPointerData->base.kind = ZR_FFI_HANDLE_POINTER;
    newPointerData->address = pointerData->address;
    newPointerData->byteLength = pointerData->byteLength;
    newPointerData->type = newType;
    ownerValue = zr_ffi_find_field_raw(context->state, selfObject, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue != ZR_NULL && ownerValue->type == ZR_VALUE_TYPE_OBJECT && ownerValue->value.object != ZR_NULL) {
        SZrObject *ownerObject = ZR_CAST_OBJECT(context->state, ownerValue->value.object);
        ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, ownerObject);
        if (bufferData != ZR_NULL && bufferData->base.kind == ZR_FFI_HANDLE_BUFFER) {
            bufferData->pinCount++;
            ownerBufferData = bufferData;
        }
    }
    pointerObject = zr_ffi_new_handle_object_with_finalizer(context->state, "PointerHandle", &newPointerData->base,
                                                            ownerValue, ZR_NULL);
    if (pointerObject == ZR_NULL) {
        if (ownerBufferData != ZR_NULL && ownerBufferData->pinCount > 0u) {
            ownerBufferData->pinCount--;
        }
        zr_ffi_destroy_type(newType);
        free(newPointerData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate PointerHandle");
        return ZR_FALSE;
    }
    zr_ffi_pointer_set_length_field(
            context->state, pointerObject, newPointerData->byteLength);
    ZrLib_Value_SetObject(context->state, result, pointerObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Pointer_Read(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(context->state, selfObject);
    ZrFfiTypeLayout *type;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    if (selfObject == ZR_NULL || pointerData == ZR_NULL || pointerData->base.kind != ZR_FFI_HANDLE_POINTER) {
        return ZR_FALSE;
    }
    if (pointerData->closed || pointerData->address == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_NATIVE_CALL, "pointer handle is null or closed");
        return ZR_FALSE;
    }
    type = zr_ffi_parse_type_descriptor(context->state, ZrLib_CallContext_Argument(context, 0), errorBuffer,
                                        sizeof(errorBuffer));
    if (type == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
        return ZR_FALSE;
    }
    if (!zr_ffi_set_result_from_scalar(context->state, type, pointerData->address, result)) {
        zr_ffi_destroy_type(type);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to read through PointerHandle");
        return ZR_FALSE;
    }
    zr_ffi_destroy_type(type);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Allocate(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 requestedSize = 0;
    ZrFfiBufferData *bufferData;
    SZrObject *bufferObject;
    if (!ZrLib_CallContext_ReadInt(context, 0, &requestedSize)) {
        return ZR_FALSE;
    }
    if (requestedSize < 0) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "buffer size must be non-negative");
        return ZR_FALSE;
    }
    bufferData = (ZrFfiBufferData *) calloc(1, sizeof(ZrFfiBufferData));
    if (bufferData == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while creating BufferHandle");
        return ZR_FALSE;
    }
    bufferData->base.kind = ZR_FFI_HANDLE_BUFFER;
    bufferData->size = (TZrSize) requestedSize;
    if (bufferData->size > 0) {
        bufferData->bytes = (unsigned char *) calloc(bufferData->size, 1);
    }
    if (bufferData->size > 0 && bufferData->bytes == ZR_NULL) {
        free(bufferData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "native buffer allocation failed");
        return ZR_FALSE;
    }
    bufferObject = zr_ffi_new_handle_object_with_finalizer(context->state, "BufferHandle", &bufferData->base, ZR_NULL,
                                                           ZR_NULL);
    if (bufferObject == ZR_NULL) {
        free(bufferData->bytes);
        free(bufferData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate BufferHandle");
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, bufferObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || bufferData == ZR_NULL || bufferData->base.kind != ZR_FFI_HANDLE_BUFFER) {
        return ZR_FALSE;
    }
    bufferData->closeRequested = ZR_TRUE;
    if (bufferData->pinCount == 0 && bufferData->bytes != ZR_NULL) {
        free(bufferData->bytes);
        bufferData->bytes = ZR_NULL;
        bufferData->size = 0;
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Pin(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, selfObject);
    ZrFfiTypeLayout *u8Type;
    ZrFfiTypeLayout *pointerType;
    ZrFfiPointerData *pointerData;
    SZrTypeValue ownerValue;
    SZrObject *pointerObject;
    if (selfObject == ZR_NULL || bufferData == ZR_NULL || bufferData->base.kind != ZR_FFI_HANDLE_BUFFER) {
        return ZR_FALSE;
    }
    if (bufferData->closeRequested ||
        (bufferData->size > 0u && bufferData->bytes == ZR_NULL)) {
        zr_ffi_raise_error(
                context->state,
                ZR_FFI_ERROR_NATIVE_CALL,
                "cannot pin a closed buffer handle");
        return ZR_FALSE;
    }
    u8Type = zr_ffi_make_primitive_type("u8");
    pointerType = zr_ffi_pointer_type_from_target(u8Type);
    zr_ffi_destroy_type(u8Type);
    if (pointerType == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to build pinned byte pointer type");
        return ZR_FALSE;
    }
    pointerType->as.pointer.direction = ZR_FFI_DIRECTION_INOUT;
    pointerData = (ZrFfiPointerData *) calloc(1, sizeof(ZrFfiPointerData));
    if (pointerData == ZR_NULL) {
        zr_ffi_destroy_type(pointerType);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while creating PointerHandle");
        return ZR_FALSE;
    }
    pointerData->base.kind = ZR_FFI_HANDLE_POINTER;
    pointerData->address = bufferData->bytes;
    pointerData->byteLength = bufferData->size;
    pointerData->type = pointerType;
    bufferData->pinCount++;
    ZrLib_Value_SetObject(context->state, &ownerValue, selfObject, ZR_VALUE_TYPE_OBJECT);
    pointerObject = zr_ffi_new_handle_object_with_finalizer(context->state, "PointerHandle", &pointerData->base,
                                                            &ownerValue, ZR_NULL);
    if (pointerObject == ZR_NULL) {
        bufferData->pinCount--;
        zr_ffi_destroy_type(pointerType);
        free(pointerData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate PointerHandle");
        return ZR_FALSE;
    }
    zr_ffi_pointer_set_length_field(
            context->state, pointerObject, pointerData->byteLength);
    ZrLib_Value_SetObject(context->state, result, pointerObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Read(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, selfObject);
    TZrInt64 offsetValue = 0;
    TZrInt64 lengthValue = 0;
    SZrObject *array;
    ZrLibTempValueRoot arrayRoot;
    TZrSize index;
    if (selfObject == ZR_NULL || bufferData == ZR_NULL || bufferData->base.kind != ZR_FFI_HANDLE_BUFFER) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, 0, &offsetValue) || !ZrLib_CallContext_ReadInt(context, 1, &lengthValue)) {
        return ZR_FALSE;
    }
    if (offsetValue < 0 || lengthValue < 0 || (TZrSize) offsetValue > bufferData->size ||
        (TZrSize) lengthValue > bufferData->size - (TZrSize) offsetValue) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "buffer.read range is out of bounds");
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_BeginTempValueRoot(context, &arrayRoot)) {
        return ZR_FALSE;
    }
    array = ZrLib_Array_New(context->state);
    if (array == ZR_NULL) {
        ZrLib_TempValueRoot_End(&arrayRoot);
        return ZR_FALSE;
    }
    ZrLib_TempValueRoot_SetObject(&arrayRoot, array, ZR_VALUE_TYPE_ARRAY);
    for (index = 0; index < (TZrSize) lengthValue; index++) {
        SZrTypeValue byteValue;
        ZrLib_Value_SetInt(context->state, &byteValue, bufferData->bytes[(TZrSize) offsetValue + index]);
        ZrLib_Array_PushValue(context->state, array, &byteValue);
    }
    ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
    ZrLib_TempValueRoot_End(&arrayRoot);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Write(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, selfObject);
    TZrInt64 offsetValue = 0;
    SZrObject *bytesArray = ZR_NULL;
    TZrSize length;
    TZrSize index;
    if (selfObject == ZR_NULL || bufferData == ZR_NULL || bufferData->base.kind != ZR_FFI_HANDLE_BUFFER) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, 0, &offsetValue) || !ZrLib_CallContext_ReadArray(context, 1, &bytesArray) ||
        bytesArray == ZR_NULL) {
        return ZR_FALSE;
    }
    length = zr_ffi_array_length(context->state, bytesArray);
    if (offsetValue < 0 || (TZrSize) offsetValue > bufferData->size ||
        length > bufferData->size - (TZrSize) offsetValue) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "buffer.write range is out of bounds");
        return ZR_FALSE;
    }
    for (index = 0; index < length; index++) {
        const SZrTypeValue *item = zr_ffi_array_get(context->state, bytesArray, index);
        TZrInt64 intValue = 0;
        if (!zr_ffi_read_int_value(item, &intValue)) {
            zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "buffer.write expects an array of integer bytes");
            return ZR_FALSE;
        }
        bufferData->bytes[(TZrSize) offsetValue + index] = (unsigned char) intValue;
    }
    ZrLib_Value_SetInt(context->state, result, (TZrInt64) length);
    return ZR_TRUE;
}

TZrBool ZrFfi_Buffer_Slice(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, selfObject);
    TZrInt64 offsetValue = 0;
    TZrInt64 lengthValue = 0;
    ZrFfiBufferData *sliceData;
    SZrObject *sliceObject;
    if (selfObject == ZR_NULL || bufferData == ZR_NULL || bufferData->base.kind != ZR_FFI_HANDLE_BUFFER) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, 0, &offsetValue) || !ZrLib_CallContext_ReadInt(context, 1, &lengthValue)) {
        return ZR_FALSE;
    }
    if (offsetValue < 0 || lengthValue < 0 || (TZrSize) offsetValue > bufferData->size ||
        (TZrSize) lengthValue > bufferData->size - (TZrSize) offsetValue) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "buffer.slice range is out of bounds");
        return ZR_FALSE;
    }
    sliceData = (ZrFfiBufferData *) calloc(1, sizeof(ZrFfiBufferData));
    if (sliceData == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while slicing BufferHandle");
        return ZR_FALSE;
    }
    sliceData->base.kind = ZR_FFI_HANDLE_BUFFER;
    sliceData->size = (TZrSize) lengthValue;
    if (sliceData->size > 0) {
        sliceData->bytes = (unsigned char *) malloc(sliceData->size);
    }
    if (sliceData->size > 0 && sliceData->bytes == ZR_NULL) {
        free(sliceData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "native buffer allocation failed");
        return ZR_FALSE;
    }
    if (sliceData->size > 0) {
        memcpy(sliceData->bytes, bufferData->bytes + (TZrSize) offsetValue, sliceData->size);
    }
    sliceObject =
            zr_ffi_new_handle_object_with_finalizer(context->state, "BufferHandle", &sliceData->base, ZR_NULL, ZR_NULL);
    if (sliceObject == ZR_NULL) {
        free(sliceData->bytes);
        free(sliceData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate sliced BufferHandle");
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, sliceObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Symbol_Call(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiSymbolData *symbolData = ZR_NULL;
    SZrObject *argumentsArray = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL || selfObject == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolData = (ZrFfiSymbolData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (!ZrLib_CallContext_ReadArray(context, 0, &argumentsArray) || argumentsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_ffi_symbol_invoke_array(
            context->state,
            selfObject,
            symbolData,
            argumentsArray,
            ZR_NULL,
            result);
}

TZrBool ZrFfi_Symbol_MetaCall(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiSymbolData *symbolData = ZR_NULL;
    SZrObject *argumentsArray;
    ZrLibTempValueRoot arrayRoot;

    if (context == ZR_NULL || result == ZR_NULL || selfObject == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolData = (ZrFfiSymbolData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (!ZrLib_CallContext_BeginTempValueRoot(context, &arrayRoot)) {
        return ZR_FALSE;
    }
    argumentsArray = ZrLib_Array_New(context->state);
    if (argumentsArray == ZR_NULL) {
        ZrLib_TempValueRoot_End(&arrayRoot);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "out of memory while preparing direct symbol call");
        return ZR_FALSE;
    }
    ZrLib_TempValueRoot_SetObject(&arrayRoot, argumentsArray, ZR_VALUE_TYPE_ARRAY);

    for (TZrSize index = 0; index < ZrLib_CallContext_ArgumentCount(context); index++) {
        if (!ZrLib_Array_PushValue(context->state, argumentsArray, ZrLib_CallContext_Argument(context, index))) {
            ZrLib_TempValueRoot_End(&arrayRoot);
            zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to append direct symbol call argument");
            return ZR_FALSE;
        }
    }

    {
        TZrBool succeeded = zr_ffi_symbol_invoke_array(
                context->state,
                selfObject,
                symbolData,
                argumentsArray,
                context,
                result);
        ZrLib_TempValueRoot_End(&arrayRoot);
        return succeeded;
    }
}
