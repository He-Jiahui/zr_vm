#include "ffi_runtime/ffi_runtime_internal.h"

/* 将内部错误类别转换为语言层异常前缀，供所有 FFI 入口保持一致的诊断格式。 */
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

/* 在当前 VM 状态中抛出带类别的 FFI 异常；调用方不应把失败返回当作普通结果。 */
void zr_ffi_raise_error(SZrState *state, ZrFfiErrorCode code, const char *format, ...) {
    char message[ZR_FFI_DIAGNOSTIC_BUFFER_LENGTH];
    va_list arguments;

    if (state == ZR_NULL || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    ZrCore_Debug_RunError(state, "[%s] %s", zr_ffi_error_name(code), message);
}

/* 为库路径、符号名和布局字段建立独立副本；返回值由对应句柄或布局析构路径释放。 */
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

/* BUG: 动态类型描述符允许非 2 的幂 align/pack，却在此按位掩码取整；例如单 i8 字段
 * 且 align=3 时，ffi.sizeof 可返回 1、ffi.alignof 返回 3。需在描述符入口验证对齐并检查加法溢出。
 */
TZrSize zr_ffi_align_up(TZrSize value, TZrSize alignment) {
    TZrSize mask;

    if (alignment <= 1) {
        return value;
    }

    mask = alignment - 1;
    return (value + mask) & ~mask;
}

/* libffi 对窄整数采用 ABI 整数槽位；调用与回调的临时存储必须按槽位宽度分配。 */
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

/* 即使声明返回 void，调用边界仍保留可传递的最小存储区域。 */
TZrSize zr_ffi_non_void_call_storage_size(const ZrFfiTypeLayout *type) {
    TZrSize storageSize = zr_ffi_call_storage_size(type);
    return storageSize > 0 ? storageSize : sizeof(void *);
}

/* 回调拒绝线程或抛异常时向 native 方交付确定的零值，避免未初始化返回槽泄露。 */
void zr_ffi_zero_call_storage(const ZrFfiTypeLayout *type, void *storage) {
    TZrSize storageSize;

    if (storage == ZR_NULL) {
        return;
    }

    storageSize = zr_ffi_non_void_call_storage_size(type);
    memset(storage, 0, storageSize);
}

/* 唯一的 native 进入点：上层须先准备 CIF，本层只校验符号句柄；Windows SEH 转为 FFI 错误，
 * POSIX 调用方须保证 native 函数不以进程信号或跨边界异常逃逸。
 */
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

/* 返回 VM 字符串的借用 C 字符串视图；描述符和符号名消费者按 NUL 终止语义读取。 */
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

/* BUG: Buffer_Write 等公开入口可传入超出 int64 范围的浮点值；此处直接转整数在 C 中
 * 属未定义行为。需先拒绝非有限、非整数及越界值，再交给字节或布局消费者。
 */
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

/* 从语言对象取借用的 native 句柄数据；调用方必须再核对 kind 和关闭状态。 */
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

/* 优先使用 zr.ffi 命名空间原型，兼容调用方已经持有的未限定原型名称。 */
static SZrObject *zr_ffi_new_instance_for_module_type(SZrState *state, const char *typeName) {
    char qualified[160];
    SZrObject *object;
    int written;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    written = snprintf(qualified, sizeof(qualified), "zr.ffi.%s", typeName);
    if (written < 0 || (TZrSize)written >= sizeof(qualified)) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, qualified);
    if (object == ZR_NULL) {
        object = ZrLib_Type_NewInstance(state, typeName);
    }
    return object;
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

/* 为 native 资源创建可被 VM 追踪的句柄；隐藏 owner/callback 字段维持依赖对象可达性。
 * BUG: SetFieldCString 的分配/pin 失败会静默返回；缺失 owner 的 SymbolHandle 仍被交付，
 * 库可被 GC 提前 dlclose 而留下失效 symbolAddress；缺失 callback 字段则无法取到 VM 回调。须用故障注入核验回滚。
 */
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

    object = zr_ffi_new_instance_for_module_type(state, typeName);
    if (object == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_NULL;
    }
    ZrLib_TempValueRoot_SetObject(&objectRoot, object, ZR_VALUE_TYPE_OBJECT);

    object->super.finalizerData = data;
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

/* 仅在库句柄不存在仍存活的符号引用后调用；平台加载器引用由此归还。 */
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

/* 加载库并将所有权交给 LibraryHandle；POSIX 采用 RTLD_LOCAL 隔离动态符号。 */
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

/* 查得的地址借用 LibraryHandle 的加载器生命期；SymbolHandle 必须保持库活着。 */
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
