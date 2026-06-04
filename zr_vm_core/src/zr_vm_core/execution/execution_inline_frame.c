#include "execution_internal.h"

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
        typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT ||
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

static TZrBool execution_inline_frame_store_field_or_nested_object(
        SZrState *state,
        const SZrFunction *function,
        const SZrFunctionFrameFieldLayout *fieldLayout,
        TZrByte *fieldAddress,
        const SZrTypeValue *value) {
    if (execution_inline_frame_store_nested_object_field(state, function, fieldLayout, fieldAddress, value)) {
        return ZR_TRUE;
    }

    return execution_inline_frame_store_field(state, fieldLayout, fieldAddress, value);
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
        if (execution_inline_frame_value_is_object_struct(sourcePhysicalValue, &sourceObject)) {
            ZrCore_Value_Copy(state, destinationValue, sourcePhysicalValue);
            return ZR_TRUE;
        }
        if (execution_inline_frame_value_is_object_struct(sourceValue, &sourceObject)) {
            ZrCore_Value_Copy(state, destinationValue, sourceValue);
            return ZR_TRUE;
        }

        ZrCore_Value_ResetAsNull(&materializedValue);
        if (!execution_inline_frame_materialize_inline_storage_to_object_value(state,
                                                                              function,
                                                                              sourceLayout->typeLayoutId,
                                                                              (const TZrByte *)sourcePlace.address,
                                                                              sourcePlace.byteSize,
                                                                              &materializedValue)) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, destinationValue, &materializedValue);
        ZrCore_Value_ResetAsNull(&materializedValue);
        return ZR_TRUE;
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
    if (!execution_inline_frame_make_place(state, function, frameBase, receiverLayout, &receiverPlace) ||
        !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
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
    if (!execution_inline_frame_make_place(state, function, frameBase, receiverLayout, &receiverPlace) ||
        !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
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
