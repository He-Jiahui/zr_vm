#include <string.h>

#include "execution_internal.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/type_layout.h"

#define ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE UINT32_MAX

TZrBool execution_inline_frame_try_get_member_by_name_to_slot(SZrState *state,
                                                              const SZrFunction *function,
                                                              TZrStackValuePointer frameBase,
                                                              TZrUInt32 receiverSlot,
                                                              SZrString *memberName,
                                                              TZrUInt32 destinationSlot,
                                                              SZrTypeValue *result);
TZrBool execution_inline_frame_try_set_member_by_name_from_slot(SZrState *state,
                                                                const SZrFunction *function,
                                                                TZrStackValuePointer frameBase,
                                                                TZrUInt32 receiverSlot,
                                                                SZrString *memberName,
                                                                TZrUInt32 sourceSlot,
                                                                const SZrTypeValue *assignedValue);
TZrBool execution_inline_frame_try_get_member_to_slot(SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrStackValuePointer frameBase,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt16 cacheIndex,
                                                      TZrUInt32 destinationSlot,
                                                      SZrTypeValue *result);
TZrBool execution_inline_frame_try_set_member_from_slot(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrStackValuePointer frameBase,
                                                        TZrUInt32 receiverSlot,
                                                        TZrUInt16 cacheIndex,
                                                        TZrUInt32 sourceSlot,
                                                        const SZrTypeValue *assignedValue);
static TZrBool execution_inline_frame_copy_slot_to_nested_field(SZrState *state,
                                                               const SZrFunction *function,
                                                               TZrStackValuePointer frameBase,
                                                               TZrUInt32 sourceSlot,
                                                               const SZrFunctionFrameFieldLayout *fieldLayout,
                                                               TZrByte *fieldAddress);

static SZrString *execution_inline_frame_refresh_string(SZrString *stringValue) {
    SZrRawObject *rawObject;
    SZrRawObject *forwardedObject;

    if (stringValue == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue);
    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL) {
        return stringValue;
    }

    return ZR_CAST_STRING(ZR_NULL, forwardedObject);
}

static SZrString *execution_inline_frame_resolve_cached_member_name(const SZrFunction *function,
                                                                    TZrUInt16 cacheIndex,
                                                                    EZrFunctionCallSiteCacheKind expectedKind) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;
    SZrFunction *mutableFunction = (SZrFunction *)function;
    SZrString *memberName;

    if (mutableFunction == ZR_NULL) {
        return ZR_NULL;
    }

    cacheEntry = execution_get_callsite_cache_entry(mutableFunction, cacheIndex, expectedKind);
    if (cacheEntry == ZR_NULL ||
        mutableFunction->memberEntries == ZR_NULL ||
        cacheEntry->memberEntryIndex >= mutableFunction->memberEntryLength) {
        return ZR_NULL;
    }

    memberName = execution_inline_frame_refresh_string(mutableFunction->memberEntries[cacheEntry->memberEntryIndex].symbol);
    mutableFunction->memberEntries[cacheEntry->memberEntryIndex].symbol = memberName;
    return memberName;
}

static TZrBool execution_inline_frame_slot_is_inline_struct(const SZrFunctionFrameSlotLayout *slotLayout) {
    return (TZrBool)(slotLayout != ZR_NULL &&
                     slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     slotLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     slotLayout->byteSize > 0u);
}

SZrTypeValue *execution_inline_frame_get_value_slot(SZrState *state,
                                                    const SZrFunction *function,
                                                    TZrStackValuePointer frameBase,
                                                    TZrUInt32 stackSlot) {
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    if (frameBase == ZR_NULL) {
        return ZR_NULL;
    }
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL || function->frameSlotLayoutLength == 0u) {
        return &frameBase[stackSlot].value;
    }

    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    if (slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
        slotLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
        !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, stackSlot, &place)) {
        return &frameBase[stackSlot].value;
    }

    return (SZrTypeValue *)place.address;
}

static TZrBool execution_inline_frame_make_place(SZrState *state,
                                                 const SZrFunction *function,
                                                 TZrStackValuePointer frameBase,
                                                 const SZrFunctionFrameSlotLayout *slotLayout,
                                                 SZrStackFramePlace *outPlace) {
    if (!execution_inline_frame_slot_is_inline_struct(slotLayout)) {
        return ZR_FALSE;
    }

    return ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, outPlace);
}

static const SZrTypeLayout *execution_inline_frame_resolve_type_layout(
        SZrState *state,
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *slotLayout) {
    const SZrTypeLayout *typeLayout;

    if (!execution_inline_frame_slot_is_inline_struct(slotLayout)) {
        return ZR_NULL;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, slotLayout->typeLayoutId, state);
    if (typeLayout == ZR_NULL ||
        (typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
         typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION) ||
        typeLayout->byteSize != slotLayout->byteSize) {
        return ZR_NULL;
    }

    return typeLayout;
}

static TZrBool execution_inline_frame_value_is_object_struct(const SZrTypeValue *value, SZrObject **outObject) {
    SZrObject *object;

    if (outObject != ZR_NULL) {
        *outObject = ZR_NULL;
    }
    if (value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(ZR_NULL, value->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_STRUCT) {
        return ZR_FALSE;
    }
    if (outObject != ZR_NULL) {
        *outObject = object;
    }
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_load_signed_int(const TZrByte *address,
                                                      TZrUInt32 byteSize,
                                                      TZrInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8): {
            TZrInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt16): {
            TZrInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt32): {
            TZrInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt64): {
            TZrInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_load_unsigned_int(const TZrByte *address,
                                                        TZrUInt32 byteSize,
                                                        TZrUInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt64): {
            TZrUInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_store_signed_int(TZrByte *address,
                                                       TZrUInt32 byteSize,
                                                       TZrInt64 value) {
    if (address == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8): {
            TZrInt8 storedValue = (TZrInt8)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt16): {
            TZrInt16 storedValue = (TZrInt16)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt32): {
            TZrInt32 storedValue = (TZrInt32)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt64): {
            TZrInt64 storedValue = value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_store_unsigned_int(TZrByte *address,
                                                         TZrUInt32 byteSize,
                                                         TZrUInt64 value) {
    if (address == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 storedValue = (TZrUInt8)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 storedValue = (TZrUInt16)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 storedValue = (TZrUInt32)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt64): {
            TZrUInt64 storedValue = value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_load_primitive_field(SZrState *state,
                                                          const SZrFunctionFrameFieldLayout *fieldLayout,
                                                          const TZrByte *fieldAddress,
                                                          SZrTypeValue *result) {
    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod || fieldAddress == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (fieldLayout->valueType) {
        ZR_VALUE_CASES_SIGNED_INT {
            TZrInt64 value;
            if (!execution_inline_frame_load_signed_int(fieldAddress, fieldLayout->byteSize, &value)) {
                return ZR_FALSE;
            }
            ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
        ZR_VALUE_CASES_UNSIGNED_INT {
            TZrUInt64 value;
            if (!execution_inline_frame_load_unsigned_int(fieldAddress, fieldLayout->byteSize, &value)) {
                return ZR_FALSE;
            }
            ZR_VALUE_FAST_SET(result, nativeUInt64, value, ZR_VALUE_TYPE_UINT64);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_BOOL: {
            TZrBool value;
            if (fieldLayout->byteSize != sizeof(value)) {
                return ZR_FALSE;
            }
            memcpy(&value, fieldAddress, sizeof(value));
            ZrCore_Value_InitAsBool(state, result, value);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_FLOAT: {
            TZrFloat32 value;
            if (fieldLayout->byteSize != sizeof(value)) {
                return ZR_FALSE;
            }
            memcpy(&value, fieldAddress, sizeof(value));
            ZR_VALUE_FAST_SET(result, nativeDouble, (TZrDouble)value, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_DOUBLE: {
            TZrDouble value;
            if (fieldLayout->byteSize != sizeof(value)) {
                return ZR_FALSE;
            }
            memcpy(&value, fieldAddress, sizeof(value));
            ZR_VALUE_FAST_SET(result, nativeDouble, value, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_store_primitive_field(const SZrFunctionFrameFieldLayout *fieldLayout,
                                                           TZrByte *fieldAddress,
                                                           const SZrTypeValue *value) {
    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod || fieldAddress == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (fieldLayout->valueType) {
        ZR_VALUE_CASES_SIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
                return execution_inline_frame_store_signed_int(fieldAddress,
                                                               fieldLayout->byteSize,
                                                               (TZrInt64)value->value.nativeObject.nativeUInt64);
            }
            return execution_inline_frame_store_signed_int(fieldAddress,
                                                           fieldLayout->byteSize,
                                                           value->value.nativeObject.nativeInt64);
        ZR_VALUE_CASES_UNSIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
                return execution_inline_frame_store_unsigned_int(fieldAddress,
                                                                 fieldLayout->byteSize,
                                                                 (TZrUInt64)value->value.nativeObject.nativeInt64);
            }
            return execution_inline_frame_store_unsigned_int(fieldAddress,
                                                             fieldLayout->byteSize,
                                                             value->value.nativeObject.nativeUInt64);
        case ZR_VALUE_TYPE_BOOL:
            if (value->type != ZR_VALUE_TYPE_BOOL || fieldLayout->byteSize != sizeof(TZrBool)) {
                return ZR_FALSE;
            }
            {
                TZrBool storedValue = value->value.nativeObject.nativeBool != 0u;
                memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT: {
            TZrFloat32 storedValue;
            if (!ZR_VALUE_IS_TYPE_FLOAT(value->type) || fieldLayout->byteSize != sizeof(storedValue)) {
                return ZR_FALSE;
            }
            storedValue = (TZrFloat32)value->value.nativeObject.nativeDouble;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_DOUBLE:
            if (!ZR_VALUE_IS_TYPE_FLOAT(value->type) || fieldLayout->byteSize != sizeof(TZrDouble)) {
                return ZR_FALSE;
            }
            memcpy(fieldAddress, &value->value.nativeObject.nativeDouble, sizeof(TZrDouble));
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_load_value_slot_field(SZrState *state,
                                                            const SZrFunctionFrameFieldLayout *fieldLayout,
                                                            const TZrByte *fieldAddress,
                                                            SZrTypeValue *result) {
    if (fieldLayout == ZR_NULL ||
        !fieldLayout->isValueSlot ||
        fieldLayout->byteSize < sizeof(SZrTypeValue) ||
        fieldAddress == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, (const SZrTypeValue *)fieldAddress);
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_store_value_slot_field(SZrState *state,
                                                             const SZrFunctionFrameFieldLayout *fieldLayout,
                                                             TZrByte *fieldAddress,
                                                             const SZrTypeValue *value) {
    if (fieldLayout == ZR_NULL ||
        !fieldLayout->isValueSlot ||
        fieldLayout->byteSize < sizeof(SZrTypeValue) ||
        fieldAddress == ZR_NULL ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, (SZrTypeValue *)fieldAddress, value);
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_load_field(SZrState *state,
                                                 const SZrFunction *function,
                                                 const SZrFunctionFrameFieldLayout *fieldLayout,
                                                 const TZrByte *fieldAddress,
                                                 SZrTypeValue *result) {
    if (fieldLayout != ZR_NULL &&
        fieldLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZrCore_Function_CopyInlineStorageToObjectValue(state,
                                                              function,
                                                              fieldLayout->typeLayoutId,
                                                              fieldAddress,
                                                              fieldLayout->byteSize,
                                                              result);
    }

    if (execution_inline_frame_load_primitive_field(state, fieldLayout, fieldAddress, result)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_load_value_slot_field(state, fieldLayout, fieldAddress, result);
}

static TZrBool execution_inline_frame_store_field(SZrState *state,
                                                  const SZrFunctionFrameFieldLayout *fieldLayout,
                                                  TZrByte *fieldAddress,
                                                  const SZrTypeValue *value) {
    if (execution_inline_frame_store_primitive_field(fieldLayout, fieldAddress, value)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_store_value_slot_field(state, fieldLayout, fieldAddress, value);
}

static TZrBool execution_inline_frame_store_nested_object_field(SZrState *state,
                                                                const SZrFunction *function,
                                                                const SZrFunctionFrameFieldLayout *fieldLayout,
                                                                TZrByte *fieldAddress,
                                                                const SZrTypeValue *value) {
    if (fieldLayout == ZR_NULL ||
        fieldLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        fieldAddress == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Function_CopyObjectValueToInlineStorage(state,
                                                          function,
                                                          fieldLayout->typeLayoutId,
                                                          fieldAddress,
                                                          fieldLayout->byteSize,
                                                          value);
}

static TZrBool execution_inline_frame_materialize_union_carrier_to_inline_storage(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId,
        TZrPtr destinationAddress,
        TZrUInt32 destinationByteSize,
        SZrObject *carrier);

static TZrBool execution_inline_frame_store_field_or_nested_object(
        SZrState *state,
        const SZrFunction *function,
        const SZrFunctionFrameFieldLayout *fieldLayout,
        TZrByte *fieldAddress,
        const SZrTypeValue *value) {
    if (fieldLayout != ZR_NULL &&
        fieldLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
        value != ZR_NULL &&
        value->type == ZR_VALUE_TYPE_OBJECT &&
        value->value.object != ZR_NULL &&
        execution_inline_frame_materialize_union_carrier_to_inline_storage(
                state,
                function,
                fieldLayout->typeLayoutId,
                fieldAddress,
                fieldLayout->byteSize,
                ZR_CAST_OBJECT(state, value->value.object))) {
        return ZR_TRUE;
    }

    if (execution_inline_frame_store_nested_object_field(state, function, fieldLayout, fieldAddress, value)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_store_field(state, fieldLayout, fieldAddress, value);
}

typedef struct SZrExecutionInlineFramePrototypeRecord {
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
} SZrExecutionInlineFramePrototypeRecord;

static TZrBool execution_inline_frame_checked_add_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }

    *out = left + right;
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_checked_mul_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || (right != 0u && left > ZR_MAX_SIZE / right)) {
        return ZR_FALSE;
    }

    *out = left * right;
    return ZR_TRUE;
}

static const SZrFunction *execution_inline_frame_entry_function(const SZrFunction *function) {
    const SZrFunction *entryFunction = function;
    const SZrFunction *prototypeContextFunction;

    if (entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    while (entryFunction->ownerFunction != ZR_NULL) {
        entryFunction = entryFunction->ownerFunction;
    }

    if (entryFunction->prototypeData != ZR_NULL &&
        entryFunction->prototypeDataLength >= sizeof(TZrUInt32) &&
        entryFunction->prototypeCount > 0u) {
        return entryFunction;
    }

    prototypeContextFunction = function->prototypeContextFunction;
    if (prototypeContextFunction != ZR_NULL &&
        prototypeContextFunction->prototypeData != ZR_NULL &&
        prototypeContextFunction->prototypeDataLength >= sizeof(TZrUInt32) &&
        prototypeContextFunction->prototypeCount > 0u) {
        return prototypeContextFunction;
    }

    return entryFunction;
}

static TZrBool execution_inline_frame_find_prototype_record(
        const SZrFunction *function,
        TZrUInt32 targetIndex,
        SZrExecutionInlineFramePrototypeRecord *outRecord) {
    const TZrByte *data;
    TZrUInt32 encodedPrototypeCount;
    TZrSize cursor;

    if (outRecord != ZR_NULL) {
        memset(outRecord, 0, sizeof(*outRecord));
    }
    if (function == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(TZrUInt32) ||
        targetIndex >= function->prototypeCount) {
        return ZR_FALSE;
    }

    memcpy(&encodedPrototypeCount, function->prototypeData, sizeof(encodedPrototypeCount));
    if (encodedPrototypeCount < function->prototypeCount) {
        return ZR_FALSE;
    }

    data = function->prototypeData;
    cursor = sizeof(TZrUInt32);
    for (TZrUInt32 index = 0u; index < function->prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrCompiledMemberInfo *members;
        TZrSize inheritsBytes;
        TZrSize decoratorsBytes;
        TZrSize membersBytes;
        TZrSize afterPrototype;
        TZrSize afterInherits;
        TZrSize membersOffset;
        TZrSize nextCursor;

        if (!execution_inline_frame_checked_add_size(cursor, sizeof(SZrCompiledPrototypeInfo), &afterPrototype) ||
            afterPrototype > function->prototypeDataLength) {
            return ZR_FALSE;
        }

        prototype = (const SZrCompiledPrototypeInfo *)(data + cursor);
        if (!execution_inline_frame_checked_mul_size(prototype->inheritsCount, sizeof(TZrUInt32), &inheritsBytes) ||
            !execution_inline_frame_checked_mul_size(prototype->decoratorsCount, sizeof(TZrUInt32), &decoratorsBytes) ||
            !execution_inline_frame_checked_mul_size(prototype->membersCount,
                                                     sizeof(SZrCompiledMemberInfo),
                                                     &membersBytes) ||
            !execution_inline_frame_checked_add_size(afterPrototype, inheritsBytes, &afterInherits) ||
            !execution_inline_frame_checked_add_size(afterInherits, decoratorsBytes, &membersOffset) ||
            !execution_inline_frame_checked_add_size(membersOffset, membersBytes, &nextCursor) ||
            nextCursor > function->prototypeDataLength) {
            return ZR_FALSE;
        }

        members = (const SZrCompiledMemberInfo *)(data + membersOffset);
        if (index == targetIndex) {
            if (outRecord != ZR_NULL) {
                outRecord->prototype = prototype;
                outRecord->members = members;
            }
            return ZR_TRUE;
        }
        cursor = nextCursor;
    }

    return ZR_FALSE;
}

static SZrString *execution_inline_frame_function_string_constant(const SZrFunction *function,
                                                                  TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[constantIndex];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(ZR_NULL, constant->value.object);
}

static TZrBool execution_inline_frame_text_equals(const TZrChar *left, const TZrChar *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(strcmp(left, right) == 0);
}

static TZrBool execution_inline_frame_union_carrier_type_matches_prototype(SZrString *carrierType,
                                                                           SZrString *prototypeName) {
    const TZrChar *carrierText;
    const TZrChar *prototypeText;
    const TZrChar *genericStart;
    TZrSize baseLength;

    if (carrierType == ZR_NULL || prototypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrCore_String_Equal(carrierType, prototypeName)) {
        return ZR_TRUE;
    }

    carrierText = ZrCore_String_GetNativeString(carrierType);
    prototypeText = ZrCore_String_GetNativeString(prototypeName);
    genericStart = carrierText != ZR_NULL ? strchr(carrierText, '<') : ZR_NULL;
    if (carrierText == ZR_NULL || prototypeText == ZR_NULL || genericStart == ZR_NULL) {
        return ZR_FALSE;
    }

    baseLength = (TZrSize)(genericStart - carrierText);
    while (baseLength > 0u &&
           (carrierText[baseLength - 1u] == ' ' ||
            carrierText[baseLength - 1u] == '\t' ||
            carrierText[baseLength - 1u] == '\r' ||
            carrierText[baseLength - 1u] == '\n')) {
        baseLength--;
    }

    return (TZrBool)(baseLength > 0u &&
                     strlen(prototypeText) == baseLength &&
                     strncmp(prototypeText, carrierText, baseLength) == 0);
}

static TZrUInt32 execution_inline_frame_select_union_tag_size(TZrUInt32 variantCount) {
    if (variantCount <= 0xffu) {
        return 1u;
    }
    if (variantCount <= 0xffffu) {
        return 2u;
    }
    return 4u;
}

static TZrBool execution_inline_frame_parse_union_payload_member_index(SZrString *memberName,
                                                                       TZrUInt32 *outIndex) {
    const TZrChar *text;
    const TZrChar *prefix = "__zr_unionPayload";
    TZrSize prefixLength;
    TZrUInt32 value = 0u;

    if (outIndex != ZR_NULL) {
        *outIndex = 0u;
    }
    if (memberName == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(memberName);
    prefixLength = strlen(prefix);
    if (text == ZR_NULL || strncmp(text, prefix, prefixLength) != 0 || text[prefixLength] == '\0') {
        return ZR_FALSE;
    }

    for (const TZrChar *cursor = text + prefixLength; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return ZR_FALSE;
        }
        if (value > (UINT32_MAX - (TZrUInt32)(*cursor - '0')) / 10u) {
            return ZR_FALSE;
        }
        value = value * 10u + (TZrUInt32)(*cursor - '0');
    }

    *outIndex = value;
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_is_union_variant_member_name(SZrString *memberName) {
    return execution_inline_frame_text_equals(ZrCore_String_GetNativeString(memberName), "__zr_unionVariant");
}

static SZrString *execution_inline_frame_carrier_string_member(SZrState *state,
                                                               SZrObject *carrier,
                                                               const TZrChar *name) {
    SZrString *memberName;
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || carrier == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    memberName = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (memberName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, carrier, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, value->value.object);
}

static const SZrTypeValue *execution_inline_frame_carrier_member_value(SZrState *state,
                                                                       SZrObject *carrier,
                                                                       const TZrChar *name) {
    SZrString *memberName;
    SZrTypeValue key;

    if (state == ZR_NULL || carrier == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    memberName = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (memberName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, carrier, &key);
}

static TZrBool execution_inline_frame_store_union_tag(TZrByte *storage,
                                                      TZrUInt32 byteSize,
                                                      TZrUInt32 tag) {
    if (storage == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 stored = (TZrUInt8)tag;
            memcpy(storage, &stored, sizeof(stored));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 stored = (TZrUInt16)tag;
            memcpy(storage, &stored, sizeof(stored));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32):
            memcpy(storage, &tag, sizeof(tag));
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_inline_frame_object_int_field(SZrState *state,
                                                       SZrObject *object,
                                                       const TZrChar *name,
                                                       TZrUInt32 *outValue) {
    const SZrTypeValue *value;

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = execution_inline_frame_carrier_member_value(state, object, name);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }

    *outValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)
                        ? (TZrUInt32)value->value.nativeObject.nativeInt64
                        : (TZrUInt32)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static SZrObject *execution_inline_frame_object_array_field(SZrState *state,
                                                            SZrObject *object,
                                                            const TZrChar *name) {
    const SZrTypeValue *value = execution_inline_frame_carrier_member_value(state, object, name);

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *execution_inline_frame_array_object_at(SZrState *state, SZrObject *arrayObject, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, arrayObject, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrBool execution_inline_frame_union_payload_pod_value_type(const TZrChar *typeNameText,
                                                                   EZrValueType *outValueType,
                                                                   TZrUInt32 *outSize) {
    EZrValueType valueType = ZR_VALUE_TYPE_UNKNOWN;
    TZrUInt32 byteSize = 0u;

    if (outValueType != ZR_NULL) {
        *outValueType = ZR_VALUE_TYPE_UNKNOWN;
    }
    if (outSize != ZR_NULL) {
        *outSize = 0u;
    }
    if (typeNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_inline_frame_text_equals(typeNameText, "i8") ||
        execution_inline_frame_text_equals(typeNameText, "int8")) {
        valueType = ZR_VALUE_TYPE_INT8;
        byteSize = (TZrUInt32)sizeof(TZrInt8);
    } else if (execution_inline_frame_text_equals(typeNameText, "i16") ||
               execution_inline_frame_text_equals(typeNameText, "int16")) {
        valueType = ZR_VALUE_TYPE_INT16;
        byteSize = (TZrUInt32)sizeof(TZrInt16);
    } else if (execution_inline_frame_text_equals(typeNameText, "i32") ||
               execution_inline_frame_text_equals(typeNameText, "int32")) {
        valueType = ZR_VALUE_TYPE_INT32;
        byteSize = (TZrUInt32)sizeof(TZrInt32);
    } else if (execution_inline_frame_text_equals(typeNameText, "int") ||
               execution_inline_frame_text_equals(typeNameText, "i64") ||
               execution_inline_frame_text_equals(typeNameText, "int64")) {
        valueType = ZR_VALUE_TYPE_INT64;
        byteSize = (TZrUInt32)sizeof(TZrInt64);
    } else if (execution_inline_frame_text_equals(typeNameText, "u8") ||
               execution_inline_frame_text_equals(typeNameText, "uint8")) {
        valueType = ZR_VALUE_TYPE_UINT8;
        byteSize = (TZrUInt32)sizeof(TZrUInt8);
    } else if (execution_inline_frame_text_equals(typeNameText, "u16") ||
               execution_inline_frame_text_equals(typeNameText, "uint16")) {
        valueType = ZR_VALUE_TYPE_UINT16;
        byteSize = (TZrUInt32)sizeof(TZrUInt16);
    } else if (execution_inline_frame_text_equals(typeNameText, "u32") ||
               execution_inline_frame_text_equals(typeNameText, "uint32")) {
        valueType = ZR_VALUE_TYPE_UINT32;
        byteSize = (TZrUInt32)sizeof(TZrUInt32);
    } else if (execution_inline_frame_text_equals(typeNameText, "uint") ||
               execution_inline_frame_text_equals(typeNameText, "u64") ||
               execution_inline_frame_text_equals(typeNameText, "uint64")) {
        valueType = ZR_VALUE_TYPE_UINT64;
        byteSize = (TZrUInt32)sizeof(TZrUInt64);
    } else if (execution_inline_frame_text_equals(typeNameText, "float") ||
               execution_inline_frame_text_equals(typeNameText, "f32")) {
        valueType = ZR_VALUE_TYPE_FLOAT;
        byteSize = (TZrUInt32)sizeof(TZrFloat32);
    } else if (execution_inline_frame_text_equals(typeNameText, "double") ||
               execution_inline_frame_text_equals(typeNameText, "f64") ||
               execution_inline_frame_text_equals(typeNameText, "f")) {
        valueType = ZR_VALUE_TYPE_DOUBLE;
        byteSize = (TZrUInt32)sizeof(TZrDouble);
    } else if (execution_inline_frame_text_equals(typeNameText, "bool")) {
        valueType = ZR_VALUE_TYPE_BOOL;
        byteSize = (TZrUInt32)sizeof(TZrBool);
    } else {
        return ZR_FALSE;
    }

    if (outValueType != ZR_NULL) {
        *outValueType = valueType;
    }
    if (outSize != ZR_NULL) {
        *outSize = byteSize;
    }
    return ZR_TRUE;
}

static TZrBool execution_inline_frame_union_payload_field_layout(SZrState *state,
                                                                 SZrObject *fieldMetadata,
                                                                 SZrFunctionFrameFieldLayout *outLayout) {
    SZrString *typeName;
    const TZrChar *typeNameText;
    EZrValueType valueType;
    TZrUInt32 expectedSize;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;

    if (outLayout != ZR_NULL) {
        memset(outLayout, 0, sizeof(*outLayout));
    }
    if (state == ZR_NULL ||
        fieldMetadata == ZR_NULL ||
        outLayout == ZR_NULL ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteOffset", &byteOffset) ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteSize", &byteSize) ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteAlign", &byteAlign)) {
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(byteAlign);
    outLayout->byteOffset = byteOffset;
    outLayout->byteSize = byteSize;
    outLayout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    outLayout->valueType = ZR_VALUE_TYPE_UNKNOWN;

    typeName = execution_inline_frame_carrier_string_member(state, fieldMetadata, "type");
    typeNameText = typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : ZR_NULL;
    if (execution_inline_frame_union_payload_pod_value_type(typeNameText, &valueType, &expectedSize) &&
        expectedSize == byteSize) {
        outLayout->valueType = valueType;
        outLayout->isPrimitivePod = ZR_TRUE;
        return ZR_TRUE;
    }

    if (byteSize >= sizeof(SZrTypeValue)) {
        outLayout->isValueSlot = ZR_TRUE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool execution_inline_frame_store_union_payload_value(SZrState *state,
                                                                TZrByte *storage,
                                                                TZrUInt32 storageSize,
                                                                SZrObject *fieldMetadata,
                                                                const SZrTypeValue *payloadValue) {
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;

    if (state == ZR_NULL ||
        storage == ZR_NULL ||
        fieldMetadata == ZR_NULL ||
        payloadValue == ZR_NULL ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteOffset", &byteOffset) ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteSize", &byteSize) ||
        !execution_inline_frame_object_int_field(state, fieldMetadata, "byteAlign", &byteAlign) ||
        byteOffset > storageSize ||
        byteSize > storageSize - byteOffset) {
        return ZR_FALSE;
    }

    if (!execution_inline_frame_union_payload_field_layout(state, fieldMetadata, &fieldLayout)) {
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(byteAlign);
    return execution_inline_frame_store_field(state, &fieldLayout, storage + byteOffset, payloadValue);
}

static void execution_inline_frame_consume_union_carrier_ownership_payload(SZrState *state,
                                                                          SZrObject *fieldMetadata,
                                                                          const SZrTypeValue *payloadValue) {
    if (state == ZR_NULL ||
        fieldMetadata == ZR_NULL ||
        payloadValue == ZR_NULL ||
        payloadValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE) {
        return;
    }

    ZrCore_Ownership_ReleaseValue(state, (SZrTypeValue *)payloadValue);
}

static TZrBool execution_inline_frame_materialize_union_carrier_to_inline_storage(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId,
        TZrPtr destinationAddress,
        TZrUInt32 destinationByteSize,
        SZrObject *carrier) {
    const SZrFunction *entryFunction;
    SZrExecutionInlineFramePrototypeRecord record;
    SZrString *carrierType;
    SZrString *carrierVariant;
    SZrString *prototypeName;
    const SZrCompiledMemberInfo *variantMember = ZR_NULL;
    SZrObject *variantMetadata;
    SZrObject *payloadFields;
    const SZrTypeLayout *destinationTypeLayout;
    TZrUInt32 tagSize;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        destinationAddress == ZR_NULL ||
        carrier == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    entryFunction = execution_inline_frame_entry_function(function);
    if (!execution_inline_frame_find_prototype_record(entryFunction, typeLayoutId, &record) ||
        record.prototype == ZR_NULL ||
        record.members == ZR_NULL ||
        record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        record.prototype->layoutByteSize == 0u ||
        record.prototype->layoutByteSize > destinationByteSize) {
        return ZR_FALSE;
    }

    carrierType = execution_inline_frame_carrier_string_member(state, carrier, "__zr_unionType");
    carrierVariant = execution_inline_frame_carrier_string_member(state, carrier, "__zr_unionVariant");
    prototypeName = execution_inline_frame_function_string_constant(entryFunction, record.prototype->nameStringIndex);
    if (carrierType == ZR_NULL ||
        carrierVariant == ZR_NULL ||
        prototypeName == ZR_NULL ||
        !execution_inline_frame_union_carrier_type_matches_prototype(carrierType, prototypeName)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < record.prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record.members[index];
        SZrString *memberName;

        if (member->memberType != ZR_AST_CONSTANT_UNION_VARIANT || member->nameStringIndex == 0u) {
            continue;
        }

        memberName = execution_inline_frame_function_string_constant(entryFunction, member->nameStringIndex);
        if (memberName != ZR_NULL && ZrCore_String_Equal(memberName, carrierVariant)) {
            variantMember = member;
            break;
        }
    }

    if (variantMember == ZR_NULL ||
        !variantMember->hasDecoratorMetadata ||
        variantMember->decoratorMetadataConstantIndex >= entryFunction->constantValueLength) {
        return ZR_FALSE;
    }

    {
        const SZrTypeValue *metadataValue = &entryFunction->constantValueList[variantMember->decoratorMetadataConstantIndex];
        if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
            return ZR_FALSE;
        }
        variantMetadata = ZR_CAST_OBJECT(state, metadataValue->value.object);
    }

    if (!execution_inline_frame_object_int_field(state, variantMetadata, "tagSize", &tagSize) ||
        tagSize == 0u ||
        tagSize > destinationByteSize) {
        return ZR_FALSE;
    }

    destinationTypeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    if (destinationTypeLayout == ZR_NULL ||
        destinationTypeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION) {
        return ZR_FALSE;
    }

    ZrCore_TypeLayout_DropInline(state, destinationTypeLayout, destinationAddress);
    memset(destinationAddress, 0, destinationByteSize);
    if (!execution_inline_frame_store_union_tag((TZrByte *)destinationAddress,
                                                tagSize,
                                                variantMember->fieldOffset)) {
        return ZR_FALSE;
    }

    payloadFields = execution_inline_frame_object_array_field(state, variantMetadata, "payloadFields");
    for (TZrUInt32 index = 0u; index < variantMember->parameterCount; index++) {
        TZrChar payloadName[64];
        SZrObject *fieldMetadata;
        const SZrTypeValue *payloadValue;

        if (snprintf(payloadName, sizeof(payloadName), "__zr_unionPayload%u", (unsigned)index) <= 0) {
            return ZR_FALSE;
        }

        fieldMetadata = execution_inline_frame_array_object_at(state, payloadFields, index);
        payloadValue = execution_inline_frame_carrier_member_value(state, carrier, payloadName);
        if (!execution_inline_frame_store_union_payload_value(state,
                                                             (TZrByte *)destinationAddress,
                                                             destinationByteSize,
                                                             fieldMetadata,
                                                             payloadValue)) {
            return ZR_FALSE;
        }
        execution_inline_frame_consume_union_carrier_ownership_payload(state, fieldMetadata, payloadValue);
    }

    return ZR_TRUE;
}

static TZrBool execution_inline_frame_materialize_union_carrier_to_inline(
        SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const SZrFunctionFrameSlotLayout *destinationLayout,
        SZrObject *carrier) {
    SZrStackFramePlace destinationPlace;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        frameBase == ZR_NULL ||
        destinationLayout == ZR_NULL ||
        carrier == ZR_NULL ||
        destinationLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        !execution_inline_frame_make_place(state, function, frameBase, destinationLayout, &destinationPlace)) {
        return ZR_FALSE;
    }

    return execution_inline_frame_materialize_union_carrier_to_inline_storage(
            state,
            function,
            destinationLayout->typeLayoutId,
            destinationPlace.address,
            destinationPlace.byteSize,
            carrier);
}

static TZrBool execution_inline_frame_materialize_union_carrier_value_to_inline(
        SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const SZrFunctionFrameSlotLayout *destinationLayout,
        const SZrTypeValue *carrierValue) {
    if (carrierValue == ZR_NULL ||
        carrierValue->type != ZR_VALUE_TYPE_OBJECT ||
        carrierValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    return execution_inline_frame_materialize_union_carrier_to_inline(
            state,
            function,
            frameBase,
            destinationLayout,
            ZR_CAST_OBJECT(state, carrierValue->value.object));
}

static TZrBool execution_inline_frame_find_union_variant_by_tag(
        const SZrExecutionInlineFramePrototypeRecord *record,
        const TZrByte *storage,
        TZrUInt32 tagSize,
        const SZrCompiledMemberInfo **outVariantMember) {
    TZrUInt64 tagValue;

    if (outVariantMember != ZR_NULL) {
        *outVariantMember = ZR_NULL;
    }
    if (record == ZR_NULL ||
        record->prototype == ZR_NULL ||
        record->members == ZR_NULL ||
        storage == ZR_NULL ||
        outVariantMember == ZR_NULL ||
        !execution_inline_frame_load_unsigned_int(storage, tagSize, &tagValue)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < record->prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record->members[index];
        if (member->memberType == ZR_AST_CONSTANT_UNION_VARIANT &&
            (TZrUInt64)member->fieldOffset == tagValue) {
            *outVariantMember = member;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool execution_inline_frame_union_variant_metadata(
        SZrState *state,
        const SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *variantMember,
        SZrObject **outMetadata) {
    const SZrTypeValue *metadataValue;

    if (outMetadata != ZR_NULL) {
        *outMetadata = ZR_NULL;
    }
    if (state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        variantMember == ZR_NULL ||
        outMetadata == ZR_NULL ||
        !variantMember->hasDecoratorMetadata ||
        variantMember->decoratorMetadataConstantIndex >= entryFunction->constantValueLength) {
        return ZR_FALSE;
    }

    metadataValue = &entryFunction->constantValueList[variantMember->decoratorMetadataConstantIndex];
    if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outMetadata = ZR_CAST_OBJECT(state, metadataValue->value.object);
    return (TZrBool)(*outMetadata != ZR_NULL);
}

static TZrBool execution_inline_frame_load_union_payload_member_to_slot(
        SZrState *state,
        const SZrFunction *function,
        const SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *variantMember,
        const SZrStackFramePlace *receiverPlace,
        TZrUInt32 payloadIndex,
        SZrTypeValue *result) {
    SZrObject *variantMetadata;
    SZrObject *payloadFields;
    SZrObject *fieldMetadata;
    SZrFunctionFrameFieldLayout fieldLayout;
    const TZrByte *fieldAddress;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        entryFunction == ZR_NULL ||
        variantMember == ZR_NULL ||
        receiverPlace == ZR_NULL ||
        receiverPlace->address == ZR_NULL ||
        result == ZR_NULL ||
        payloadIndex >= variantMember->parameterCount) {
        return ZR_FALSE;
    }
    if (!execution_inline_frame_union_variant_metadata(state, entryFunction, variantMember, &variantMetadata)) {
        return ZR_FALSE;
    }

    payloadFields = execution_inline_frame_object_array_field(state, variantMetadata, "payloadFields");
    fieldMetadata = execution_inline_frame_array_object_at(state, payloadFields, payloadIndex);
    if (!execution_inline_frame_union_payload_field_layout(state, fieldMetadata, &fieldLayout)) {
        return ZR_FALSE;
    }
    if (fieldLayout.byteOffset > receiverPlace->byteSize ||
        fieldLayout.byteSize > receiverPlace->byteSize - fieldLayout.byteOffset) {
        return ZR_FALSE;
    }

    fieldAddress = (const TZrByte *)receiverPlace->address + fieldLayout.byteOffset;
    return execution_inline_frame_load_field(state, function, &fieldLayout, fieldAddress, result);
}

static TZrBool execution_inline_frame_store_union_payload_member_from_slot(
        SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *variantMember,
        const SZrStackFramePlace *receiverPlace,
        TZrUInt32 payloadIndex,
        TZrUInt32 sourceSlot,
        const SZrTypeValue *assignedValue) {
    SZrObject *variantMetadata;
    SZrObject *payloadFields;
    SZrObject *fieldMetadata;
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrByte *fieldAddress;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        frameBase == ZR_NULL ||
        entryFunction == ZR_NULL ||
        variantMember == ZR_NULL ||
        receiverPlace == ZR_NULL ||
        receiverPlace->address == ZR_NULL ||
        assignedValue == ZR_NULL ||
        payloadIndex >= variantMember->parameterCount) {
        return ZR_FALSE;
    }
    if (!execution_inline_frame_union_variant_metadata(state, entryFunction, variantMember, &variantMetadata)) {
        return ZR_FALSE;
    }

    payloadFields = execution_inline_frame_object_array_field(state, variantMetadata, "payloadFields");
    fieldMetadata = execution_inline_frame_array_object_at(state, payloadFields, payloadIndex);
    if (!execution_inline_frame_union_payload_field_layout(state, fieldMetadata, &fieldLayout)) {
        return ZR_FALSE;
    }
    if (fieldLayout.byteOffset > receiverPlace->byteSize ||
        fieldLayout.byteSize > receiverPlace->byteSize - fieldLayout.byteOffset) {
        return ZR_FALSE;
    }

    fieldAddress = (TZrByte *)receiverPlace->address + fieldLayout.byteOffset;
    if (execution_inline_frame_copy_slot_to_nested_field(state,
                                                         function,
                                                         frameBase,
                                                         sourceSlot,
                                                         &fieldLayout,
                                                         fieldAddress)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_store_field_or_nested_object(state,
                                                              function,
                                                              &fieldLayout,
                                                              fieldAddress,
                                                              assignedValue);
}

static TZrBool execution_inline_frame_try_get_union_member_to_slot(
        SZrState *state,
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *receiverLayout,
        const SZrStackFramePlace *receiverPlace,
        SZrString *memberName,
        SZrTypeValue *result) {
    const SZrFunction *entryFunction;
    SZrExecutionInlineFramePrototypeRecord record;
    const SZrCompiledMemberInfo *variantMember;
    TZrUInt32 tagSize;
    TZrUInt32 payloadIndex;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        receiverLayout == ZR_NULL ||
        receiverPlace == ZR_NULL ||
        receiverPlace->address == ZR_NULL ||
        memberName == ZR_NULL ||
        result == ZR_NULL ||
        receiverLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    entryFunction = execution_inline_frame_entry_function(function);
    if (!execution_inline_frame_find_prototype_record(entryFunction, receiverLayout->typeLayoutId, &record) ||
        record.prototype == ZR_NULL ||
        record.members == ZR_NULL ||
        record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        record.prototype->layoutByteSize == 0u ||
        record.prototype->layoutByteSize > receiverPlace->byteSize) {
        return ZR_FALSE;
    }

    tagSize = execution_inline_frame_select_union_tag_size(record.prototype->membersCount);
    if (tagSize == 0u ||
        tagSize > receiverPlace->byteSize ||
        !execution_inline_frame_find_union_variant_by_tag(&record,
                                                          (const TZrByte *)receiverPlace->address,
                                                          tagSize,
                                                          &variantMember)) {
        return ZR_FALSE;
    }

    if (execution_inline_frame_is_union_variant_member_name(memberName)) {
        SZrString *variantName = execution_inline_frame_function_string_constant(entryFunction,
                                                                                variantMember->nameStringIndex);
        if (variantName == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(variantName));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    if (execution_inline_frame_parse_union_payload_member_index(memberName, &payloadIndex)) {
        return execution_inline_frame_load_union_payload_member_to_slot(state,
                                                                       function,
                                                                       entryFunction,
                                                                       variantMember,
                                                                       receiverPlace,
                                                                       payloadIndex,
                                                                       result);
    }

    return ZR_FALSE;
}

static TZrBool execution_inline_frame_try_set_union_member_from_slot(
        SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const SZrFunctionFrameSlotLayout *receiverLayout,
        const SZrStackFramePlace *receiverPlace,
        SZrString *memberName,
        TZrUInt32 sourceSlot,
        const SZrTypeValue *assignedValue) {
    const SZrFunction *entryFunction;
    SZrExecutionInlineFramePrototypeRecord record;
    const SZrCompiledMemberInfo *variantMember;
    TZrUInt32 tagSize;
    TZrUInt32 payloadIndex;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        frameBase == ZR_NULL ||
        receiverLayout == ZR_NULL ||
        receiverPlace == ZR_NULL ||
        receiverPlace->address == ZR_NULL ||
        memberName == ZR_NULL ||
        assignedValue == ZR_NULL ||
        receiverLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    entryFunction = execution_inline_frame_entry_function(function);
    if (!execution_inline_frame_find_prototype_record(entryFunction, receiverLayout->typeLayoutId, &record) ||
        record.prototype == ZR_NULL ||
        record.members == ZR_NULL ||
        record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        record.prototype->layoutByteSize == 0u ||
        record.prototype->layoutByteSize > receiverPlace->byteSize) {
        return ZR_FALSE;
    }

    tagSize = execution_inline_frame_select_union_tag_size(record.prototype->membersCount);
    if (tagSize == 0u ||
        tagSize > receiverPlace->byteSize ||
        !execution_inline_frame_find_union_variant_by_tag(&record,
                                                          (const TZrByte *)receiverPlace->address,
                                                          tagSize,
                                                          &variantMember)) {
        return ZR_FALSE;
    }

    if (!execution_inline_frame_parse_union_payload_member_index(memberName, &payloadIndex)) {
        return ZR_FALSE;
    }

    return execution_inline_frame_store_union_payload_member_from_slot(state,
                                                                      function,
                                                                      frameBase,
                                                                      entryFunction,
                                                                      variantMember,
                                                                      receiverPlace,
                                                                      payloadIndex,
                                                                      sourceSlot,
                                                                      assignedValue);
}

static void execution_inline_frame_sync_physical_struct_field(SZrState *state,
                                                             TZrStackValuePointer frameBase,
                                                             TZrUInt32 receiverSlot,
                                                             SZrString *memberName,
                                                             const SZrTypeValue *assignedValue) {
    SZrObject *receiverObject;
    SZrTypeValue key;

    if (state == ZR_NULL ||
        frameBase == ZR_NULL ||
        memberName == ZR_NULL ||
        assignedValue == ZR_NULL ||
        !execution_inline_frame_value_is_object_struct(&frameBase[receiverSlot].value, &receiverObject)) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, receiverObject, &key, assignedValue);
}

static TZrBool execution_inline_frame_materialize_inline_storage_to_object_value(SZrState *state,
                                                                                 const SZrFunction *function,
                                                                                 TZrUInt32 typeLayoutId,
                                                                                 const TZrByte *sourceAddress,
                                                                                 TZrUInt32 sourceByteSize,
                                                                                 SZrTypeValue *outValue) {
    return ZrCore_Function_CopyInlineStorageToObjectValue(state,
                                                          function,
                                                          typeLayoutId,
                                                          sourceAddress,
                                                          sourceByteSize,
                                                          outValue);
}

TZrBool execution_inline_frame_try_materialize_stack_slot_value(SZrState *state,
                                                               const SZrFunction *function,
                                                               TZrStackValuePointer frameBase,
                                                               TZrUInt32 sourceSlot,
                                                               SZrTypeValue *outValue) {
    const SZrFunctionFrameSlotLayout *sourceLayout;
    SZrStackFramePlace sourcePlace;

    if (state == ZR_NULL || function == ZR_NULL || frameBase == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceLayout = ZrCore_Function_FindFrameSlotLayout(function, sourceSlot);
    if (!execution_inline_frame_make_place(state, function, frameBase, sourceLayout, &sourcePlace)) {
        return ZR_FALSE;
    }

    return execution_inline_frame_materialize_inline_storage_to_object_value(state,
                                                                            function,
                                                                            sourceLayout->typeLayoutId,
                                                                            (const TZrByte *)sourcePlace.address,
                                                                            sourcePlace.byteSize,
                                                                            outValue);
}

static TZrBool execution_inline_frame_copy_nested_field_to_slot(SZrState *state,
                                                               const SZrFunction *function,
                                                               TZrStackValuePointer frameBase,
                                                               TZrUInt32 destinationSlot,
                                                               const SZrFunctionFrameFieldLayout *fieldLayout,
                                                               const TZrByte *fieldAddress) {
    const SZrFunctionFrameSlotLayout *destinationLayout;
    const SZrTypeLayout *typeLayout;
    SZrStackFramePlace destinationPlace;

    if (destinationSlot == ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE ||
        fieldLayout == ZR_NULL ||
        fieldLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        fieldAddress == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationLayout = ZrCore_Function_FindFrameSlotLayout(function, destinationSlot);
    if (!execution_inline_frame_slot_is_inline_struct(destinationLayout) ||
        destinationLayout->typeLayoutId != fieldLayout->typeLayoutId) {
        return ZR_FALSE;
    }

    typeLayout = execution_inline_frame_resolve_type_layout(state, function, destinationLayout);
    if (typeLayout == ZR_NULL ||
        typeLayout->byteSize > fieldLayout->byteSize ||
        typeLayout->byteSize != destinationLayout->byteSize ||
        !execution_inline_frame_make_place(state, function, frameBase, destinationLayout, &destinationPlace)) {
        return ZR_FALSE;
    }

    return ZrCore_TypeLayout_CopyInline(state, typeLayout, destinationPlace.address, fieldAddress);
}

static TZrBool execution_inline_frame_copy_slot_to_nested_field(SZrState *state,
                                                               const SZrFunction *function,
                                                               TZrStackValuePointer frameBase,
                                                               TZrUInt32 sourceSlot,
                                                               const SZrFunctionFrameFieldLayout *fieldLayout,
                                                               TZrByte *fieldAddress) {
    const SZrFunctionFrameSlotLayout *sourceLayout;
    const SZrTypeLayout *typeLayout;
    SZrStackFramePlace sourcePlace;

    if (sourceSlot == ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE ||
        fieldLayout == ZR_NULL ||
        fieldLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        fieldAddress == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceLayout = ZrCore_Function_FindFrameSlotLayout(function, sourceSlot);
    if (!execution_inline_frame_slot_is_inline_struct(sourceLayout) ||
        sourceLayout->typeLayoutId != fieldLayout->typeLayoutId) {
        return ZR_FALSE;
    }

    typeLayout = execution_inline_frame_resolve_type_layout(state, function, sourceLayout);
    if (typeLayout == ZR_NULL ||
        typeLayout->byteSize > fieldLayout->byteSize ||
        typeLayout->byteSize != sourceLayout->byteSize ||
        !execution_inline_frame_make_place(state, function, frameBase, sourceLayout, &sourcePlace)) {
        return ZR_FALSE;
    }

    return ZrCore_TypeLayout_CopyInline(state, typeLayout, fieldAddress, sourcePlace.address);
}

static TZrBool execution_inline_frame_copy_inline_to_inline(SZrState *state,
                                                           const SZrFunction *function,
                                                           TZrStackValuePointer frameBase,
                                                           const SZrFunctionFrameSlotLayout *destinationLayout,
                                                           const SZrFunctionFrameSlotLayout *sourceLayout) {
    const SZrTypeLayout *typeLayout;

    if (!execution_inline_frame_slot_is_inline_struct(destinationLayout) ||
        !execution_inline_frame_slot_is_inline_struct(sourceLayout) ||
        destinationLayout->typeLayoutId != sourceLayout->typeLayoutId) {
        return ZR_FALSE;
    }

    typeLayout = execution_inline_frame_resolve_type_layout(state, function, destinationLayout);
    if (typeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Function_CopyFrameSlotInline(state,
                                               typeLayout,
                                               function,
                                               frameBase,
                                               destinationLayout->stackSlot,
                                               function,
                                               frameBase,
                                               sourceLayout->stackSlot);
}

static TZrBool execution_inline_frame_materialize_object_to_inline(SZrState *state,
                                                                  const SZrFunction *function,
                                                                  TZrStackValuePointer frameBase,
                                                                  const SZrFunctionFrameSlotLayout *destinationLayout,
                                                                  SZrObject *sourceObject) {
    SZrStackFramePlace destinationPlace;
    SZrTypeValue sourceValue;

    if (sourceObject == ZR_NULL ||
        sourceObject->prototype == ZR_NULL ||
        !execution_inline_frame_make_place(state, function, frameBase, destinationLayout, &destinationPlace)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    sourceValue.type = ZR_VALUE_TYPE_OBJECT;
    return ZrCore_Function_CopyObjectValueToInlineStorage(state,
                                                          function,
                                                          destinationLayout->typeLayoutId,
                                                          destinationPlace.address,
                                                          destinationPlace.byteSize,
                                                          &sourceValue);
}

TZrBool execution_inline_frame_try_copy_stack_slot(SZrState *state,
                                                   const SZrFunction *function,
                                                   TZrStackValuePointer frameBase,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    const SZrFunctionFrameSlotLayout *destinationLayout;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    SZrStackFramePlace sourcePlace;
    SZrObject *sourceObject;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourcePhysicalValue;
    SZrTypeValue *sourceValue;
    SZrTypeValue materializedValue;

    if (state == ZR_NULL || function == ZR_NULL || frameBase == ZR_NULL) {
        return ZR_FALSE;
    }
    if (destinationSlot == ZR_INSTRUCTION_USE_RET_FLAG || sourceSlot == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }
    destinationLayout = ZrCore_Function_FindFrameSlotLayout(function, destinationSlot);
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(function, sourceSlot);
    destinationValue = execution_inline_frame_get_value_slot(state, function, frameBase, destinationSlot);
    sourcePhysicalValue = &frameBase[sourceSlot].value;
    sourceValue = execution_inline_frame_get_value_slot(state, function, frameBase, sourceSlot);

    if (execution_inline_frame_slot_is_inline_struct(destinationLayout)) {
        if (execution_inline_frame_materialize_union_carrier_value_to_inline(state,
                                                                             function,
                                                                             frameBase,
                                                                             destinationLayout,
                                                                             sourcePhysicalValue) ||
            execution_inline_frame_materialize_union_carrier_value_to_inline(state,
                                                                             function,
                                                                             frameBase,
                                                                             destinationLayout,
                                                                             sourceValue)) {
            ZrCore_Value_ResetAsNull(destinationValue);
            ZrCore_Value_ResetAsNull(sourcePhysicalValue);
            if (sourceValue != sourcePhysicalValue) {
                ZrCore_Value_ResetAsNull(sourceValue);
            }
            return ZR_TRUE;
        }

        if (execution_inline_frame_copy_inline_to_inline(state, function, frameBase, destinationLayout, sourceLayout)) {
            ZrCore_Value_ResetAsNull(destinationValue);
            return ZR_TRUE;
        }

        if (execution_inline_frame_value_is_object_struct(sourcePhysicalValue, &sourceObject) ||
            execution_inline_frame_value_is_object_struct(sourceValue, &sourceObject)) {
            if (!execution_inline_frame_materialize_object_to_inline(state,
                                                                    function,
                                                                    frameBase,
                                                                    destinationLayout,
                                                                    sourceObject)) {
                return ZR_FALSE;
            }
            ZrCore_Value_ResetAsNull(destinationValue);
            return ZR_TRUE;
        }

        return ZR_FALSE;
    }

    if (execution_inline_frame_make_place(state, function, frameBase, sourceLayout, &sourcePlace)) {
        if (sourceValue != ZR_NULL &&
            !ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
            ZrCore_Value_Copy(state, destinationValue, sourceValue);
            return ZR_TRUE;
        }
        if (sourcePhysicalValue != ZR_NULL &&
            sourcePhysicalValue != sourceValue &&
            !ZR_VALUE_IS_TYPE_NULL(sourcePhysicalValue->type)) {
            ZrCore_Value_Copy(state, destinationValue, sourcePhysicalValue);
            return ZR_TRUE;
        }

        ZrCore_Value_ResetAsNull(&materializedValue);
        if (execution_inline_frame_materialize_inline_storage_to_object_value(state,
                                                                             function,
                                                                             sourceLayout->typeLayoutId,
                                                                             (const TZrByte *)sourcePlace.address,
                                                                             sourcePlace.byteSize,
                                                                             &materializedValue)) {
            ZrCore_Value_Copy(state, destinationValue, &materializedValue);
            ZrCore_Value_ResetAsNull(&materializedValue);
            return ZR_TRUE;
        }

        if (execution_inline_frame_value_is_object_struct(sourcePhysicalValue, &sourceObject)) {
            ZrCore_Value_Copy(state, destinationValue, sourcePhysicalValue);
            return ZR_TRUE;
        }
        if (execution_inline_frame_value_is_object_struct(sourceValue, &sourceObject)) {
            ZrCore_Value_Copy(state, destinationValue, sourceValue);
            return ZR_TRUE;
        }
        if (sourcePhysicalValue != ZR_NULL &&
            sourcePhysicalValue != sourceValue &&
            !ZR_VALUE_IS_TYPE_NULL(sourcePhysicalValue->type)) {
            ZrCore_Value_Copy(state, destinationValue, sourcePhysicalValue);
            return ZR_TRUE;
        }

        return ZR_FALSE;
    }

    return ZR_FALSE;
}

TZrBool execution_inline_frame_try_get_member_by_name(SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrStackValuePointer frameBase,
                                                      TZrUInt32 receiverSlot,
                                                      SZrString *memberName,
                                                      SZrTypeValue *result) {
    return execution_inline_frame_try_get_member_by_name_to_slot(state,
                                                                 function,
                                                                 frameBase,
                                                                 receiverSlot,
                                                                 memberName,
                                                                 ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE,
                                                                 result);
}

TZrBool execution_inline_frame_try_get_member_by_name_to_slot(SZrState *state,
                                                              const SZrFunction *function,
                                                              TZrStackValuePointer frameBase,
                                                              TZrUInt32 receiverSlot,
                                                              SZrString *memberName,
                                                              TZrUInt32 destinationSlot,
                                                              SZrTypeValue *result) {
    const SZrFunctionFrameSlotLayout *receiverLayout;
    SZrFunctionFrameFieldLayout fieldLayout;
    SZrStackFramePlace receiverPlace;
    const TZrByte *fieldAddress;

    if (state == ZR_NULL || function == ZR_NULL || frameBase == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverLayout = ZrCore_Function_FindFrameSlotLayout(function, receiverSlot);
    if (!execution_inline_frame_make_place(state, function, frameBase, receiverLayout, &receiverPlace)) {
        return ZR_FALSE;
    }

    if (execution_inline_frame_try_get_union_member_to_slot(state,
                                                            function,
                                                            receiverLayout,
                                                            &receiverPlace,
                                                            memberName,
                                                            result)) {
        return ZR_TRUE;
    }

    if (!ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
                                                          function,
                                                          receiverLayout->typeLayoutId,
                                                          memberName,
                                                          &fieldLayout) ||
        fieldLayout.byteOffset > receiverPlace.byteSize ||
        fieldLayout.byteSize > receiverPlace.byteSize - fieldLayout.byteOffset) {
        return ZR_FALSE;
    }

    fieldAddress = (const TZrByte *)receiverPlace.address + fieldLayout.byteOffset;
    if (execution_inline_frame_copy_nested_field_to_slot(state,
                                                         function,
                                                         frameBase,
                                                         destinationSlot,
                                                         &fieldLayout,
                                                         fieldAddress)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_load_field(state, function, &fieldLayout, fieldAddress, result);
}

TZrBool execution_inline_frame_try_set_member_by_name(SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrStackValuePointer frameBase,
                                                      TZrUInt32 receiverSlot,
                                                      SZrString *memberName,
                                                      const SZrTypeValue *assignedValue) {
    return execution_inline_frame_try_set_member_by_name_from_slot(state,
                                                                   function,
                                                                   frameBase,
                                                                   receiverSlot,
                                                                   memberName,
                                                                   ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE,
                                                                   assignedValue);
}

TZrBool execution_inline_frame_try_set_member_by_name_from_slot(SZrState *state,
                                                                const SZrFunction *function,
                                                                TZrStackValuePointer frameBase,
                                                                TZrUInt32 receiverSlot,
                                                                SZrString *memberName,
                                                                TZrUInt32 sourceSlot,
                                                                const SZrTypeValue *assignedValue) {
    const SZrFunctionFrameSlotLayout *receiverLayout;
    SZrFunctionFrameFieldLayout fieldLayout;
    SZrStackFramePlace receiverPlace;
    TZrByte *fieldAddress;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        frameBase == ZR_NULL ||
        memberName == ZR_NULL ||
        assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverLayout = ZrCore_Function_FindFrameSlotLayout(function, receiverSlot);
    if (!execution_inline_frame_make_place(state, function, frameBase, receiverLayout, &receiverPlace)) {
        return ZR_FALSE;
    }

    if (execution_inline_frame_try_set_union_member_from_slot(state,
                                                             function,
                                                             frameBase,
                                                             receiverLayout,
                                                             &receiverPlace,
                                                             memberName,
                                                             sourceSlot,
                                                             assignedValue)) {
        return ZR_TRUE;
    }

    if (!ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
                                                         function,
                                                         receiverLayout->typeLayoutId,
                                                         memberName,
                                                         &fieldLayout) ||
        fieldLayout.byteOffset > receiverPlace.byteSize ||
        fieldLayout.byteSize > receiverPlace.byteSize - fieldLayout.byteOffset) {
        return ZR_FALSE;
    }

    fieldAddress = (TZrByte *)receiverPlace.address + fieldLayout.byteOffset;
    if (execution_inline_frame_copy_slot_to_nested_field(state,
                                                         function,
                                                         frameBase,
                                                         sourceSlot,
                                                         &fieldLayout,
                                                         fieldAddress)) {
        execution_inline_frame_sync_physical_struct_field(state, frameBase, receiverSlot, memberName, assignedValue);
        return ZR_TRUE;
    }

    if (!execution_inline_frame_store_field_or_nested_object(state, function, &fieldLayout, fieldAddress, assignedValue)) {
        return ZR_FALSE;
    }
    execution_inline_frame_sync_physical_struct_field(state, frameBase, receiverSlot, memberName, assignedValue);
    return ZR_TRUE;
}

TZrBool execution_inline_frame_try_get_member(SZrState *state,
                                              const SZrFunction *function,
                                              TZrStackValuePointer frameBase,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt16 cacheIndex,
                                              SZrTypeValue *result) {
    return execution_inline_frame_try_get_member_to_slot(state,
                                                        function,
                                                        frameBase,
                                                        receiverSlot,
                                                        cacheIndex,
                                                        ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE,
                                                        result);
}

TZrBool execution_inline_frame_try_get_member_to_slot(SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrStackValuePointer frameBase,
                                                      TZrUInt32 receiverSlot,
                                                      TZrUInt16 cacheIndex,
                                                      TZrUInt32 destinationSlot,
                                                      SZrTypeValue *result) {
    SZrString *memberName = execution_inline_frame_resolve_cached_member_name(
            function, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);

    return execution_inline_frame_try_get_member_by_name_to_slot(
            state, function, frameBase, receiverSlot, memberName, destinationSlot, result);
}

TZrBool execution_inline_frame_try_set_member(SZrState *state,
                                              const SZrFunction *function,
                                              TZrStackValuePointer frameBase,
                                              TZrUInt32 receiverSlot,
                                              TZrUInt16 cacheIndex,
                                              const SZrTypeValue *assignedValue) {
    return execution_inline_frame_try_set_member_from_slot(state,
                                                          function,
                                                          frameBase,
                                                          receiverSlot,
                                                          cacheIndex,
                                                          ZR_EXECUTION_INLINE_FRAME_STACK_SLOT_NONE,
                                                          assignedValue);
}

TZrBool execution_inline_frame_try_set_member_from_slot(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrStackValuePointer frameBase,
                                                        TZrUInt32 receiverSlot,
                                                        TZrUInt16 cacheIndex,
                                                        TZrUInt32 sourceSlot,
                                                        const SZrTypeValue *assignedValue) {
    SZrString *memberName = execution_inline_frame_resolve_cached_member_name(
            function, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET);

    return execution_inline_frame_try_set_member_by_name_from_slot(
            state, function, frameBase, receiverSlot, memberName, sourceSlot, assignedValue);
}
