//
// zr.ffi runtime implementation.
//

#include "zr_vm_lib_ffi/runtime.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(ZR_VM_HAS_LIBFFI)
#define ZR_VM_HAS_LIBFFI 0
#endif

#if defined(ZR_PLATFORM_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

#if ZR_VM_HAS_LIBFFI
#include <ffi.h>
#else
typedef int ffi_abi;
typedef struct ffi_type ffi_type;
typedef struct ffi_cif ffi_cif;
typedef struct ffi_closure ffi_closure;
#endif

#if ZR_VM_HAS_LIBFFI
typedef ffi_arg ZrFfiAbiUnsignedSlot;
typedef ffi_sarg ZrFfiAbiSignedSlot;
#else
typedef uintptr_t ZrFfiAbiUnsignedSlot;
typedef intptr_t ZrFfiAbiSignedSlot;
#endif

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

#define ZR_FFI_HIDDEN_HANDLE_FIELD "__zr_ffi_handle"
#define ZR_FFI_HIDDEN_OWNER_FIELD "__zr_ffi_owner"
#define ZR_FFI_HIDDEN_CALLBACK_FIELD "__zr_ffi_callback"

typedef enum ZrFfiErrorCode {
    ZR_FFI_ERROR_NONE = 0,
    ZR_FFI_ERROR_LOAD,
    ZR_FFI_ERROR_SYMBOL,
    ZR_FFI_ERROR_ABI_MISMATCH,
    ZR_FFI_ERROR_MARSHAL,
    ZR_FFI_ERROR_VERSION,
    ZR_FFI_ERROR_CALLBACK_THREAD,
    ZR_FFI_ERROR_NATIVE_CALL
} ZrFfiErrorCode;

typedef enum ZrFfiTypeKind {
    ZR_FFI_TYPE_VOID = 0,
    ZR_FFI_TYPE_BOOL,
    ZR_FFI_TYPE_I8,
    ZR_FFI_TYPE_U8,
    ZR_FFI_TYPE_I16,
    ZR_FFI_TYPE_U16,
    ZR_FFI_TYPE_I32,
    ZR_FFI_TYPE_U32,
    ZR_FFI_TYPE_I64,
    ZR_FFI_TYPE_U64,
    ZR_FFI_TYPE_F32,
    ZR_FFI_TYPE_F64,
    ZR_FFI_TYPE_POINTER,
    ZR_FFI_TYPE_STRING,
    ZR_FFI_TYPE_STRUCT,
    ZR_FFI_TYPE_UNION,
    ZR_FFI_TYPE_ENUM,
    ZR_FFI_TYPE_FUNCTION
} ZrFfiTypeKind;

typedef enum ZrFfiDirection { ZR_FFI_DIRECTION_IN = 0, ZR_FFI_DIRECTION_OUT, ZR_FFI_DIRECTION_INOUT } ZrFfiDirection;

typedef enum ZrFfiStringEncoding {
    ZR_FFI_STRING_UTF8 = 0,
    ZR_FFI_STRING_UTF16,
    ZR_FFI_STRING_ANSI
} ZrFfiStringEncoding;

typedef enum ZrFfiHandleKind {
    ZR_FFI_HANDLE_LIBRARY = 0,
    ZR_FFI_HANDLE_SYMBOL,
    ZR_FFI_HANDLE_CALLBACK,
    ZR_FFI_HANDLE_POINTER,
    ZR_FFI_HANDLE_BUFFER
} ZrFfiHandleKind;

typedef struct ZrFfiTypeLayout ZrFfiTypeLayout;
typedef struct ZrFfiSignature ZrFfiSignature;

typedef struct ZrFfiFieldLayout {
    char *name;
    ZrFfiTypeLayout *type;
    TZrSize offset;
} ZrFfiFieldLayout;

typedef struct ZrFfiParameter {
    ZrFfiTypeLayout *type;
} ZrFfiParameter;

struct ZrFfiSignature {
    ffi_abi abi;
    TZrBool isVarargs;
    TZrSize parameterCount;
    ZrFfiParameter *parameters;
    ZrFfiTypeLayout *returnType;
#if ZR_VM_HAS_LIBFFI
    ffi_cif cif;
    ffi_type **ffiParameterTypes;
    TZrBool cifPrepared;
#endif
};

struct ZrFfiTypeLayout {
    ZrFfiTypeKind kind;
    TZrSize size;
    TZrSize align;
    char *name;
#if ZR_VM_HAS_LIBFFI
    ffi_type *ffiType;
    ffi_type ffiAggregateType;
    ffi_type **ffiElements;
#endif
    union {
        struct {
            ZrFfiTypeLayout *pointee;
            ZrFfiDirection direction;
        } pointer;
        struct {
            ZrFfiStringEncoding encoding;
        } stringType;
        struct {
            TZrSize fieldCount;
            ZrFfiFieldLayout *fields;
        } aggregate;
        struct {
            ZrFfiTypeLayout *underlying;
        } enumType;
        struct {
            ZrFfiSignature *signature;
        } functionType;
    } as;
};

typedef struct ZrFfiHandleData {
    ZrFfiHandleKind kind;
    TZrBool finalized;
} ZrFfiHandleData;

typedef struct ZrFfiLibraryData {
    ZrFfiHandleData base;
    void *libraryHandle;
    char *libraryPath;
    TZrBool closeRequested;
    TZrSize openSymbolCount;
} ZrFfiLibraryData;

typedef struct ZrFfiSymbolData {
    ZrFfiHandleData base;
    void *symbolAddress;
    char *symbolName;
    ZrFfiSignature *signature;
    TZrBool closed;
} ZrFfiSymbolData;

typedef struct ZrFfiCallbackData {
    ZrFfiHandleData base;
    SZrState *state;
    SZrObject *ownerObject;
#if defined(ZR_PLATFORM_WIN)
    DWORD ownerThreadId;
#else
    pthread_t ownerThreadId;
#endif
    ZrFfiSignature *signature;
    TZrBool closed;
    ZrFfiErrorCode lastError;
    char lastErrorMessage[256];
#if ZR_VM_HAS_LIBFFI
    ffi_closure *closure;
#endif
    void *codePointer;
} ZrFfiCallbackData;

typedef struct ZrFfiPointerData {
    ZrFfiHandleData base;
    unsigned char *address;
    ZrFfiTypeLayout *type;
    TZrBool closed;
} ZrFfiPointerData;

typedef struct ZrFfiBufferData {
    ZrFfiHandleData base;
    unsigned char *bytes;
    TZrSize size;
    TZrBool closeRequested;
    TZrSize pinCount;
} ZrFfiBufferData;

typedef struct ZrFfiMarshalledValue {
    void *argumentPointer;
    void *ownedAllocation;
} ZrFfiMarshalledValue;

typedef struct ZrFfiCallbackInvokeArgs {
    const SZrTypeValue *callbackValue;
    SZrTypeValue *argumentValues;
    TZrSize argumentCount;
    SZrTypeValue *result;
    TZrBool succeeded;
} ZrFfiCallbackInvokeArgs;

static void zr_ffi_destroy_type(ZrFfiTypeLayout *type);
static void zr_ffi_destroy_signature(ZrFfiSignature *signature);
static ZrFfiTypeLayout *zr_ffi_clone_type(const ZrFfiTypeLayout *type);
static ZrFfiTypeLayout *zr_ffi_parse_type_descriptor(SZrState *state, const SZrTypeValue *descriptorValue,
                                                     char *errorBuffer, TZrSize errorBufferSize);
static ZrFfiSignature *zr_ffi_parse_signature(SZrState *state, SZrObject *signatureObject, char *errorBuffer,
                                              TZrSize errorBufferSize);
static void zr_ffi_handle_finalize(SZrState *state, SZrRawObject *rawObject);
static TZrBool zr_ffi_set_result_from_scalar(SZrState *state, ZrFfiTypeLayout *type, const void *value,
                                             SZrTypeValue *result);
static TZrBool zr_ffi_build_scalar_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            void *buffer, char *errorBuffer, TZrSize errorBufferSize);
static void zr_ffi_callback_try_invoke(SZrState *state, TZrPtr arguments);

static const char *zr_ffi_error_name(ZrFfiErrorCode code) {
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

static void zr_ffi_raise_error(SZrState *state, ZrFfiErrorCode code, const char *format, ...) {
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

static char *zr_ffi_strdup(const char *text) {
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

static TZrSize zr_ffi_align_up(TZrSize value, TZrSize alignment) {
    TZrSize mask;

    if (alignment <= 1) {
        return value;
    }

    mask = alignment - 1;
    return (value + mask) & ~mask;
}

static TZrSize zr_ffi_call_storage_size(const ZrFfiTypeLayout *type) {
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

static TZrSize zr_ffi_non_void_call_storage_size(const ZrFfiTypeLayout *type) {
    TZrSize storageSize = zr_ffi_call_storage_size(type);
    return storageSize > 0 ? storageSize : sizeof(void *);
}

static void zr_ffi_zero_call_storage(const ZrFfiTypeLayout *type, void *storage) {
    TZrSize storageSize;

    if (storage == ZR_NULL) {
        return;
    }

    storageSize = zr_ffi_non_void_call_storage_size(type);
    memset(storage, 0, storageSize);
}

static TZrBool zr_ffi_invoke_native_symbol(ZrFfiSymbolData *symbolData, void *returnStorage, void **ffiArguments,
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

static const SZrTypeValue *zr_ffi_find_field_raw(SZrState *state, SZrObject *object, const char *fieldName) {
    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

static TZrBool zr_ffi_read_string_value(SZrState *state, const SZrTypeValue *value, const char **outText) {
    if (state == ZR_NULL || value == ZR_NULL || outText == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
    return *outText != ZR_NULL;
}

static TZrBool zr_ffi_read_object_string_field(SZrState *state, SZrObject *object, const char *fieldName,
                                               const char **outText) {
    return zr_ffi_read_string_value(state, zr_ffi_find_field_raw(state, object, fieldName), outText);
}

static TZrBool zr_ffi_read_bool_value(const SZrTypeValue *value, TZrBool *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        *outValue = value->value.nativeObject.nativeBool;
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

static TZrBool zr_ffi_read_object_bool_field(SZrState *state, SZrObject *object, const char *fieldName,
                                             TZrBool *outValue) {
    return zr_ffi_read_bool_value(zr_ffi_find_field_raw(state, object, fieldName), outValue);
}

static TZrBool zr_ffi_read_int_value(const SZrTypeValue *value, TZrInt64 *outValue) {
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

static TZrBool zr_ffi_value_is_object(const SZrTypeValue *value, SZrObject **outObject) {
    if (value == ZR_NULL || outObject == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outObject = ZR_CAST_OBJECT(ZR_NULL, value->value.object);
    return *outObject != ZR_NULL;
}

static TZrBool zr_ffi_read_object_int_field(SZrState *state, SZrObject *object, const char *fieldName,
                                            TZrInt64 *outValue) {
    return zr_ffi_read_int_value(zr_ffi_find_field_raw(state, object, fieldName), outValue);
}

static TZrBool zr_ffi_read_object_field_object(SZrState *state, SZrObject *object, const char *fieldName,
                                               SZrObject **outObject) {
    const SZrTypeValue *value = zr_ffi_find_field_raw(state, object, fieldName);
    if (value == ZR_NULL || outObject == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    *outObject = ZR_CAST_OBJECT(state, value->value.object);
    return *outObject != ZR_NULL;
}

static const SZrTypeValue *zr_ffi_array_get(SZrState *state, SZrObject *array, TZrSize index) {
    return ZrLib_Array_Get(state, array, index);
}

static TZrSize zr_ffi_array_length(SZrState *state, SZrObject *array) {
    ZR_UNUSED_PARAMETER(state);
    return ZrLib_Array_Length(array);
}

static ZrFfiHandleData *zr_ffi_get_handle_data(SZrState *state, SZrObject *object) {
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

static void zr_ffi_set_hidden_pointer(SZrState *state, SZrObject *object, const char *fieldName, void *pointerValue) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNativePointer(state, &value, pointerValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &value);
}

static void zr_ffi_set_hidden_value(SZrState *state, SZrObject *object, const char *fieldName,
                                    const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static SZrObject *zr_ffi_get_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL) {
        return ZR_NULL;
    }

    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        ZrCore_Debug_RunError(context->state, "ffi method called without a valid receiver");
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static SZrObject *zr_ffi_new_handle_object_with_finalizer(SZrState *state, const char *typeName, ZrFfiHandleData *data,
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

static void zr_ffi_close_dynamic_library(void *libraryHandle) {
    if (libraryHandle == ZR_NULL) {
        return;
    }

#if defined(ZR_PLATFORM_WIN)
    FreeLibrary((HMODULE) libraryHandle);
#else
    dlclose(libraryHandle);
#endif
}

static void *zr_ffi_open_dynamic_library(const char *path, char *errorBuffer, TZrSize errorBufferSize) {
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

static void *zr_ffi_lookup_symbol(void *libraryHandle, const char *symbolName, char *errorBuffer,
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

static void zr_ffi_destroy_type(ZrFfiTypeLayout *type) {
    TZrSize index;

    if (type == ZR_NULL) {
        return;
    }

    free(type->name);
    switch (type->kind) {
        case ZR_FFI_TYPE_POINTER:
            zr_ffi_destroy_type(type->as.pointer.pointee);
            break;
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            for (index = 0; index < type->as.aggregate.fieldCount; index++) {
                free(type->as.aggregate.fields[index].name);
                zr_ffi_destroy_type(type->as.aggregate.fields[index].type);
            }
            free(type->as.aggregate.fields);
            break;
        case ZR_FFI_TYPE_ENUM:
            zr_ffi_destroy_type(type->as.enumType.underlying);
            break;
        case ZR_FFI_TYPE_FUNCTION:
            zr_ffi_destroy_signature(type->as.functionType.signature);
            break;
        default:
            break;
    }
#if ZR_VM_HAS_LIBFFI
    free(type->ffiElements);
#endif
    free(type);
}

static void zr_ffi_destroy_signature(ZrFfiSignature *signature) {
    TZrSize index;

    if (signature == ZR_NULL) {
        return;
    }

    for (index = 0; index < signature->parameterCount; index++) {
        zr_ffi_destroy_type(signature->parameters[index].type);
    }
    free(signature->parameters);
    zr_ffi_destroy_type(signature->returnType);
#if ZR_VM_HAS_LIBFFI
    free(signature->ffiParameterTypes);
#endif
    free(signature);
}

static ZrFfiTypeLayout *zr_ffi_new_type(ZrFfiTypeKind kind) {
    ZrFfiTypeLayout *type = (ZrFfiTypeLayout *) calloc(1, sizeof(ZrFfiTypeLayout));
    if (type != ZR_NULL) {
        type->kind = kind;
    }
    return type;
}

static ZrFfiTypeLayout *zr_ffi_clone_type(const ZrFfiTypeLayout *type) {
    TZrSize index;
    ZrFfiTypeLayout *copy;

    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    copy = zr_ffi_new_type(type->kind);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    copy->size = type->size;
    copy->align = type->align;
    copy->name = zr_ffi_strdup(type->name);
#if ZR_VM_HAS_LIBFFI
    copy->ffiType = type->ffiType;
    copy->ffiAggregateType = type->ffiAggregateType;
#endif

    switch (type->kind) {
        case ZR_FFI_TYPE_POINTER:
            copy->as.pointer.direction = type->as.pointer.direction;
            copy->as.pointer.pointee = zr_ffi_clone_type(type->as.pointer.pointee);
            break;
        case ZR_FFI_TYPE_STRING:
            copy->as.stringType.encoding = type->as.stringType.encoding;
            break;
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            copy->as.aggregate.fieldCount = type->as.aggregate.fieldCount;
            if (copy->as.aggregate.fieldCount > 0) {
                copy->as.aggregate.fields =
                        (ZrFfiFieldLayout *) calloc(copy->as.aggregate.fieldCount, sizeof(ZrFfiFieldLayout));
            }
            if (copy->as.aggregate.fieldCount > 0 && copy->as.aggregate.fields == ZR_NULL) {
                zr_ffi_destroy_type(copy);
                return ZR_NULL;
            }
            for (index = 0; index < copy->as.aggregate.fieldCount; index++) {
                copy->as.aggregate.fields[index].name = zr_ffi_strdup(type->as.aggregate.fields[index].name);
                copy->as.aggregate.fields[index].offset = type->as.aggregate.fields[index].offset;
                copy->as.aggregate.fields[index].type = zr_ffi_clone_type(type->as.aggregate.fields[index].type);
            }
#if ZR_VM_HAS_LIBFFI
            if (copy->as.aggregate.fieldCount > 0) {
                copy->ffiElements = (ffi_type **) calloc(copy->as.aggregate.fieldCount + 1, sizeof(ffi_type *));
                if (copy->ffiElements == ZR_NULL) {
                    zr_ffi_destroy_type(copy);
                    return ZR_NULL;
                }
                for (index = 0; index < copy->as.aggregate.fieldCount; index++) {
                    copy->ffiElements[index] = copy->as.aggregate.fields[index].type->ffiType;
                }
                copy->ffiAggregateType.elements = copy->ffiElements;
                copy->ffiType = &copy->ffiAggregateType;
            }
#endif
            break;
        case ZR_FFI_TYPE_ENUM:
            copy->as.enumType.underlying = zr_ffi_clone_type(type->as.enumType.underlying);
            break;
        default:
            break;
    }

    return copy;
}

static void zr_ffi_init_primitive_type(ZrFfiTypeLayout *type, const char *name, TZrSize size, TZrSize align
#if ZR_VM_HAS_LIBFFI
                                       ,
                                       ffi_type *ffiType
#endif
) {
    if (type == ZR_NULL) {
        return;
    }

    type->name = zr_ffi_strdup(name);
    type->size = size;
    type->align = align;
#if ZR_VM_HAS_LIBFFI
    type->ffiType = ffiType;
#endif
}

static ZrFfiTypeLayout *zr_ffi_make_primitive_type(const char *name) {
    ZrFfiTypeLayout *type = ZR_NULL;

    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    if (strcmp(name, "void") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_VOID);
        zr_ffi_init_primitive_type(type, "void", 0, 1
#if ZR_VM_HAS_LIBFFI
                                   ,
                                   &ffi_type_void
#endif
        );
    } else if (strcmp(name, "bool") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_BOOL);
        zr_ffi_init_primitive_type(type, "bool", sizeof(unsigned char), sizeof(unsigned char)
#if ZR_VM_HAS_LIBFFI
                                                                                ,
                                   &ffi_type_uint8
#endif
        );
    } else if (strcmp(name, "i8") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I8);
        zr_ffi_init_primitive_type(type, "i8", sizeof(int8_t), sizeof(int8_t)
#if ZR_VM_HAS_LIBFFI
                                                                       ,
                                   &ffi_type_sint8
#endif
        );
    } else if (strcmp(name, "u8") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U8);
        zr_ffi_init_primitive_type(type, "u8", sizeof(uint8_t), sizeof(uint8_t)
#if ZR_VM_HAS_LIBFFI
                                                                        ,
                                   &ffi_type_uint8
#endif
        );
    } else if (strcmp(name, "i16") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I16);
        zr_ffi_init_primitive_type(type, "i16", sizeof(int16_t), sizeof(int16_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint16
#endif
        );
    } else if (strcmp(name, "u16") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U16);
        zr_ffi_init_primitive_type(type, "u16", sizeof(uint16_t), sizeof(uint16_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint16
#endif
        );
    } else if (strcmp(name, "i32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I32);
        zr_ffi_init_primitive_type(type, "i32", sizeof(int32_t), sizeof(int32_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint32
#endif
        );
    } else if (strcmp(name, "u32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U32);
        zr_ffi_init_primitive_type(type, "u32", sizeof(uint32_t), sizeof(uint32_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint32
#endif
        );
    } else if (strcmp(name, "i64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I64);
        zr_ffi_init_primitive_type(type, "i64", sizeof(int64_t), sizeof(int64_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint64
#endif
        );
    } else if (strcmp(name, "u64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U64);
        zr_ffi_init_primitive_type(type, "u64", sizeof(uint64_t), sizeof(uint64_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint64
#endif
        );
    } else if (strcmp(name, "f32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_F32);
        zr_ffi_init_primitive_type(type, "f32", sizeof(float), sizeof(float)
#if ZR_VM_HAS_LIBFFI
                                                                       ,
                                   &ffi_type_float
#endif
        );
    } else if (strcmp(name, "f64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_F64);
        zr_ffi_init_primitive_type(type, "f64", sizeof(double), sizeof(double)
#if ZR_VM_HAS_LIBFFI
                                                                        ,
                                   &ffi_type_double
#endif
        );
    }

    return type;
}

static ffi_abi zr_ffi_parse_abi(const char *abiText, char *errorBuffer, TZrSize errorBufferSize) {
    if (abiText == ZR_NULL || abiText[0] == '\0' || strcmp(abiText, "default") == 0 || strcmp(abiText, "system") == 0 ||
        strcmp(abiText, "sysv") == 0 || strcmp(abiText, "win64") == 0) {
#if ZR_VM_HAS_LIBFFI
        return FFI_DEFAULT_ABI;
#else
        return 0;
#endif
    }

    if (strcmp(abiText, "cdecl") == 0) {
#if ZR_VM_HAS_LIBFFI && defined(_WIN32) && !defined(_WIN64)
        return FFI_MS_CDECL;
#elif ZR_VM_HAS_LIBFFI
        return FFI_DEFAULT_ABI;
#else
        return 0;
#endif
    }

    if (strcmp(abiText, "stdcall") == 0) {
#if ZR_VM_HAS_LIBFFI && defined(_WIN32) && !defined(_WIN64)
        return FFI_STDCALL;
#elif ZR_VM_HAS_LIBFFI && defined(_WIN64)
        return FFI_DEFAULT_ABI;
#else
        snprintf(errorBuffer, errorBufferSize, "stdcall is not supported on this build");
        return 0;
#endif
    }

    snprintf(errorBuffer, errorBufferSize, "unsupported calling convention '%s'", abiText);
    return 0;
}

static ZrFfiTypeLayout *zr_ffi_pointer_type_from_target(const ZrFfiTypeLayout *target) {
    ZrFfiTypeLayout *pointerType = zr_ffi_new_type(ZR_FFI_TYPE_POINTER);
    if (pointerType == ZR_NULL) {
        return ZR_NULL;
    }

    pointerType->name = zr_ffi_strdup("pointer");
    pointerType->size = sizeof(void *);
    pointerType->align = sizeof(void *);
    pointerType->as.pointer.direction = ZR_FFI_DIRECTION_IN;
    pointerType->as.pointer.pointee = zr_ffi_clone_type(target);
#if ZR_VM_HAS_LIBFFI
    pointerType->ffiType = &ffi_type_pointer;
#endif
    if (pointerType->as.pointer.pointee == ZR_NULL) {
        zr_ffi_destroy_type(pointerType);
        return ZR_NULL;
    }
    return pointerType;
}

static ZrFfiTypeLayout *zr_ffi_parse_type_descriptor(SZrState *state, const SZrTypeValue *descriptorValue,
                                                     char *errorBuffer, TZrSize errorBufferSize) {
    ZrFfiTypeLayout *type;
    SZrObject *descriptorObject;
    const char *kindText;

    if (descriptorValue == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "missing type descriptor");
        return ZR_NULL;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_STRING) {
        const char *primitiveName = ZR_NULL;
        zr_ffi_read_string_value(state, descriptorValue, &primitiveName);
        type = zr_ffi_make_primitive_type(primitiveName);
        if (type == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "unknown primitive type '%s'",
                     primitiveName != ZR_NULL ? primitiveName : "<null>");
        }
        return type;
    }

    if (!zr_ffi_value_is_object(descriptorValue, &descriptorObject)) {
        snprintf(errorBuffer, errorBufferSize, "type descriptor must be a string or object");
        return ZR_NULL;
    }

    if (!zr_ffi_read_object_string_field(state, descriptorObject, "kind", &kindText)) {
        snprintf(errorBuffer, errorBufferSize, "type descriptor object is missing 'kind'");
        return ZR_NULL;
    }

    if (strcmp(kindText, "string") == 0) {
        const char *encodingText = "utf8";
        type = zr_ffi_new_type(ZR_FFI_TYPE_STRING);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("string");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        zr_ffi_read_object_string_field(state, descriptorObject, "encoding", &encodingText);
        if (strcmp(encodingText, "utf16") == 0) {
            type->as.stringType.encoding = ZR_FFI_STRING_UTF16;
        } else if (strcmp(encodingText, "ansi") == 0) {
            type->as.stringType.encoding = ZR_FFI_STRING_ANSI;
        } else {
            type->as.stringType.encoding = ZR_FFI_STRING_UTF8;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    if (strcmp(kindText, "pointer") == 0) {
        const SZrTypeValue *pointeeValue = zr_ffi_find_field_raw(state, descriptorObject, "to");
        const char *directionText = "in";
        type = zr_ffi_new_type(ZR_FFI_TYPE_POINTER);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("pointer");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        zr_ffi_read_object_string_field(state, descriptorObject, "direction", &directionText);
        if (strcmp(directionText, "out") == 0) {
            type->as.pointer.direction = ZR_FFI_DIRECTION_OUT;
        } else if (strcmp(directionText, "inout") == 0) {
            type->as.pointer.direction = ZR_FFI_DIRECTION_INOUT;
        } else {
            type->as.pointer.direction = ZR_FFI_DIRECTION_IN;
        }
        type->as.pointer.pointee = zr_ffi_parse_type_descriptor(state, pointeeValue, errorBuffer, errorBufferSize);
        if (type->as.pointer.pointee == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    if (strcmp(kindText, "struct") == 0 || strcmp(kindText, "union") == 0) {
        TZrBool isUnion = strcmp(kindText, "union") == 0 ? ZR_TRUE : ZR_FALSE;
        SZrObject *fieldsArray = ZR_NULL;
        TZrSize fieldCount;
        TZrSize index;
        TZrSize currentSize = 0;
        TZrSize maxAlign = 1;
        TZrInt64 packValue = 0;
        TZrInt64 explicitAlignValue = 0;
        TZrSize effectivePack = 0;

        if (!zr_ffi_read_object_field_object(state, descriptorObject, "fields", &fieldsArray) ||
            fieldsArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
            snprintf(errorBuffer, errorBufferSize, "%s descriptor is missing a 'fields' array", kindText);
            return ZR_NULL;
        }

        fieldCount = zr_ffi_array_length(state, fieldsArray);
        type = zr_ffi_new_type(isUnion ? ZR_FFI_TYPE_UNION : ZR_FFI_TYPE_STRUCT);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }

        zr_ffi_read_object_string_field(state, descriptorObject, "name", &kindText);
        type->name = zr_ffi_strdup(kindText != ZR_NULL ? kindText : (isUnion ? "union" : "struct"));
        zr_ffi_read_object_int_field(state, descriptorObject, "pack", &packValue);
        zr_ffi_read_object_int_field(state, descriptorObject, "align", &explicitAlignValue);
        if (packValue > 0) {
            effectivePack = (TZrSize)packValue;
        }
        type->as.aggregate.fieldCount = fieldCount;
        if (fieldCount > 0) {
            type->as.aggregate.fields = (ZrFfiFieldLayout *) calloc(fieldCount, sizeof(ZrFfiFieldLayout));
        }
        if (fieldCount > 0 && type->as.aggregate.fields == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }

        for (index = 0; index < fieldCount; index++) {
            const SZrTypeValue *fieldValue = zr_ffi_array_get(state, fieldsArray, index);
            SZrObject *fieldObject;
            const char *fieldName = ZR_NULL;
            const SZrTypeValue *fieldTypeValue;
            ZrFfiTypeLayout *fieldType;
            TZrInt64 explicitOffsetValue = -1;
            TZrSize fieldAlign;
            TZrSize fieldOffset;

            if (!zr_ffi_value_is_object(fieldValue, &fieldObject) ||
                !zr_ffi_read_object_string_field(state, fieldObject, "name", &fieldName)) {
                snprintf(errorBuffer, errorBufferSize, "aggregate field %llu is invalid", (unsigned long long) index);
                zr_ffi_destroy_type(type);
                return ZR_NULL;
            }

            fieldTypeValue = zr_ffi_find_field_raw(state, fieldObject, "type");
            fieldType = zr_ffi_parse_type_descriptor(state, fieldTypeValue, errorBuffer, errorBufferSize);
            if (fieldType == ZR_NULL) {
                zr_ffi_destroy_type(type);
                return ZR_NULL;
            }

            type->as.aggregate.fields[index].name = zr_ffi_strdup(fieldName);
            type->as.aggregate.fields[index].type = fieldType;
            zr_ffi_read_object_int_field(state, fieldObject, "offset", &explicitOffsetValue);
            fieldAlign = fieldType->align;
            if (effectivePack > 0 && fieldAlign > effectivePack) {
                fieldAlign = effectivePack;
            }

            if (explicitOffsetValue >= 0) {
                fieldOffset = (TZrSize)explicitOffsetValue;
            } else if (isUnion) {
                fieldOffset = 0;
            } else {
                fieldOffset = zr_ffi_align_up(currentSize, fieldAlign);
            }

            type->as.aggregate.fields[index].offset = fieldOffset;
            if (fieldOffset + fieldType->size > currentSize) {
                currentSize = fieldOffset + fieldType->size;
            }
            if (fieldAlign > maxAlign) {
                maxAlign = fieldAlign;
            }
        }

        if (effectivePack > 0 && maxAlign > effectivePack) {
            maxAlign = effectivePack;
        }
        if (explicitAlignValue > 0) {
            maxAlign = (TZrSize)explicitAlignValue;
        }
        type->align = maxAlign;
        type->size = zr_ffi_align_up(currentSize, maxAlign);
#if ZR_VM_HAS_LIBFFI
        type->ffiElements = (ffi_type **) calloc(fieldCount + 1, sizeof(ffi_type *));
        if (type->ffiElements == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
        for (index = 0; index < fieldCount; index++) {
            type->ffiElements[index] = type->as.aggregate.fields[index].type->ffiType;
        }
        memset(&type->ffiAggregateType, 0, sizeof(type->ffiAggregateType));
        type->ffiAggregateType.size = type->size;
        type->ffiAggregateType.alignment = (unsigned short)type->align;
        type->ffiAggregateType.type = FFI_TYPE_STRUCT;
        type->ffiAggregateType.elements = type->ffiElements;
        type->ffiType = &type->ffiAggregateType;
#endif
        return type;
    }

    if (strcmp(kindText, "enum") == 0) {
        const SZrTypeValue *underlyingValue = zr_ffi_find_field_raw(state, descriptorObject, "underlyingType");
        if (underlyingValue == ZR_NULL) {
            underlyingValue = zr_ffi_find_field_raw(state, descriptorObject, "valueType");
        }
        type = zr_ffi_new_type(ZR_FFI_TYPE_ENUM);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("enum");
        type->as.enumType.underlying =
                zr_ffi_parse_type_descriptor(state, underlyingValue, errorBuffer, errorBufferSize);
        if (type->as.enumType.underlying == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
        type->size = type->as.enumType.underlying->size;
        type->align = type->as.enumType.underlying->align;
#if ZR_VM_HAS_LIBFFI
        type->ffiType = type->as.enumType.underlying->ffiType;
#endif
        return type;
    }

    if (strcmp(kindText, "function") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_FUNCTION);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("function");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        type->as.functionType.signature = zr_ffi_parse_signature(state, descriptorObject, errorBuffer, errorBufferSize);
        if (type->as.functionType.signature == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    snprintf(errorBuffer, errorBufferSize, "unsupported type kind '%s'", kindText);
    return ZR_NULL;
}

static ZrFfiSignature *zr_ffi_parse_signature(SZrState *state, SZrObject *signatureObject, char *errorBuffer,
                                              TZrSize errorBufferSize) {
    ZrFfiSignature *signature;
    const SZrTypeValue *returnTypeValue;
    SZrObject *parametersArray = ZR_NULL;
    TZrSize parameterCount = 0;
    TZrSize index;
    const char *abiText = ZR_NULL;

    signature = (ZrFfiSignature *) calloc(1, sizeof(ZrFfiSignature));
    if (signature == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "out of memory while allocating signature");
        return ZR_NULL;
    }

    returnTypeValue = zr_ffi_find_field_raw(state, signatureObject, "returnType");
    signature->returnType = zr_ffi_parse_type_descriptor(state, returnTypeValue, errorBuffer, errorBufferSize);
    if (signature->returnType == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }

    zr_ffi_read_object_field_object(state, signatureObject, "parameters", &parametersArray);
    if (parametersArray != ZR_NULL) {
        parameterCount = zr_ffi_array_length(state, parametersArray);
    }
    signature->parameterCount = parameterCount;
    if (parameterCount > 0) {
        signature->parameters = (ZrFfiParameter *) calloc(parameterCount, sizeof(ZrFfiParameter));
    }
    if (parameterCount > 0 && signature->parameters == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "out of memory while allocating signature parameters");
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }

    for (index = 0; index < parameterCount; index++) {
        const SZrTypeValue *parameterValue = zr_ffi_array_get(state, parametersArray, index);
        const SZrTypeValue *parameterTypeValue = parameterValue;
        SZrObject *parameterObject = ZR_NULL;

        if (zr_ffi_value_is_object(parameterValue, &parameterObject)) {
            const SZrTypeValue *embeddedTypeValue = zr_ffi_find_field_raw(state, parameterObject, "type");
            if (embeddedTypeValue != ZR_NULL) {
                parameterTypeValue = embeddedTypeValue;
            }
        }

        signature->parameters[index].type =
                zr_ffi_parse_type_descriptor(state, parameterTypeValue, errorBuffer, errorBufferSize);
        if (signature->parameters[index].type == ZR_NULL) {
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    }

    zr_ffi_read_object_string_field(state, signatureObject, "abi", &abiText);
    if (abiText == ZR_NULL) {
        zr_ffi_read_object_string_field(state, signatureObject, "callingConvention", &abiText);
    }
    signature->abi = zr_ffi_parse_abi(abiText, errorBuffer, errorBufferSize);
    if (errorBuffer[0] != '\0') {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    zr_ffi_read_object_bool_field(state, signatureObject, "varargs", &signature->isVarargs);

#if ZR_VM_HAS_LIBFFI
    if (parameterCount > 0) {
        signature->ffiParameterTypes = (ffi_type **) calloc(parameterCount, sizeof(ffi_type *));
        if (signature->ffiParameterTypes == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "out of memory while allocating ffi parameter type list");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
        for (index = 0; index < parameterCount; index++) {
            signature->ffiParameterTypes[index] = signature->parameters[index].type->ffiType;
        }
    }

    if (signature->isVarargs) {
        if (ffi_prep_cif_var(&signature->cif, signature->abi, (unsigned int) parameterCount,
                             (unsigned int) parameterCount, signature->returnType->ffiType,
                             signature->ffiParameterTypes) == FFI_OK) {
            signature->cifPrepared = ZR_TRUE;
        } else {
            snprintf(errorBuffer, errorBufferSize, "ffi_prep_cif_var failed");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    } else {
        if (ffi_prep_cif(&signature->cif, signature->abi, (unsigned int) parameterCount, signature->returnType->ffiType,
                         signature->ffiParameterTypes) == FFI_OK) {
            signature->cifPrepared = ZR_TRUE;
        } else {
            snprintf(errorBuffer, errorBufferSize, "ffi_prep_cif failed");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    }
#endif

    return signature;
}

static TZrBool zr_ffi_extract_numeric_value(const SZrTypeValue *value, double *outDouble) {
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

static TZrBool zr_ffi_build_struct_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
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

static TZrBool zr_ffi_build_scalar_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            void *buffer, char *errorBuffer, TZrSize errorBufferSize) {
    double numericValue = 0.0;

    if (type == ZR_NULL || buffer == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "invalid scalar argument target");
        return ZR_FALSE;
    }

    switch (type->kind) {
        case ZR_FFI_TYPE_BOOL:
            if (!zr_ffi_read_bool_value(value, (TZrBool *) buffer)) {
                snprintf(errorBuffer, errorBufferSize, "expected bool-compatible value");
                return ZR_FALSE;
            }
            return ZR_TRUE;
        case ZR_FFI_TYPE_I8:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(int8_t *) buffer = (int8_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U8:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(uint8_t *) buffer = (uint8_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I16:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(int16_t *) buffer = (int16_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U16:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(uint16_t *) buffer = (uint16_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I32:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(int32_t *) buffer = (int32_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U32:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(uint32_t *) buffer = (uint32_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_I64:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(int64_t *) buffer = (int64_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_U64:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(uint64_t *) buffer = (uint64_t) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_F32:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(float *) buffer = (float) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_F64:
            if (!zr_ffi_extract_numeric_value(value, &numericValue)) {
                break;
            }
            *(double *) buffer = (double) numericValue;
            return ZR_TRUE;
        case ZR_FFI_TYPE_STRING: {
            const char *text = ZR_NULL;
            if (!zr_ffi_read_string_value(state, value, &text)) {
                break;
            }
            *(const char **) buffer = text;
            return ZR_TRUE;
        }
        case ZR_FFI_TYPE_POINTER: {
            SZrObject *pointerObject = ZR_NULL;
            ZrFfiPointerData *pointerData = ZR_NULL;
            if (value->type == ZR_VALUE_TYPE_NULL) {
                *(void **) buffer = ZR_NULL;
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

static void zr_ffi_callback_try_invoke(SZrState *state, TZrPtr arguments) {
    ZrFfiCallbackInvokeArgs *invokeArgs = (ZrFfiCallbackInvokeArgs *)arguments;

    if (state == ZR_NULL || invokeArgs == ZR_NULL || invokeArgs->result == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNull(invokeArgs->result);
    invokeArgs->succeeded =
            ZrLib_CallValue(state, invokeArgs->callbackValue, ZR_NULL, invokeArgs->argumentValues, invokeArgs->argumentCount, invokeArgs->result);
}

static TZrBool zr_ffi_struct_to_object(SZrState *state, ZrFfiTypeLayout *type, const unsigned char *bytes,
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

static TZrBool zr_ffi_set_result_from_scalar(SZrState *state, ZrFfiTypeLayout *type, const void *value,
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

static void zr_ffi_symbol_release_owner(SZrState *state, SZrObject *object) {
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

static void zr_ffi_pointer_release_owner(SZrState *state, SZrObject *object) {
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

static void zr_ffi_handle_finalize(SZrState *state, SZrRawObject *rawObject) {
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
static void zr_ffi_callback_trampoline(ffi_cif *cif, void *returnValue, void **arguments, void *userData) {
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

TZrBool ZrFfi_LoadLibrary(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrString *pathString = ZR_NULL;
    const char *pathText = ZR_NULL;
    char errorBuffer[256] = {0};
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
    char errorBuffer[256] = {0};
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
    char errorBuffer[256] = {0};
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
    char errorBuffer[256] = {0};

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
    versionProc = (const char *(*) (void) ) zr_ffi_lookup_symbol(libraryData->libraryHandle, symbolName, errorBuffer,
                                                                 sizeof(errorBuffer));
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
    char errorBuffer[256] = {0};

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
    char errorBuffer[256] = {0};
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
    ZrLib_Value_SetObject(context->state, result, pointerObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Library_GetSymbol(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiLibraryData *libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(context->state, selfObject);
    SZrString *symbolNameString = ZR_NULL;
    const char *symbolName;
    SZrObject *signatureObject = ZR_NULL;
    ZrFfiSignature *signature;
    ZrFfiSymbolData *symbolData;
    SZrObject *symbolObject;
    SZrTypeValue ownerValue;
    char errorBuffer[256] = {0};
    void *symbolAddress;

    if (selfObject == ZR_NULL || libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadString(context, 0, &symbolNameString) ||
        !ZrLib_CallContext_ReadObject(context, 1, &signatureObject) || signatureObject == ZR_NULL) {
        return ZR_FALSE;
    }
    if (libraryData->closeRequested || libraryData->libraryHandle == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_LOAD, "library handle is closed");
        return ZR_FALSE;
    }

    symbolName = ZrCore_String_GetNativeString(symbolNameString);
    signature = zr_ffi_parse_signature(context->state, signatureObject, errorBuffer, sizeof(errorBuffer));
    if (signature == ZR_NULL) {
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "%s", errorBuffer);
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

TZrBool ZrFfi_Pointer_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(context->state, selfObject);
    if (selfObject == ZR_NULL || pointerData == ZR_NULL || pointerData->base.kind != ZR_FFI_HANDLE_POINTER) {
        return ZR_FALSE;
    }
    if (!pointerData->closed) {
        pointerData->closed = ZR_TRUE;
        pointerData->address = ZR_NULL;
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
    char errorBuffer[256] = {0};

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
    newPointerData->type = newType;
    ownerValue = zr_ffi_find_field_raw(context->state, selfObject, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue != ZR_NULL && ownerValue->type == ZR_VALUE_TYPE_OBJECT && ownerValue->value.object != ZR_NULL) {
        SZrObject *ownerObject = ZR_CAST_OBJECT(context->state, ownerValue->value.object);
        ZrFfiBufferData *bufferData = (ZrFfiBufferData *) zr_ffi_get_handle_data(context->state, ownerObject);
        if (bufferData != ZR_NULL && bufferData->base.kind == ZR_FFI_HANDLE_BUFFER) {
            bufferData->pinCount++;
        }
    }
    pointerObject = zr_ffi_new_handle_object_with_finalizer(context->state, "PointerHandle", &newPointerData->base,
                                                            ownerValue, ZR_NULL);
    if (pointerObject == ZR_NULL) {
        zr_ffi_destroy_type(newType);
        free(newPointerData);
        zr_ffi_raise_error(context->state, ZR_FFI_ERROR_MARSHAL, "failed to instantiate PointerHandle");
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(context->state, result, pointerObject, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrFfi_Pointer_Read(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *selfObject = zr_ffi_get_self_object(context);
    ZrFfiPointerData *pointerData = (ZrFfiPointerData *) zr_ffi_get_handle_data(context->state, selfObject);
    ZrFfiTypeLayout *type;
    char errorBuffer[256] = {0};
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

static TZrBool zr_ffi_symbol_invoke_array(SZrState *state,
                                          SZrObject *selfObject,
                                          ZrFfiSymbolData *symbolData,
                                          SZrObject *argumentsArray,
                                          SZrTypeValue *result) {
    const SZrTypeValue *ownerValue;
    SZrObject *ownerObject;
    ZrFfiLibraryData *libraryData;
    TZrSize argumentCount;
    TZrSize index;
    char errorBuffer[256] = {0};
#if ZR_VM_HAS_LIBFFI
    ZrFfiMarshalledValue *marshalledValues = ZR_NULL;
    void **ffiArguments = ZR_NULL;
    SZrTypeValue **callbackArguments = ZR_NULL;
    unsigned char *returnStorage = ZR_NULL;
    TZrBool callSucceeded = ZR_FALSE;
#endif
    if (selfObject == ZR_NULL || symbolData == ZR_NULL || symbolData->base.kind != ZR_FFI_HANDLE_SYMBOL) {
        return ZR_FALSE;
    }
    if (symbolData->closed) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_NATIVE_CALL, "symbol handle is closed");
        return ZR_FALSE;
    }
    if (argumentsArray == ZR_NULL) {
        return ZR_FALSE;
    }
    ownerValue = zr_ffi_find_field_raw(state, selfObject, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue == ZR_NULL || ownerValue->type != ZR_VALUE_TYPE_OBJECT || ownerValue->value.object == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_NATIVE_CALL, "symbol handle has no owning LibraryHandle");
        return ZR_FALSE;
    }
    ownerObject = ZR_CAST_OBJECT(state, ownerValue->value.object);
    libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(state, ownerObject);
    if (libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY || libraryData->closeRequested ||
        libraryData->libraryHandle == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_LOAD, "owning library handle is closed");
        return ZR_FALSE;
    }
    argumentCount = zr_ffi_array_length(state, argumentsArray);
    if (symbolData->signature == ZR_NULL || argumentCount != symbolData->signature->parameterCount) {
        zr_ffi_raise_error(
                state, ZR_FFI_ERROR_MARSHAL, "symbol '%s' expected %llu arguments but got %llu",
                symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>",
                (unsigned long long) (symbolData->signature != ZR_NULL ? symbolData->signature->parameterCount : 0),
                (unsigned long long) argumentCount);
        return ZR_FALSE;
    }
#if !ZR_VM_HAS_LIBFFI
    zr_ffi_raise_error(state, ZR_FFI_ERROR_ABI_MISMATCH, "this build does not include libffi");
    return ZR_FALSE;
#else
    if (argumentCount > 0) {
        marshalledValues = (ZrFfiMarshalledValue *) calloc(argumentCount, sizeof(ZrFfiMarshalledValue));
        ffiArguments = (void **) calloc(argumentCount, sizeof(void *));
        callbackArguments = (SZrTypeValue **) calloc(argumentCount, sizeof(SZrTypeValue *));
    }
    if ((argumentCount > 0 &&
         (marshalledValues == ZR_NULL || ffiArguments == ZR_NULL || callbackArguments == ZR_NULL))) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "out of memory while preparing ffi arguments");
        goto cleanup;
    }
    for (index = 0; index < argumentCount; index++) {
        const SZrTypeValue *argumentValue = zr_ffi_array_get(state, argumentsArray, index);
        ZrFfiTypeLayout *parameterType = symbolData->signature->parameters[index].type;
        ZrFfiMarshalledValue *marshalledValue = &marshalledValues[index];
        if (parameterType->kind == ZR_FFI_TYPE_FUNCTION) {
            SZrObject *callbackObject = ZR_NULL;
            ZrFfiCallbackData *callbackData = ZR_NULL;
            if (!zr_ffi_value_is_object(argumentValue, &callbackObject)) {
                zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "argument %llu must be a CallbackHandle",
                                   (unsigned long long) (index + 1));
                goto cleanup;
            }
            callbackData = (ZrFfiCallbackData *) zr_ffi_get_handle_data(state, callbackObject);
            if (callbackData == ZR_NULL || callbackData->base.kind != ZR_FFI_HANDLE_CALLBACK || callbackData->closed) {
                zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "argument %llu is not an open CallbackHandle",
                                   (unsigned long long) (index + 1));
                goto cleanup;
            }
            callbackData->lastError = ZR_FFI_ERROR_NONE;
            callbackData->lastErrorMessage[0] = '\0';
            marshalledValue->ownedAllocation = calloc(1, sizeof(void *));
            if (marshalledValue->ownedAllocation == ZR_NULL) {
                zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL,
                                   "out of memory while marshalling callback argument");
                goto cleanup;
            }
            *(void **) marshalledValue->ownedAllocation = callbackData->codePointer;
            marshalledValue->argumentPointer = marshalledValue->ownedAllocation;
            ffiArguments[index] = marshalledValue->argumentPointer;
            callbackArguments[index] = (SZrTypeValue *) argumentValue;
            continue;
        }
        marshalledValue->ownedAllocation = calloc(1, zr_ffi_non_void_call_storage_size(parameterType));
        if (marshalledValue->ownedAllocation == ZR_NULL) {
            zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "out of memory while marshalling argument");
            goto cleanup;
        }
        if (!zr_ffi_build_scalar_argument(state, argumentValue, parameterType,
                                          marshalledValue->ownedAllocation, errorBuffer, sizeof(errorBuffer))) {
            zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL,
                               "argument %llu for symbol '%s' failed to marshal: %s", (unsigned long long) (index + 1),
                               symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>", errorBuffer);
            goto cleanup;
        }
        marshalledValue->argumentPointer = marshalledValue->ownedAllocation;
        ffiArguments[index] = marshalledValue->argumentPointer;
    }
    returnStorage = (unsigned char *) calloc(1, zr_ffi_non_void_call_storage_size(symbolData->signature->returnType));
    if (returnStorage == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "out of memory while preparing ffi return storage");
        goto cleanup;
    }
    if (!zr_ffi_invoke_native_symbol(symbolData, returnStorage, ffiArguments, errorBuffer, sizeof(errorBuffer))) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_NATIVE_CALL, "%s",
                           errorBuffer[0] != '\0' ? errorBuffer : "ffi native call failed");
        goto cleanup;
    }
    for (index = 0; index < argumentCount; index++) {
        if (callbackArguments[index] != ZR_NULL) {
            SZrObject *callbackObject = ZR_CAST_OBJECT(state, callbackArguments[index]->value.object);
            ZrFfiCallbackData *callbackData =
                    (ZrFfiCallbackData *) zr_ffi_get_handle_data(state, callbackObject);
            if (callbackData != ZR_NULL && callbackData->lastError != ZR_FFI_ERROR_NONE) {
                zr_ffi_raise_error(state, callbackData->lastError, "%s",
                                   callbackData->lastErrorMessage[0] != '\0' ? callbackData->lastErrorMessage
                                                                             : "ffi callback failed");
                goto cleanup;
            }
        }
    }
    if (!zr_ffi_set_result_from_scalar(state, symbolData->signature->returnType, returnStorage, result)) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "failed to marshal return value from symbol '%s'",
                           symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>");
        goto cleanup;
    }
    callSucceeded = ZR_TRUE;
cleanup:
    if (marshalledValues != ZR_NULL) {
        for (index = 0; index < argumentCount; index++) {
            free(marshalledValues[index].ownedAllocation);
        }
    }
    free(marshalledValues);
    free(ffiArguments);
    free(callbackArguments);
    free(returnStorage);
    return callSucceeded;
#endif
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

    return zr_ffi_symbol_invoke_array(context->state, selfObject, symbolData, argumentsArray, result);
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
        TZrBool succeeded = zr_ffi_symbol_invoke_array(context->state, selfObject, symbolData, argumentsArray, result);
        ZrLib_TempValueRoot_End(&arrayRoot);
        return succeeded;
    }
}
