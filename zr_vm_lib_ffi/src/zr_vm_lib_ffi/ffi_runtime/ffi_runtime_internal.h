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
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/property_reference.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"

#include <limits.h>
#include <errno.h>
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

/* native call 的整数槽可能宽于声明的窄整数；有 libffi 时采用
 * ffi_arg 宽度，降级构建保留同一类指针宽度表示。 */
#if ZR_VM_HAS_LIBFFI
typedef ffi_arg ZrFfiAbiUnsignedSlot;
typedef ffi_sarg ZrFfiAbiSignedSlot;
#else
typedef uintptr_t ZrFfiAbiUnsignedSlot;
typedef intptr_t ZrFfiAbiSignedSlot;
#endif

/* VM 对象的隐藏 native-data 入口与 finalizerData 指向同一份句柄数据；
 * 取出后仍必须校验 kind 和关闭状态。 */
#define ZR_FFI_HIDDEN_HANDLE_FIELD "__zr_ffi_handle"
#define ZR_FFI_HIDDEN_HANDLE_ID_FIELD "__zr_ffi_handleId"
/* 让 Symbol/Pointer 对象持有 Library/Buffer 的 GC 引用；关闭时还须
 * 配合 openSymbolCount 或 pinCount 才能释放底层地址。 */
#define ZR_FFI_HIDDEN_OWNER_FIELD "__zr_ffi_owner"
/* CallbackHandle 以 VM 可扫描字段保持用户 callable 存活；trampoline
 * 再从 ownerObject 读取它，不能仅依赖 native 裸指针。 */
#define ZR_FFI_HIDDEN_CALLBACK_FIELD "__zr_ffi_callback"

/* native 错误按加载、ABI、编组、线程和执行阶段分类，最终由
 * zr_ffi_raise_error 映射为语言侧可观察诊断。 */
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

/* 动态描述符和 canonical 合同落到同一类型树；kind 决定 union 成员
 * 的解释、marshal 分支与递归析构路径。 */
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

/* 指针参数的方向控制临时 pointee 的初始值及 native 返回后的写回。 */
typedef enum ZrFfiDirection { ZR_FFI_DIRECTION_IN = 0, ZR_FFI_DIRECTION_OUT, ZR_FFI_DIRECTION_INOUT } ZrFfiDirection;

/* 字符串描述符可记录 UTF-8、UTF-16 或 ANSI 意图；实际编组路径
 * 仍需与该标记保持一致，相关风险见 descriptor 解析处。 */
typedef enum ZrFfiStringEncoding {
    ZR_FFI_STRING_UTF8 = 0,
    ZR_FFI_STRING_UTF16,
    ZR_FFI_STRING_ANSI
} ZrFfiStringEncoding;

/* 所有句柄共享首字段 kind；公开方法先核对种类，GC finalizer
 * 据此分发库、符号、回调、指针和缓冲区的资源释放。 */
typedef enum ZrFfiHandleKind {
    ZR_FFI_HANDLE_LIBRARY = 0,
    ZR_FFI_HANDLE_SYMBOL,
    ZR_FFI_HANDLE_CALLBACK,
    ZR_FFI_HANDLE_POINTER,
    ZR_FFI_HANDLE_BUFFER
} ZrFfiHandleKind;

typedef struct ZrFfiTypeLayout ZrFfiTypeLayout;
typedef struct ZrFfiSignature ZrFfiSignature;

/* 聚合字段描述 native 相对偏移及自有子类型树；名称和子树由父
 * TypeLayout 释放，偏移必须服从目标 ABI 布局。 */
typedef struct ZrFfiFieldLayout {
    char *name;
    ZrFfiTypeLayout *type;
    TZrSize offset;
} ZrFfiFieldLayout;

/* 每个 native 参数拥有独立类型树，整体由 ZrFfiSignature 回收。 */
typedef struct ZrFfiParameter {
    ZrFfiTypeLayout *type;
} ZrFfiParameter;

/* SymbolHandle/CallbackHandle 的可执行调用契约：参数与返回树自有，
 * 策略决定 callback 和错误处理，CIF 仅在带 libffi 的构建中准备。 */
struct ZrFfiSignature {
    ffi_abi abi;
    TZrUInt64 canonicalSignatureHash;
    TZrBool isVarargs;
    EZrFfiErrorPolicy errorPolicy;
    EZrFfiCleanupPolicy cleanupPolicy;
    EZrFfiCallbackLifetime callbackLifetime;
    EZrFfiCallbackThreadPolicy callbackThreadPolicy;
    EZrFfiCallbackExceptionPolicy callbackExceptionPolicy;
    TZrSize parameterCount;
    ZrFfiParameter *parameters;
    ZrFfiTypeLayout *returnType;
#if ZR_VM_HAS_LIBFFI
    ffi_cif cif;
    ffi_type **ffiParameterTypes;
    TZrBool cifPrepared;
#endif
};

/* 递归布局树同时承载语言描述符、native 大小对齐和 libffi 表示。
 * as 分支由 kind 选定；子树及 ffiElements 自有，原语 ffiType 可借用。 */
struct ZrFfiTypeLayout {
    ZrFfiTypeKind kind;
    TZrSize size;
    TZrSize align;
    TZrUInt64 canonicalSignatureHash;
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

/* 派生句柄必须以此结构为首字段，供 VM finalizerData 转型与
 * kind 分发；finalized 防止同一 native 资源重复归还。 */
typedef struct ZrFfiHandleData {
    ZrFfiHandleKind kind;
    TZrBool finalized;
} ZrFfiHandleData;

/* LibraryHandle 持有加载器引用；closeRequested 阻止后续调用，
 * 已创建 SymbolHandle 仍借用地址，计数清零后才真正卸载库。 */
typedef struct ZrFfiLibraryData {
    ZrFfiHandleData base;
    void *libraryHandle;
    char *libraryPath;
    TZrBool closeRequested;
    TZrSize openSymbolCount;
} ZrFfiLibraryData;

/* symbolAddress 借用所属动态库的加载器生存期；隐藏 owner 字段
 * 保持 LibraryHandle 可达，签名树由此 SymbolHandle 独占。 */
typedef struct ZrFfiSymbolData {
    ZrFfiHandleData base;
    void *symbolAddress;
    char *symbolName;
    ZrFfiSignature *signature;
    TZrBool closed;
} ZrFfiSymbolData;

/* libffi closure 将 native 调用送回创建它的 VM state/线程。
 * ownerObject 保持用户 callable 根，active* 策略只在调用窗口生效。 */
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
    TZrBool invocationActive;
    EZrFfiCallbackLifetime activeLifetime;
    EZrFfiCallbackThreadPolicy activeThreadPolicy;
    EZrFfiCallbackExceptionPolicy activeExceptionPolicy;
    TZrBool callbackExceptionObserved;
    ZrFfiErrorCode lastError;
    char lastErrorMessage[ZR_FFI_ERROR_BUFFER_LENGTH];
#if ZR_VM_HAS_LIBFFI
    ffi_closure *closure;
#endif
    void *codePointer;
} ZrFfiCallbackData;

/* PointerHandle 不拥有 address；若地址来自 BufferHandle，隐藏 owner
 * 与 pinCount 共同延长其有效期，closed 后不可再读取。 */
typedef struct ZrFfiPointerData {
    ZrFfiHandleData base;
    unsigned char *address;
    TZrSize byteLength;
    ZrFfiTypeLayout *type;
    TZrBool closed;
} ZrFfiPointerData;

/* BufferHandle 拥有 bytes；closeRequested 先阻止新借用，现有
 * PointerHandle 的 pinCount 清零后才可释放字节区。 */
typedef struct ZrFfiBufferData {
    ZrFfiHandleData base;
    unsigned char *bytes;
    TZrSize size;
    TZrBool closeRequested;
    TZrSize pinCount;
} ZrFfiBufferData;

/* 一次 symbol 调用的临时参数帧：记录需释放的本地内存和 callback
 * 状态快照，统一 cleanup 后不得再被 native 保留。 */
typedef struct ZrFfiMarshalledValue {
    void *argumentPointer;
    void *ownedAllocation;
    void *ownedPointeeAllocation;
    void *callbackCodePointer;
    ZrFfiCallbackData *callbackData;
    TZrBool callbackActivationInstalled;
    TZrBool savedInvocationActive;
    EZrFfiCallbackLifetime savedLifetime;
    EZrFfiCallbackThreadPolicy savedThreadPolicy;
    EZrFfiCallbackExceptionPolicy savedExceptionPolicy;
    TZrBool savedCallbackExceptionObserved;
    ZrFfiErrorCode savedLastError;
    char savedLastErrorMessage[ZR_FFI_ERROR_BUFFER_LENGTH];
} ZrFfiMarshalledValue;

/* trampoline 向 Exception_TryRun 传递的短命调用上下文；结果槽
 * 和参数数组只在这次同步 VM 回调中借用。 */
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
ZrFfiSignature *zr_ffi_signature_from_contract(
        const SZrNativeImportContract *contract,
        char *errorBuffer,
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
                                          ZrLibCallContext *writebackContext,
                                          SZrTypeValue *result);

#endif // ZR_VM_LIB_FFI_RUNTIME_INTERNAL_H
