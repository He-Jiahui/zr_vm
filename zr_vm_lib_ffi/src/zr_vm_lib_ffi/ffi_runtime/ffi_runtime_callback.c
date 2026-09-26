#include "ffi_runtime/ffi_runtime_internal.h"

/** @brief 为原生实参封送及联合体活动字段判定提取 VM 数值。 */
/* BUG: i64/u64 经 double 中转会丢失大于 2^53 的部分整数精度；
 * SymbolHandle.call 和回调返回值均会再转换为整数，
 * tests/ffi/test_ffi_module.c 的 9007199254740993 恒等调用应保持原值。 */
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

/** @brief 用值约定推断联合体活动字段，供聚合实参封送检查唯一性。 */
/* TODO: 零值、false 和 null 均被视为未激活，当前约定无法表达值为零的活动字段；
 * 对照 union 参数契约及 tests/ffi/test_native_extern_contract.c 的歧义用例，确认是否需要显式判别字段。 */
static TZrBool zr_ffi_union_field_is_default(
        SZrState *state,
        const SZrTypeValue *value,
        const ZrFfiTypeLayout *type) {
    double numericValue = 0.0;
    TZrBool boolValue = ZR_FALSE;

    if (value == ZR_NULL || value->type == ZR_VALUE_TYPE_NULL) {
        return ZR_TRUE;
    }
    if (type != ZR_NULL && type->kind == ZR_FFI_TYPE_ENUM) {
        return zr_ffi_union_field_is_default(
                state, value, type->as.enumType.underlying);
    }
    if (type != ZR_NULL && type->kind == ZR_FFI_TYPE_BOOL &&
        zr_ffi_read_bool_value(value, &boolValue)) {
        return !boolValue;
    }
    if (zr_ffi_extract_numeric_value(value, &numericValue)) {
        return numericValue == 0.0;
    }
    (void)state;
    return ZR_FALSE;
}

/** @brief 将 VM 对象按已解析的 ABI 布局封送为 struct/union 的原生传值缓冲区。
 * @note 调用方提供至少 type->size 字节；联合体要求恰有一个非默认字段。
 */
TZrBool zr_ffi_build_struct_argument(SZrState *state, const SZrTypeValue *value, ZrFfiTypeLayout *type,
                                            unsigned char *buffer, char *errorBuffer, TZrSize errorBufferSize) {
    TZrSize index;
    SZrObject *object;

    if (!zr_ffi_value_is_object(value, &object)) {
        snprintf(errorBuffer, errorBufferSize, "expected object value for aggregate argument");
        return ZR_FALSE;
    }

    memset(buffer, 0, type->size);
    if (type->kind == ZR_FFI_TYPE_UNION) {
        const ZrFfiFieldLayout *activeField = ZR_NULL;
        const SZrTypeValue *activeValue = ZR_NULL;

        for (index = 0; index < type->as.aggregate.fieldCount; index++) {
            const ZrFfiFieldLayout *field =
                    &type->as.aggregate.fields[index];
            const SZrTypeValue *fieldValue =
                    zr_ffi_find_field_raw(state, object, field->name);
            if (fieldValue == ZR_NULL) {
                snprintf(
                        errorBuffer,
                        errorBufferSize,
                        "missing field '%s' in union value",
                        field->name);
                return ZR_FALSE;
            }
            if (!zr_ffi_union_field_is_default(
                        state, fieldValue, field->type)) {
                if (activeField != ZR_NULL) {
                    snprintf(
                            errorBuffer,
                            errorBufferSize,
                            "union argument must identify exactly one non-default active field");
                    return ZR_FALSE;
                }
                activeField = field;
                activeValue = fieldValue;
            }
        }
        if (activeField == ZR_NULL) {
            snprintf(
                    errorBuffer,
                    errorBufferSize,
                    "union argument has no non-default active field");
            return ZR_FALSE;
        }
        return zr_ffi_build_scalar_argument(
                state,
                activeValue,
                activeField->type,
                buffer + activeField->offset,
                errorBuffer,
                errorBufferSize);
    }

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

/** @brief 只让声明了 pointer lowering 的包装对象进入原生地址提取路径。 */
static TZrBool zr_ffi_object_uses_pointer_lowering(SZrState *state, SZrObject *object) {
    const char *loweringKind = ZR_NULL;

    if (state == ZR_NULL || object == ZR_NULL || object->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_ffi_read_object_string_field(state, &object->prototype->super, "__zr_ffiLoweringKind", &loweringKind) &&
           loweringKind != ZR_NULL &&
           strcmp(loweringKind, "pointer") == 0;
}

/** @brief 从类型原型读取 handle_id lowering 契约，避免普通对象被当作整数句柄。 */
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

/** @brief 为 pointer 实参借用 PointerHandle/BufferHandle 的原生地址。
 * @note 返回地址不转移所有权；调用方须保证包装对象及其底层资源在原生调用期间存活。
 */
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

/** @brief 从未关闭的句柄包装对象取得被隐藏字段或公开 handleId 承载的整数值。 */
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

/** @brief 仅在原型声明的底层整数类型与形参布局一致时拆解 handle_id 包装。 */
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

/** @brief 为 SymbolHandle.call、ref/inout 及回调返回值构造 libffi 所需的原生存储。
 * @note buffer 属于调用方；指针与字符串路径只借用底层资源，使用期由调用方维持。
 */
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
        /* BUG: 数值类型只验证“可提取”，未按目标整数范围或有限性校验；
         * 如 i32 形参收到 1e300，随后浮点转整数超出 C 可表示范围。 */
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
        /* BUG: 类型描述符接受 utf16/ansi，但这里始终借用 VM 的 char* 文本；
         * 目标 C API 按 utf16/ansi 解释时会收到错误字节，需按 encoding 转码并管理临时缓冲区。 */
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

    snprintf(errorBuffer,
             errorBufferSize,
             "unsupported value type %d for ffi argument",
             value != ZR_NULL ? (int)value->type : -1);
    return ZR_FALSE;
}

/** @brief 在 VM 的 TryRun 异常边界内调用回调值，并把结果交还 libffi trampoline。 */
void zr_ffi_callback_try_invoke(SZrState *state, TZrPtr arguments) {
    ZrFfiCallbackInvokeArgs *invokeArgs = (ZrFfiCallbackInvokeArgs *)arguments;

    if (state == ZR_NULL || invokeArgs == ZR_NULL || invokeArgs->result == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetNull(invokeArgs->result);
    invokeArgs->succeeded =
            ZrLib_CallValue(state, invokeArgs->callbackValue, ZR_NULL, invokeArgs->argumentValues, invokeArgs->argumentCount, invokeArgs->result);
}

/** @brief 将原生聚合值变为 VM 对象，供原生返回、ref/out 写回及回调形参复用。
 * @note 构造期间以临时根保护对象；返回后由调用方负责维持结果的 GC 可达性。
 */
TZrBool zr_ffi_struct_to_object(SZrState *state, ZrFfiTypeLayout *type, const unsigned char *bytes,
                                       SZrTypeValue *result) {
    TZrSize index;
    SZrObject *object;
    ZrLibTempValueRoot objectRoot;

    if (!ZrLib_TempValueRoot_Begin(state, &objectRoot)) {
        return ZR_FALSE;
    }
    object = type->name != ZR_NULL ? ZrLib_Type_NewInstance(state, type->name) : ZR_NULL;
    if (object == ZR_NULL) {
        object = ZrLib_Object_New(state);
    }
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
        /* BUG: SetFieldCString 在分配/pin 失败时静默返回，此处仍交付缺字段的聚合对象；
         * struct 返回、ref/out 写回及回调形参均受影响，须用分配失败注入核验错误传播。 */
        ZrLib_Object_SetFieldCString(state, object, field->name, &fieldValue);
    }

    ZrLib_Value_SetObject(state, result, object, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&objectRoot);
    return ZR_TRUE;
}

/** @brief 将 libffi 返回存储或回调形参解码为 VM 值；调用方负责原生缓冲区的生命周期。 */
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
            /* BUG: 大于 INT64_MAX 的 u64 被强制转为有符号整数，原生无符号结果失真；
             * 本路径由原生返回、ref/out 写回及回调形参共同调用。 */
            ZrLib_Value_SetInt(state, result, (TZrInt64) * (const uint64_t *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_F32:
            ZrLib_Value_SetFloat(state, result, *(const float *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_F64:
            ZrLib_Value_SetFloat(state, result, *(const double *) value);
            return ZR_TRUE;
        case ZR_FFI_TYPE_STRING:
            /* BUG: 描述符可指定 utf16/ansi，但原生结果始终按 char* 交给 VM 字符串接口；
             * 指向 UTF-16 数据的结果会在首个零字节处截断。 */
            ZrLib_Value_SetString(state, result,
                                  *(const char *const *) value != ZR_NULL ? *(const char *const *) value : "");
            return ZR_TRUE;
        case ZR_FFI_TYPE_ENUM:
            return zr_ffi_set_result_from_scalar(state, type->as.enumType.underlying, value, result);
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            return zr_ffi_struct_to_object(state, type, (const unsigned char *) value, result);
        default:
            /* BUG: pointer/function 类型可进入原生返回或回调形参契约，
             * 这里缺少反向转换，调用链只能以封送失败结束。 */
            return ZR_FALSE;
    }
}

/** @brief SymbolHandle 终结时归还库的符号引用，并兑现库此前的关闭请求。 */
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

/** @brief PointerHandle 关闭或终结时归还 BufferHandle 的借用计数并兑现延期释放。 */
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

/** @brief VM GC 对各类 FFI Handle 的统一终结入口，释放签名、闭包及原生资源。
 * @note 显式关闭后的资源状态由各分支识别；finalized 状态阻止重复执行终结器。
 */
void zr_ffi_handle_finalize(SZrState *state, SZrRawObject *rawObject) {
    SZrObject *object;
    ZrFfiHandleData *handleData;

    if (rawObject == ZR_NULL) {
        return;
    }
    object = ZR_CAST_OBJECT(state, rawObject);
    handleData = (ZrFfiHandleData *)rawObject->finalizerData;

    if (handleData == ZR_NULL || handleData->finalized) {
        return;
    }
    rawObject->finalizerData = ZR_NULL;
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
            if (!pointerData->closed) {
                zr_ffi_pointer_release_owner(state, object);
            }
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
/** @brief libffi 闭包进入 VM 的同步回调边界。
 * @note 由 ffi_prep_closure_loc 注册；只允许所有者线程及有效调用期，异常须在返回原生栈前化为策略约定的结果。
 */
void zr_ffi_callback_trampoline(ffi_cif *cif, void *returnValue, void **arguments, void *userData) {
    ZrFfiCallbackData *callbackData = (ZrFfiCallbackData *) userData;
    const SZrTypeValue *callbackValue;
    SZrTypeValue *argumentValues = ZR_NULL;
    SZrTypeValue callResult;
    ZrFfiCallbackInvokeArgs invokeArgs;
    SZrCallInfo *savedCallInfo = ZR_NULL;
    TZrStackValuePointer savedStackTop;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    TZrBool hasSavedCallInfoBase = ZR_FALSE;
    TZrBool hasSavedCallInfoTop = ZR_FALSE;
    TZrBool hasSavedCallInfoReturn = ZR_FALSE;
    TZrUInt32 savedExceptionHandlerStackLength;
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

    /* call 生命周期由外层 zr_ffi_symbol_invoke_array 暂时激活；
     * 原生代码留存闭包并在外层返回后再调用时，必须拒绝进入 VM。 */
    if (callbackData->activeLifetime ==
                ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL &&
        !callbackData->invocationActive) {
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(
                callbackData->lastErrorMessage,
                sizeof(callbackData->lastErrorMessage),
                "callback invoked outside its call lifetime");
        zr_ffi_zero_call_storage(
                callbackData->signature->returnType, returnValue);
        return;
    }

#if defined(ZR_PLATFORM_WIN)
    if (callbackData->activeThreadPolicy ==
            ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN ||
        GetCurrentThreadId() != callbackData->ownerThreadId) {
#else
    if (callbackData->activeThreadPolicy ==
            ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN ||
        !pthread_equal(pthread_self(), callbackData->ownerThreadId)) {
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

    /* TODO: 聚合形参转换后仅存于 calloc 的 argumentValues，临时根已经结束；
     * 后续对象分配失败可经 GcMalloc 进入 GcFull。核对 GC 是否扫描该数组，
     * 并用多聚合形参与强制收集验证前序参数在进入 CallValue 前仍可达。 */
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

    /* 回调可重入正在执行的 VM；锚点记录外层栈位置，避免回调期间栈扩容后
     * 使用失效指针，并在 TryRun 后恢复原生调用现场。 */
    savedCallInfo = callbackData->state->callInfoList;
    savedStackTop = callbackData->state->stackTop.valuePointer;
    savedExceptionHandlerStackLength =
            callbackData->state->exceptionHandlerStackLength;
    ZrCore_Function_StackAnchorInit(
            callbackData->state, savedStackTop, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                callbackData->state,
                savedCallInfo->functionBase.valuePointer,
                &savedCallInfoBaseAnchor);
        hasSavedCallInfoBase = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                callbackData->state,
                savedCallInfo->functionTop.valuePointer,
                &savedCallInfoTopAnchor);
        hasSavedCallInfoTop = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->hasReturnDestination &&
        savedCallInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                callbackData->state,
                savedCallInfo->returnDestination,
                &savedCallInfoReturnAnchor);
        hasSavedCallInfoReturn = ZR_TRUE;
    }
    invokeArgs.callbackValue = callbackValue;
    invokeArgs.argumentValues = argumentValues;
    invokeArgs.argumentCount = callbackData->signature->parameterCount;
    invokeArgs.result = &callResult;
    invokeArgs.succeeded = ZR_FALSE;
    callbackStatus = ZrCore_Exception_TryRun(callbackData->state, zr_ffi_callback_try_invoke, &invokeArgs);
    savedStackTop = ZrCore_Function_StackAnchorRestore(
            callbackData->state, &savedStackTopAnchor);
    callbackData->state->stackTop.valuePointer = savedStackTop;
    callbackData->state->callInfoList = savedCallInfo;
    if (savedCallInfo != ZR_NULL) {
        if (hasSavedCallInfoBase) {
            savedCallInfo->functionBase.valuePointer =
                    ZrCore_Function_StackAnchorRestore(
                            callbackData->state,
                            &savedCallInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            savedCallInfo->functionTop.valuePointer =
                    ZrCore_Function_StackAnchorRestore(
                            callbackData->state,
                            &savedCallInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            savedCallInfo->returnDestination =
                    ZrCore_Function_StackAnchorRestore(
                            callbackData->state,
                            &savedCallInfoReturnAnchor);
        }
    }
    callbackData->state->exceptionHandlerStackLength =
            savedExceptionHandlerStackLength;
    /* VM 异常不能跨过 C/libffi 栈传播；此处按回调异常策略记录失败、
     * 清理线程异常状态，并给原生调用者一个零值返回。 */
    if (callbackStatus != ZR_THREAD_STATUS_FINE || !invokeArgs.succeeded) {
        callbackData->callbackExceptionObserved = ZR_TRUE;
        if (callbackData->activeExceptionPolicy ==
            ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ABORT) {
            abort();
        }
        callbackData->lastError = ZR_FFI_ERROR_NATIVE_CALL;
        snprintf(callbackData->lastErrorMessage,
                 sizeof(callbackData->lastErrorMessage),
                 "zr callback execution failed%s",
                 callbackStatus != ZR_THREAD_STATUS_FINE ? " with VM exception" : "");
        ZrCore_Exception_ClearCurrent(callbackData->state);
        callbackData->state->threadStatus = ZR_THREAD_STATUS_FINE;
        callbackData->state->pendingControl.kind =
                ZR_VM_PENDING_CONTROL_NONE;
        callbackData->state->pendingControl.callInfo = ZR_NULL;
        callbackData->state->pendingControl.targetInstructionOffset = 0u;
        callbackData->state->pendingControl.valueSlot = 0u;
        ZrCore_Value_ResetAsNull(
                &callbackData->state->pendingControl.value);
        callbackData->state->pendingControl.hasValue = ZR_FALSE;
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

    /* BUG: void 回调签名可被解析，但这里仍调用不支持 VOID 的 build_scalar_argument；
     * 成功执行的 void 回调会被记为封送失败，需让 void 直接完成返回。 */
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
