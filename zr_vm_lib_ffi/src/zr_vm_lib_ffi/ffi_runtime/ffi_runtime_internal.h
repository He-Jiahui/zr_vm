//
// Internal zr.ffi runtime interfaces shared across translation units.
//

#ifndef ZR_VM_LIB_FFI_RUNTIME_INTERNAL_H
#define ZR_VM_LIB_FFI_RUNTIME_INTERNAL_H

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

#define ZR_FFI_HIDDEN_HANDLE_FIELD "__zr_ffi_handle"
#define ZR_FFI_HIDDEN_HANDLE_ID_FIELD "__zr_ffi_handleId"
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
    char lastErrorMessage[ZR_FFI_ERROR_BUFFER_LENGTH];
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

const char *zr_ffi_error_name(ZrFfiErrorCode code);
void zr_ffi_raise_error(SZrState *state, ZrFfiErrorCode code, const char *format, ...);
char *zr_ffi_strdup(const char *text);
TZrSize zr_ffi_align_up(TZrSize value, TZrSize alignment);
TZrSize zr_ffi_call_storage_size(const ZrFfiTypeLayout *type);
TZrSize zr_ffi_non_void_call_storage_size(const ZrFfiTypeLayout *type);
void zr_ffi_zero_call_storage(const ZrFfiTypeLayout *type, void *storage);
TZrBool zr_ffi_invoke_native_symbol(ZrFfiSymbolData *symbolData, void *returnStorage, void **ffiArguments,
                                           char *errorBuffer, TZrSize errorBufferSize);
const SZrTypeValue *zr_ffi_find_field_raw(SZrState *state, SZrObject *object, const char *fieldName);
TZrBool zr_ffi_read_string_value(SZrState *state, const SZrTypeValue *value, const char **outText);
TZrBool zr_ffi_read_object_string_field(SZrState *state, SZrObject *object, const char *fieldName,
                                               const char **outText);
TZrBool zr_ffi_read_bool_value(const SZrTypeValue *value, TZrBool *outValue);
TZrBool zr_ffi_read_object_bool_field(SZrState *state, SZrObject *object, const char *fieldName,
                                             TZrBool *outValue);
TZrBool zr_ffi_read_int_value(const SZrTypeValue *value, TZrInt64 *outValue);
TZrBool zr_ffi_value_is_object(const SZrTypeValue *value, SZrObject **outObject);
TZrBool zr_ffi_read_object_int_field(SZrState *state, SZrObject *object, const char *fieldName,
                                            TZrInt64 *outValue);
TZrBool zr_ffi_read_object_field_object(SZrState *state, SZrObject *object, const char *fieldName,
                                               SZrObject **outObject);
const SZrTypeValue *zr_ffi_array_get(SZrState *state, SZrObject *array, TZrSize index);
TZrSize zr_ffi_array_length(SZrState *state, SZrObject *array);
ZrFfiHandleData *zr_ffi_get_handle_data(SZrState *state, SZrObject *object);
void zr_ffi_set_hidden_pointer(SZrState *state, SZrObject *object, const char *fieldName, void *pointerValue);
void zr_ffi_set_hidden_value(SZrState *state, SZrObject *object, const char *fieldName,
                                    const SZrTypeValue *value);
SZrObject *zr_ffi_get_self_object(const ZrLibCallContext *context);
SZrObject *zr_ffi_new_handle_object_with_finalizer(SZrState *state, const char *typeName, ZrFfiHandleData *data,
                                                          const SZrTypeValue *ownerValue,
                                                          const SZrTypeValue *callbackValue);
void zr_ffi_close_dynamic_library(void *libraryHandle);
void *zr_ffi_open_dynamic_library(const char *path, char *errorBuffer, TZrSize errorBufferSize);
void *zr_ffi_lookup_symbol(void *libraryHandle, const char *symbolName, char *errorBuffer,
                                  TZrSize errorBufferSize);
void zr_ffi_destroy_type(ZrFfiTypeLayout *type);
void zr_ffi_destroy_signature(ZrFfiSignature *signature);
ZrFfiTypeLayout *zr_ffi_new_type(ZrFfiTypeKind kind);
ZrFfiTypeLayout *zr_ffi_clone_type(const ZrFfiTypeLayout *type);
void zr_ffi_init_primitive_type(ZrFfiTypeLayout *type, const char *name, TZrSize size, TZrSize align
#if ZR_VM_HAS_LIBFFI
                                       ,
                                       ffi_type *ffiType
#endif
);
ZrFfiTypeLayout *zr_ffi_make_primitive_type(const char *name);
ffi_abi zr_ffi_parse_abi(const char *abiText, char *errorBuffer, TZrSize errorBufferSize);
ZrFfiTypeLayout *zr_ffi_pointer_type_from_target(const ZrFfiTypeLayout *target);
ZrFfiTypeLayout *zr_ffi_parse_type_descriptor(SZrState *state, const SZrTypeValue *descriptorValue,
                                                     char *errorBuffer, TZrSize errorBufferSize);
ZrFfiSignature *zr_ffi_parse_signature(SZrState *state, SZrObject *signatureObject, char *errorBuffer,
                                              TZrSize errorBufferSize);
TZrBool zr_ffi_extract_numeric_value(const SZrTypeValue *value, double *outDouble);
TZrBool zr_ffi_build_struct_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            unsigned char *buffer, char *errorBuffer, TZrSize errorBufferSize);
TZrBool zr_ffi_build_scalar_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            void *buffer, char *errorBuffer, TZrSize errorBufferSize);
void zr_ffi_callback_try_invoke(SZrState *state, TZrPtr arguments);
TZrBool zr_ffi_struct_to_object(SZrState *state, ZrFfiTypeLayout *type, const unsigned char *bytes,
                                       SZrTypeValue *result);
TZrBool zr_ffi_set_result_from_scalar(SZrState *state, ZrFfiTypeLayout *type, const void *value,
                                             SZrTypeValue *result);
void zr_ffi_symbol_release_owner(SZrState *state, SZrObject *object);
void zr_ffi_pointer_release_owner(SZrState *state, SZrObject *object);
void zr_ffi_handle_finalize(SZrState *state, SZrRawObject *rawObject);
void zr_ffi_callback_trampoline(ffi_cif *cif, void *returnValue, void **arguments, void *userData);
TZrBool zr_ffi_symbol_invoke_array(SZrState *state,
                                          SZrObject *selfObject,
                                          ZrFfiSymbolData *symbolData,
                                          SZrObject *argumentsArray,
                                          SZrTypeValue *result);

#endif // ZR_VM_LIB_FFI_RUNTIME_INTERNAL_H
