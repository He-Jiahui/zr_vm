#include "ffi_runtime_internal.h"

const char *zr_ffi_error_name(ZrFfiErrorCode code) {
    switch (code) {
        case ZR_FFI_ERROR_LOAD:
            return "LoadError";
        case ZR_FFI_ERROR_SYMBOL:
            return "SymbolError";
        case ZR_FFI_ERROR_ABI_MISMATCH:
            return "AbiMismatch";
        case ZR_FFI_ERROR_MARSHAL:
            return "MarshalError";
        case ZR_FFI_ERROR_VERSION:
            return "VersionError";
        case ZR_FFI_ERROR_CALLBACK_THREAD:
            return "CallbackThreadError";
        case ZR_FFI_ERROR_NATIVE_CALL:
            return "NativeCallError";
        default:
            return "FfiError";
    }
}

void zr_ffi_raise_error(SZrState *state, ZrFfiErrorCode code, const char *format, ...) {
    char message[512];
    va_list arguments;

    if (state == ZR_NULL || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    ZrCore_Debug_RunError(state, "[%s] %s", zr_ffi_error_name(code), message);
}

char *zr_ffi_strdup(const char *text) {
    TZrSize length;
    char *copy;

    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (char *) malloc(length + 1);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

TZrSize zr_ffi_align_up(TZrSize value, TZrSize alignment) {
    TZrSize mask;

    if (alignment <= 1) {
        return value;
    }

    mask = alignment - 1;
    return (value + mask) & ~mask;
}

TZrSize zr_ffi_call_storage_size(const ZrFfiTypeLayout *type) {
    if (type == ZR_NULL) {
        return sizeof(void *);
    }

    switch (type->kind) {
        case ZR_FFI_TYPE_VOID:
            return 0;
        case ZR_FFI_TYPE_BOOL:
        case ZR_FFI_TYPE_I8:
        case ZR_FFI_TYPE_U8:
        case ZR_FFI_TYPE_I16:
        case ZR_FFI_TYPE_U16:
        case ZR_FFI_TYPE_I32:
        case ZR_FFI_TYPE_U32:
        case ZR_FFI_TYPE_I64:
        case ZR_FFI_TYPE_U64:
        case ZR_FFI_TYPE_ENUM:
            return sizeof(ZrFfiAbiUnsignedSlot);
        case ZR_FFI_TYPE_POINTER:
        case ZR_FFI_TYPE_STRING:
        case ZR_FFI_TYPE_FUNCTION:
            return sizeof(void *);
        default:
            return type->size;
    }
}

TZrSize zr_ffi_non_void_call_storage_size(const ZrFfiTypeLayout *type) {
    TZrSize storageSize = zr_ffi_call_storage_size(type);
    return storageSize > 0 ? storageSize : sizeof(void *);
}

void zr_ffi_zero_call_storage(const ZrFfiTypeLayout *type, void *storage) {
    TZrSize storageSize;

    if (storage == ZR_NULL) {
        return;
    }

    storageSize = zr_ffi_non_void_call_storage_size(type);
    memset(storage, 0, storageSize);
}

TZrBool zr_ffi_invoke_native_symbol(ZrFfiSymbolData *symbolData, void *returnStorage, void **ffiArguments,
                                           char *errorBuffer, TZrSize errorBufferSize) {
#if !ZR_VM_HAS_LIBFFI
    ZR_UNUSED_PARAMETER(symbolData);
    ZR_UNUSED_PARAMETER(returnStorage);
    ZR_UNUSED_PARAMETER(ffiArguments);
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        snprintf(errorBuffer, errorBufferSize, "this build does not include libffi");
    }
    return ZR_FALSE;
#else
    if (symbolData == ZR_NULL || symbolData->signature == ZR_NULL || symbolData->symbolAddress == ZR_NULL) {
        if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
            snprintf(errorBuffer, errorBufferSize, "symbol handle is not callable");
        }
        return ZR_FALSE;
    }

#if defined(_MSC_VER) && defined(ZR_PLATFORM_WIN)
    __try {
        ffi_call(&symbolData->signature->cif, FFI_FN(symbolData->symbolAddress), returnStorage, ffiArguments);
        return ZR_TRUE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
            snprintf(errorBuffer,
                     errorBufferSize,
                     "ffi_call for symbol '%s' raised SEH 0x%08lx",
                     symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>",
                     (unsigned long) GetExceptionCode());
        }
        return ZR_FALSE;
    }
#else
    ffi_call(&symbolData->signature->cif, FFI_FN(symbolData->symbolAddress), returnStorage, ffiArguments);
    return ZR_TRUE;
#endif
#endif
}

const SZrTypeValue *zr_ffi_find_field_raw(SZrState *state, SZrObject *object, const char *fieldName) {
    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

TZrBool zr_ffi_read_string_value(SZrState *state, const SZrTypeValue *value, const char **outText) {
    if (state == ZR_NULL || value == ZR_NULL || outText == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
    return *outText != ZR_NULL;
}

TZrBool zr_ffi_read_object_string_field(SZrState *state, SZrObject *object, const char *fieldName,
                                               const char **outText) {
    return zr_ffi_read_string_value(state, zr_ffi_find_field_raw(state, object, fieldName), outText);
}

TZrBool zr_ffi_read_bool_value(const SZrTypeValue *value, TZrBool *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        *outValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64 != 0 ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeUInt64 != 0 ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool zr_ffi_read_object_bool_field(SZrState *state, SZrObject *object, const char *fieldName,
                                             TZrBool *outValue) {
    return zr_ffi_read_bool_value(zr_ffi_find_field_raw(state, object, fieldName), outValue);
}

TZrBool zr_ffi_read_int_value(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64) value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = (TZrInt64) value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool zr_ffi_value_is_object(const SZrTypeValue *value, SZrObject **outObject) {
    if (value == ZR_NULL || outObject == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outObject = ZR_CAST_OBJECT(ZR_NULL, value->value.object);
    return *outObject != ZR_NULL;
}

TZrBool zr_ffi_read_object_int_field(SZrState *state, SZrObject *object, const char *fieldName,
                                            TZrInt64 *outValue) {
    return zr_ffi_read_int_value(zr_ffi_find_field_raw(state, object, fieldName), outValue);
}

TZrBool zr_ffi_read_object_field_object(SZrState *state, SZrObject *object, const char *fieldName,
                                               SZrObject **outObject) {
    const SZrTypeValue *value = zr_ffi_find_field_raw(state, object, fieldName);
    if (value == ZR_NULL || outObject == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    *outObject = ZR_CAST_OBJECT(state, value->value.object);
    return *outObject != ZR_NULL;
}

const SZrTypeValue *zr_ffi_array_get(SZrState *state, SZrObject *array, TZrSize index) {
    return ZrLib_Array_Get(state, array, index);
}

TZrSize zr_ffi_array_length(SZrState *state, SZrObject *array) {
    ZR_UNUSED_PARAMETER(state);
    return ZrLib_Array_Length(array);
}

ZrFfiHandleData *zr_ffi_get_handle_data(SZrState *state, SZrObject *object) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    value = zr_ffi_find_field_raw(state, object, ZR_FFI_HIDDEN_HANDLE_FIELD);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_NULL;
    }

    return (ZrFfiHandleData *) value->value.nativeObject.nativePointer;
}

void zr_ffi_set_hidden_pointer(SZrState *state, SZrObject *object, const char *fieldName, void *pointerValue) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNativePointer(state, &value, pointerValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &value);
}

void zr_ffi_set_hidden_value(SZrState *state, SZrObject *object, const char *fieldName,
                                    const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

SZrObject *zr_ffi_get_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL) {
        return ZR_NULL;
    }

    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        ZrCore_Debug_RunError(context->state, "ffi method called without a valid receiver");
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

SZrObject *zr_ffi_new_handle_object_with_finalizer(SZrState *state, const char *typeName, ZrFfiHandleData *data,
                                                          const SZrTypeValue *ownerValue,
                                                          const SZrTypeValue *callbackValue) {
    SZrObject *object;
    ZrLibTempValueRoot objectRoot;

    if (state == ZR_NULL || typeName == ZR_NULL || data == ZR_NULL) {
        return ZR_NULL;
    }
    if (!ZrLib_TempValueRoot_Begin(state, &objectRoot)) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, typeName);
    if (object == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_NULL;
    }
    ZrLib_TempValueRoot_SetObject(&objectRoot, object, ZR_VALUE_TYPE_OBJECT);

    object->super.scanMarkGcFunction = zr_ffi_handle_finalize;
    zr_ffi_set_hidden_pointer(state, object, ZR_FFI_HIDDEN_HANDLE_FIELD, data);
    if (ownerValue != ZR_NULL) {
        zr_ffi_set_hidden_value(state, object, ZR_FFI_HIDDEN_OWNER_FIELD, ownerValue);
    }
    if (callbackValue != ZR_NULL) {
        zr_ffi_set_hidden_value(state, object, ZR_FFI_HIDDEN_CALLBACK_FIELD, callbackValue);
    }

    ZrLib_TempValueRoot_End(&objectRoot);
    return object;
}

void zr_ffi_close_dynamic_library(void *libraryHandle) {
    if (libraryHandle == ZR_NULL) {
        return;
    }

#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE) libraryHandle);
#else
    dlclose(libraryHandle);
#endif
}

void *zr_ffi_open_dynamic_library(const char *path, char *errorBuffer, TZrSize errorBufferSize) {
#if defined(ZR_PLATFORM_WIN)
    HMODULE module = LoadLibraryA(path);
    if (module == NULL && errorBuffer != ZR_NULL && errorBufferSize > 0) {
        snprintf(errorBuffer, errorBufferSize, "LoadLibraryA failed with code %lu", (unsigned long) GetLastError());
    }
    return (void *) module;
#else
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == ZR_NULL && errorBuffer != ZR_NULL && errorBufferSize > 0) {
        const char *dlError = dlerror();
        snprintf(errorBuffer, errorBufferSize, "%s", dlError != ZR_NULL ? dlError : "dlopen failed");
    }
    return handle;
#endif
}

void *zr_ffi_lookup_symbol(void *libraryHandle, const char *symbolName, char *errorBuffer,
                                  TZrSize errorBufferSize) {
#if defined(ZR_PLATFORM_WIN)
    FARPROC symbol = libraryHandle != ZR_NULL ? GetProcAddress((HMODULE) libraryHandle, symbolName) : NULL;
    if (symbol == NULL && errorBuffer != ZR_NULL && errorBufferSize > 0) {
        snprintf(errorBuffer, errorBufferSize, "GetProcAddress failed with code %lu", (unsigned long) GetLastError());
    }
    return (void *) symbol;
#else
    void *symbol;
    if (libraryHandle == ZR_NULL) {
        return ZR_NULL;
    }
    dlerror();
    symbol = dlsym(libraryHandle, symbolName);
    if (symbol == ZR_NULL && errorBuffer != ZR_NULL && errorBufferSize > 0) {
        const char *dlError = dlerror();
        snprintf(errorBuffer, errorBufferSize, "%s", dlError != ZR_NULL ? dlError : "dlsym failed");
    }
    return symbol;
#endif
}
