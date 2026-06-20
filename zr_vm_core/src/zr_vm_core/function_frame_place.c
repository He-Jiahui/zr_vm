//
// Created by Codex on 2026/5/17.
//

#include "zr_vm_core/function.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"

#include <string.h>

static TZrSize function_frame_byte_storage_slot_count(TZrUInt32 byteSize) {
    TZrSize slotByteSize = sizeof(SZrTypeValueOnStack);

    if (byteSize == 0u) {
        return 0u;
    }

    return ((TZrSize)byteSize + slotByteSize - 1u) / slotByteSize;
}

TZrSize ZrCore_Function_GetFrameStorageSlotCount(const SZrFunction *function) {
    TZrSize storageSlotCount;
    TZrSize byteStorageSlotCount;

    if (function == ZR_NULL) {
        return 0u;
    }

    storageSlotCount = ZrCore_Function_GetGeneratedFrameSlotCount(function);
    byteStorageSlotCount = function_frame_byte_storage_slot_count(function->frameByteSize);
    return byteStorageSlotCount > storageSlotCount ? byteStorageSlotCount : storageSlotCount;
}

TZrBool ZrCore_Function_FrameStackSlotIntersectsInlineStruct(const SZrFunction *function,
                                                             TZrStackValuePointer frameBase,
                                                             TZrStackValuePointer stackSlot) {
    const TZrByte *slotStart;
    const TZrByte *slotEnd;

    if (function == ZR_NULL || frameBase == ZR_NULL || stackSlot == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    slotStart = (const TZrByte *)stackSlot;
    slotEnd = slotStart + sizeof(SZrTypeValueOnStack);
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        const TZrByte *inlineStart;
        const TZrByte *inlineEnd;

        if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            slotLayout->byteSize == 0u) {
            continue;
        }

        inlineStart = (const TZrByte *)frameBase + slotLayout->byteOffset;
        inlineEnd = inlineStart + slotLayout->byteSize;
        if (slotStart < inlineEnd && inlineStart < slotEnd) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Function_FrameStackSlotIntersectsFrameGcValue(const SZrFunction *function,
                                                             TZrStackValuePointer frameBase,
                                                             TZrStackValuePointer stackSlot) {
    const TZrByte *slotStart;
    const TZrByte *slotEnd;

    if (function == ZR_NULL || frameBase == ZR_NULL || stackSlot == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    slotStart = (const TZrByte *)stackSlot;
    slotEnd = slotStart + sizeof(SZrTypeValueOnStack);
    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        const TZrByte *layoutStart;
        const TZrByte *layoutEnd;

        if (slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE) {
            if (slotLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
                continue;
            }
        } else if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                   slotLayout->byteSize == 0u) {
            continue;
        }

        layoutStart = (const TZrByte *)frameBase + slotLayout->byteOffset;
        layoutEnd = layoutStart + slotLayout->byteSize;
        if (slotStart < layoutEnd && layoutStart < slotEnd) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_frame_slot_matches_layout_kind(const SZrFunctionFrameSlotLayout *slotLayout,
                                                       const SZrTypeLayout *layout) {
    if (slotLayout == ZR_NULL || layout == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrTypeLayoutKind)layout->kind) {
        case ZR_TYPE_LAYOUT_KIND_VALUE:
            return (TZrBool)(slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE);
        case ZR_TYPE_LAYOUT_KIND_STRUCT:
        case ZR_TYPE_LAYOUT_KIND_UNION:
            return (TZrBool)(slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT);
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 function_frame_parameter_index_for_stack_slot(const SZrFunction *function,
                                                              TZrUInt32 stackSlot) {
    TZrUInt32 parameterIndex = 0u;

    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];

        if (slotLayout->isParameter && slotLayout->stackSlot < stackSlot) {
            parameterIndex++;
        }
    }

    return parameterIndex;
}

static void function_frame_reset_value_parameter_destination(struct SZrState *state, SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    /*
     * Value-frame parameter slots are overwritten during frame setup before the
     * callee owns them. Stack-frame storage can be reused under an inline frame,
     * so release any staged owner before normalizing both byte-frame and dense
     * mirrors for the copied parameter.
     */
    if (ZrCore_Value_HasReleasableOwnershipNoProfile(value)) {
        ZrCore_Ownership_ReleaseValue(state, value);
        return;
    }
    ZrCore_Value_ResetAsNull(value);
}

static void function_frame_drop_value_slot_if_owner(struct SZrState *state, SZrTypeValue *value) {
    if (!ZrCore_Value_HasReleasableOwnershipNoProfile(value)) {
        return;
    }

    ZrCore_Ownership_ReleaseValue(state, value);
}

static TZrBool function_frame_value_is_struct_object(const SZrTypeValue *value, SZrObject **outObject) {
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

static const SZrTypeValue *function_frame_get_object_member_value(SZrState *state,
                                                                  SZrObject *object,
                                                                  SZrString *memberName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrBool function_frame_load_signed_int(const TZrByte *address,
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

static TZrBool function_frame_load_unsigned_int(const TZrByte *address,
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

static TZrBool function_frame_store_signed_int(TZrByte *address,
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

static TZrBool function_frame_store_unsigned_int(TZrByte *address,
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

static TZrBool function_frame_load_primitive_field(SZrState *state,
                                                   const SZrFunctionFrameFieldLayout *fieldLayout,
                                                   const TZrByte *fieldAddress,
                                                   SZrTypeValue *result) {
    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod ||
        fieldAddress == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (fieldLayout->valueType) {
        ZR_VALUE_CASES_SIGNED_INT {
            TZrInt64 value;
            if (!function_frame_load_signed_int(fieldAddress, fieldLayout->byteSize, &value)) {
                return ZR_FALSE;
            }
            ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
        ZR_VALUE_CASES_UNSIGNED_INT {
            TZrUInt64 value;
            if (!function_frame_load_unsigned_int(fieldAddress, fieldLayout->byteSize, &value)) {
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

static TZrBool function_frame_store_primitive_field(const SZrFunctionFrameFieldLayout *fieldLayout,
                                                    TZrByte *fieldAddress,
                                                    const SZrTypeValue *value) {
    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod ||
        fieldAddress == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (fieldLayout->valueType) {
        ZR_VALUE_CASES_SIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
                return function_frame_store_signed_int(fieldAddress,
                                                       fieldLayout->byteSize,
                                                       (TZrInt64)value->value.nativeObject.nativeUInt64);
            }
            return function_frame_store_signed_int(fieldAddress,
                                                   fieldLayout->byteSize,
                                                   value->value.nativeObject.nativeInt64);
        ZR_VALUE_CASES_UNSIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
                return function_frame_store_unsigned_int(fieldAddress,
                                                         fieldLayout->byteSize,
                                                         (TZrUInt64)value->value.nativeObject.nativeInt64);
            }
            return function_frame_store_unsigned_int(fieldAddress,
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

static TZrBool function_frame_load_field(SZrState *state,
                                         const SZrFunction *function,
                                         const SZrFunctionFrameFieldLayout *fieldLayout,
                                         const TZrByte *fieldAddress,
                                         SZrTypeValue *result);

static TZrBool function_frame_store_field(SZrState *state,
                                          const SZrFunction *function,
                                          const SZrFunctionFrameFieldLayout *fieldLayout,
                                          TZrByte *fieldAddress,
                                          const SZrTypeValue *value);

static TZrBool function_frame_load_value_slot_field(SZrState *state,
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

static TZrBool function_frame_store_value_slot_field(SZrState *state,
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

static TZrBool function_frame_load_field(SZrState *state,
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

    if (function_frame_load_primitive_field(state, fieldLayout, fieldAddress, result)) {
        return ZR_TRUE;
    }

    return function_frame_load_value_slot_field(state, fieldLayout, fieldAddress, result);
}

static TZrBool function_frame_store_nested_object_field(SZrState *state,
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

static TZrBool function_frame_store_field(SZrState *state,
                                          const SZrFunction *function,
                                          const SZrFunctionFrameFieldLayout *fieldLayout,
                                          TZrByte *fieldAddress,
                                          const SZrTypeValue *value) {
    if (function_frame_store_nested_object_field(state, function, fieldLayout, fieldAddress, value)) {
        return ZR_TRUE;
    }

    if (function_frame_store_primitive_field(fieldLayout, fieldAddress, value)) {
        return ZR_TRUE;
    }

    return function_frame_store_value_slot_field(state, fieldLayout, fieldAddress, value);
}

typedef struct SZrFunctionFrameInlineToObjectContext {
    const TZrByte *sourceAddress;
    TZrUInt32 sourceByteSize;
    SZrObject *object;
    TZrBool failed;
} SZrFunctionFrameInlineToObjectContext;

typedef struct SZrFunctionFrameObjectToInlineContext {
    TZrByte *destinationAddress;
    TZrUInt32 destinationByteSize;
    SZrObject *sourceObject;
    TZrBool failed;
} SZrFunctionFrameObjectToInlineContext;

typedef struct SZrFunctionFrameInlineInitContext {
    TZrByte *address;
    TZrUInt32 byteSize;
    TZrBool failed;
} SZrFunctionFrameInlineInitContext;

static TZrBool function_frame_copy_inline_field_to_object(SZrState *state,
                                                          const SZrFunction *function,
                                                          TZrUInt32 typeLayoutId,
                                                          SZrString *fieldName,
                                                          const SZrFunctionFrameFieldLayout *fieldLayout,
                                                          TZrPtr userData) {
    SZrFunctionFrameInlineToObjectContext *context = (SZrFunctionFrameInlineToObjectContext *)userData;
    const TZrByte *fieldAddress;
    SZrTypeValue key;
    SZrTypeValue fieldValue;

    ZR_UNUSED_PARAMETER(typeLayoutId);
    if (context == ZR_NULL ||
        fieldName == ZR_NULL ||
        fieldLayout == ZR_NULL ||
        fieldLayout->byteOffset > context->sourceByteSize ||
        fieldLayout->byteSize > context->sourceByteSize - fieldLayout->byteOffset) {
        if (context != ZR_NULL) {
            context->failed = ZR_TRUE;
        }
        return ZR_FALSE;
    }

    fieldAddress = context->sourceAddress + fieldLayout->byteOffset;
    ZrCore_Value_ResetAsNull(&fieldValue);
    if (!function_frame_load_field(state, function, fieldLayout, fieldAddress, &fieldValue)) {
        context->failed = ZR_TRUE;
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, context->object, &key, &fieldValue);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        context->failed = ZR_TRUE;
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool function_frame_copy_object_field_to_inline(SZrState *state,
                                                          const SZrFunction *function,
                                                          TZrUInt32 typeLayoutId,
                                                          SZrString *fieldName,
                                                          const SZrFunctionFrameFieldLayout *fieldLayout,
                                                          TZrPtr userData) {
    SZrFunctionFrameObjectToInlineContext *context = (SZrFunctionFrameObjectToInlineContext *)userData;
    const SZrTypeValue *sourceFieldValue;
    TZrByte *fieldAddress;

    ZR_UNUSED_PARAMETER(typeLayoutId);
    if (context == ZR_NULL ||
        fieldName == ZR_NULL ||
        fieldLayout == ZR_NULL ||
        fieldLayout->byteOffset > context->destinationByteSize ||
        fieldLayout->byteSize > context->destinationByteSize - fieldLayout->byteOffset) {
        return ZR_FALSE;
    }

    sourceFieldValue = function_frame_get_object_member_value(state, context->sourceObject, fieldName);
    if (sourceFieldValue == ZR_NULL) {
        return ZR_TRUE;
    }

    fieldAddress = context->destinationAddress + fieldLayout->byteOffset;
    if (!function_frame_store_field(state, function, fieldLayout, fieldAddress, sourceFieldValue)) {
        context->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool function_frame_init_inline_field(SZrState *state,
                                                const SZrFunction *function,
                                                TZrUInt32 typeLayoutId,
                                                SZrString *fieldName,
                                                const SZrFunctionFrameFieldLayout *fieldLayout,
                                                TZrPtr userData) {
    SZrFunctionFrameInlineInitContext *context = (SZrFunctionFrameInlineInitContext *)userData;
    TZrByte *fieldAddress;

    ZR_UNUSED_PARAMETER(typeLayoutId);
    ZR_UNUSED_PARAMETER(fieldName);
    if (context == ZR_NULL ||
        fieldLayout == ZR_NULL ||
        fieldLayout->byteOffset > context->byteSize ||
        fieldLayout->byteSize > context->byteSize - fieldLayout->byteOffset) {
        if (context != ZR_NULL) {
            context->failed = ZR_TRUE;
        }
        return ZR_FALSE;
    }

    fieldAddress = context->address + fieldLayout->byteOffset;
    if (fieldLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        if (!ZrCore_Function_InitInlineStorage(state,
                                               function,
                                               fieldLayout->typeLayoutId,
                                               fieldAddress,
                                               fieldLayout->byteSize)) {
            context->failed = ZR_TRUE;
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (fieldLayout->isValueSlot && fieldLayout->byteSize >= sizeof(SZrTypeValue)) {
        ZrCore_Value_ResetAsNull((SZrTypeValue *)fieldAddress);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_CopyInlineStorageToObjectValue(
        SZrState *state,
        const SZrFunction *sourceFunction,
        TZrUInt32 sourceTypeLayoutId,
        const void *sourceAddress,
        TZrUInt32 sourceByteSize,
        SZrTypeValue *outValue) {
    SZrObjectPrototype *prototype;
    SZrObject *object;
    const SZrTypeLayout *typeLayout;
    SZrFunctionFrameInlineToObjectContext context;
    TZrBool objectIgnored = ZR_FALSE;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL ||
        sourceFunction == ZR_NULL ||
        sourceAddress == ZR_NULL ||
        outValue == ZR_NULL ||
        sourceTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(sourceFunction, sourceTypeLayoutId, state);
    prototype = ZrCore_Function_ResolvePrototypeFrameStructPrototype(state, sourceFunction, sourceTypeLayoutId);
    if (typeLayout == ZR_NULL ||
        prototype == ZR_NULL ||
        typeLayout->byteSize > sourceByteSize) {
        return ZR_FALSE;
    }

    object = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    object->prototype = prototype;
    ZrCore_Object_Init(state, object);

    if (state->global != ZR_NULL &&
        !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global,
                                                          state,
                                                          ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                                          &objectIgnored)) {
        return ZR_FALSE;
    }

    context.sourceAddress = (const TZrByte *)sourceAddress;
    context.sourceByteSize = sourceByteSize;
    context.object = object;
    context.failed = ZR_FALSE;
    if (!ZrCore_Function_VisitPrototypeFrameFieldLayouts(state,
                                                         sourceFunction,
                                                         sourceTypeLayoutId,
                                                         function_frame_copy_inline_field_to_object,
                                                         &context)) {
        context.failed = ZR_TRUE;
    }

    if (objectIgnored && state->global != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    }
    if (context.failed) {
        ZrCore_Value_ResetAsNull(outValue);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

TZrBool ZrCore_Function_CopyObjectValueToInlineStorage(SZrState *state,
                                                       const SZrFunction *destinationFunction,
                                                       TZrUInt32 destinationTypeLayoutId,
                                                       TZrPtr destinationAddress,
                                                       TZrUInt32 destinationByteSize,
                                                       const SZrTypeValue *sourceValue) {
    const SZrTypeLayout *typeLayout;
    SZrFunctionFrameObjectToInlineContext context;
    SZrObject *sourceObject;

    if (state == ZR_NULL ||
        destinationFunction == ZR_NULL ||
        destinationAddress == ZR_NULL ||
        !function_frame_value_is_struct_object(sourceValue, &sourceObject) ||
        sourceObject->prototype == ZR_NULL ||
        destinationTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(destinationFunction, destinationTypeLayoutId, state);
    if (typeLayout == ZR_NULL || typeLayout->byteSize > destinationByteSize) {
        return ZR_FALSE;
    }

    context.destinationAddress = (TZrByte *)destinationAddress;
    context.destinationByteSize = destinationByteSize;
    context.sourceObject = sourceObject;
    context.failed = ZR_FALSE;
    if (!ZrCore_Function_VisitPrototypeFrameFieldLayouts(state,
                                                         destinationFunction,
                                                         destinationTypeLayoutId,
                                                         function_frame_copy_object_field_to_inline,
                                                         &context)) {
        context.failed = ZR_TRUE;
    }
    return (TZrBool)(!context.failed);
}

TZrBool ZrCore_Function_InitInlineStorage(SZrState *state,
                                          const SZrFunction *function,
                                          TZrUInt32 typeLayoutId,
                                          TZrPtr address,
                                          TZrUInt32 byteSize) {
    const SZrTypeLayout *typeLayout;
    SZrFunctionFrameInlineInitContext context;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        address == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    if (typeLayout == ZR_NULL || typeLayout->byteSize > byteSize) {
        return ZR_FALSE;
    }

    memset(address, 0, typeLayout->byteSize);
    context.address = (TZrByte *)address;
    context.byteSize = byteSize;
    context.failed = ZR_FALSE;
    if (!ZrCore_Function_VisitPrototypeFrameFieldLayouts(state,
                                                         function,
                                                         typeLayoutId,
                                                         function_frame_init_inline_field,
                                                         &context)) {
        context.failed = ZR_TRUE;
    }
    return (TZrBool)(!context.failed);
}

TZrBool ZrCore_Function_MakeFrameSlotPlace(struct SZrState *state,
                                           const SZrFunction *function,
                                           TZrStackValuePointer frameBase,
                                           TZrUInt32 stackSlot,
                                           SZrStackFramePlace *outPlace) {
    const SZrFunctionFrameSlotLayout *slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);

    if (slotLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_Stack_MakeFramePlace(state,
                                       frameBase,
                                       slotLayout->byteOffset,
                                       slotLayout->byteSize,
                                       slotLayout->byteAlign,
                                       outPlace);
}

TZrBool ZrCore_Function_CopyFrameSlotInline(struct SZrState *state,
                                            const SZrTypeLayout *layout,
                                            const SZrFunction *destinationFunction,
                                            TZrStackValuePointer destinationFrameBase,
                                            TZrUInt32 destinationStackSlot,
                                            const SZrFunction *sourceFunction,
                                            TZrStackValuePointer sourceFrameBase,
                                            TZrUInt32 sourceStackSlot) {
    const SZrFunctionFrameSlotLayout *destinationLayout =
            ZrCore_Function_FindFrameSlotLayout(destinationFunction, destinationStackSlot);
    const SZrFunctionFrameSlotLayout *sourceLayout =
            ZrCore_Function_FindFrameSlotLayout(sourceFunction, sourceStackSlot);
    SZrStackFramePlace destinationPlace;
    SZrStackFramePlace sourcePlace;

    if (!function_frame_slot_matches_layout_kind(destinationLayout, layout) ||
        !function_frame_slot_matches_layout_kind(sourceLayout, layout)) {
        return ZR_FALSE;
    }

    if (!ZrCore_Stack_MakeFramePlace(state,
                                     destinationFrameBase,
                                     destinationLayout->byteOffset,
                                     destinationLayout->byteSize,
                                     destinationLayout->byteAlign,
                                     &destinationPlace) ||
        !ZrCore_Stack_MakeFramePlace(state,
                                     sourceFrameBase,
                                     sourceLayout->byteOffset,
                                     sourceLayout->byteSize,
                                     sourceLayout->byteAlign,
                                     &sourcePlace)) {
        return ZR_FALSE;
    }

    return ZrCore_Stack_CopyInlinePlace(state, layout, &destinationPlace, &sourcePlace);
}

TZrBool ZrCore_Function_CopyObjectValueToFrameSlotInline(struct SZrState *state,
                                                         const SZrFunction *destinationFunction,
                                                         TZrStackValuePointer destinationFrameBase,
                                                         TZrUInt32 destinationStackSlot,
                                                         const struct SZrTypeValue *sourceValue) {
    const SZrFunctionFrameSlotLayout *destinationLayout =
            ZrCore_Function_FindFrameSlotLayout(destinationFunction, destinationStackSlot);
    SZrStackFramePlace destinationPlace;

    if (destinationLayout == ZR_NULL ||
        destinationLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        !ZrCore_Function_MakeFrameSlotPlace(state,
                                            destinationFunction,
                                            destinationFrameBase,
                                            destinationStackSlot,
                                            &destinationPlace)) {
        return ZR_FALSE;
    }

    return ZrCore_Function_CopyObjectValueToInlineStorage(state,
                                                          destinationFunction,
                                                          destinationLayout->typeLayoutId,
                                                          destinationPlace.address,
                                                          destinationPlace.byteSize,
                                                          sourceValue);
}

TZrBool ZrCore_Function_CopyFrameSlotInlineToObjectValue(struct SZrState *state,
                                                         const SZrFunction *sourceFunction,
                                                         TZrStackValuePointer sourceFrameBase,
                                                         TZrUInt32 sourceStackSlot,
                                                         struct SZrTypeValue *outValue) {
    const SZrFunctionFrameSlotLayout *sourceLayout =
            ZrCore_Function_FindFrameSlotLayout(sourceFunction, sourceStackSlot);
    SZrStackFramePlace sourcePlace;

    if (sourceLayout == ZR_NULL ||
        sourceLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        !ZrCore_Function_MakeFrameSlotPlace(state,
                                            sourceFunction,
                                            sourceFrameBase,
                                            sourceStackSlot,
                                            &sourcePlace)) {
        if (outValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(outValue);
        }
        return ZR_FALSE;
    }

    return ZrCore_Function_CopyInlineStorageToObjectValue(state,
                                                          sourceFunction,
                                                          sourceLayout->typeLayoutId,
                                                          (const TZrByte *)sourcePlace.address,
                                                          sourcePlace.byteSize,
                                                          outValue);
}

TZrBool ZrCore_Function_CopyInlineFrameParameters(struct SZrState *state,
                                                  const SZrFunction *calleeFunction,
                                                  TZrStackValuePointer calleeFrameBase,
                                                  const SZrFunction *callerFunction,
                                                  TZrStackValuePointer callerFrameBase,
                                                  TZrUInt32 callerArgumentStartSlot,
                                                  FZrFunctionFrameTypeLayoutResolver resolver,
                                                  TZrPtr resolverUserData) {
    if (calleeFunction == ZR_NULL || callerFunction == ZR_NULL ||
        calleeFrameBase == ZR_NULL || callerFrameBase == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < calleeFunction->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *parameterLayout = &calleeFunction->frameSlotLayouts[index];
        const SZrFunctionFrameSlotLayout *sourceLayout;
        const SZrTypeLayout *typeLayout;
        const SZrTypeValue *sourceValue;
        TZrUInt32 sourceArgumentIndex;
        TZrUInt32 sourceStackSlot;

        if (!parameterLayout->isParameter ||
            parameterLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }

        sourceArgumentIndex = function_frame_parameter_index_for_stack_slot(calleeFunction, parameterLayout->stackSlot);
        if (resolver == ZR_NULL || callerArgumentStartSlot > UINT32_MAX - sourceArgumentIndex) {
            return ZR_FALSE;
        }

        typeLayout = resolver(calleeFunction, parameterLayout->typeLayoutId, resolverUserData);
        if (!function_frame_slot_matches_layout_kind(parameterLayout, typeLayout)) {
            return ZR_FALSE;
        }

        sourceStackSlot = callerArgumentStartSlot + sourceArgumentIndex;
        if (ZrCore_Function_CopyFrameSlotInline(state,
                                                typeLayout,
                                                calleeFunction,
                                                calleeFrameBase,
                                                parameterLayout->stackSlot,
                                                callerFunction,
                                                callerFrameBase,
                                                sourceStackSlot)) {
            continue;
        }

        sourceLayout = ZrCore_Function_FindFrameSlotLayout(callerFunction, sourceStackSlot);
        sourceValue = ZR_NULL;
        if (sourceLayout == ZR_NULL) {
            sourceValue = ZrCore_Stack_GetValue(callerFrameBase + sourceStackSlot);
        } else if (sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
                   sourceLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
            SZrStackFramePlace sourcePlace;
            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    callerFunction,
                                                    callerFrameBase,
                                                    sourceStackSlot,
                                                    &sourcePlace)) {
                return ZR_FALSE;
            }
            sourceValue = (const SZrTypeValue *)sourcePlace.address;
        }

        if (sourceValue == ZR_NULL ||
            !ZrCore_Function_CopyObjectValueToFrameSlotInline(state,
                                                              calleeFunction,
                                                              calleeFrameBase,
                                                              parameterLayout->stackSlot,
                                                              sourceValue)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrCore_Function_CopyValueFrameParameters(struct SZrState *state,
                                                 const SZrFunction *calleeFunction,
                                                 TZrStackValuePointer calleeFrameBase,
                                                 TZrStackValuePointer argumentBase,
                                                 TZrSize argumentsCount) {
    if (state == ZR_NULL || calleeFunction == ZR_NULL ||
        calleeFrameBase == ZR_NULL || argumentBase == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < calleeFunction->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *parameterLayout = &calleeFunction->frameSlotLayouts[index];
        TZrUInt32 parameterIndex;
        SZrStackFramePlace destinationPlace;

        if (!parameterLayout->isParameter) {
            continue;
        }

        parameterIndex = function_frame_parameter_index_for_stack_slot(calleeFunction, parameterLayout->stackSlot);
        if (parameterIndex >= argumentsCount) {
            continue;
        }

        if (parameterLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            parameterLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            continue;
        }

        if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                calleeFunction,
                                                calleeFrameBase,
                                                parameterLayout->stackSlot,
                                                &destinationPlace)) {
            return ZR_FALSE;
        }

        {
            const SZrTypeValue *sourceValue = ZrCore_Stack_GetValue(argumentBase + parameterIndex);
            SZrTypeValue *byteDestination = (SZrTypeValue *)destinationPlace.address;
            SZrTypeValue *denseDestination = ZrCore_Stack_GetValue(calleeFrameBase + parameterLayout->stackSlot);

            function_frame_reset_value_parameter_destination(state, byteDestination);
            ZrCore_Value_Copy(state, byteDestination, sourceValue);
            if (denseDestination != byteDestination &&
                !ZrCore_Value_SlotsOverlapNoProfile(byteDestination, denseDestination)) {
                function_frame_reset_value_parameter_destination(state, denseDestination);
                ZrCore_Value_Copy(state, denseDestination, byteDestination);
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrCore_Function_CopyValueFrameParametersFromFrame(struct SZrState *state,
                                                          const SZrFunction *calleeFunction,
                                                          TZrStackValuePointer calleeFrameBase,
                                                          const SZrFunction *sourceFunction,
                                                          TZrStackValuePointer sourceFrameBase,
                                                          TZrUInt32 sourceArgumentStartSlot,
                                                          TZrSize argumentsCount) {
    if (state == ZR_NULL || calleeFunction == ZR_NULL || sourceFunction == ZR_NULL ||
        calleeFrameBase == ZR_NULL || sourceFrameBase == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < calleeFunction->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *parameterLayout = &calleeFunction->frameSlotLayouts[index];
        const SZrFunctionFrameSlotLayout *sourceLayout;
        const SZrTypeValue *denseSourceValue;
        const SZrTypeValue *sourceValue = ZR_NULL;
        TZrUInt32 parameterIndex;
        TZrUInt32 sourceStackSlot;
        SZrStackFramePlace destinationPlace;

        if (!parameterLayout->isParameter) {
            continue;
        }

        parameterIndex = function_frame_parameter_index_for_stack_slot(calleeFunction, parameterLayout->stackSlot);
        if (parameterIndex >= argumentsCount || sourceArgumentStartSlot > UINT32_MAX - parameterIndex) {
            continue;
        }

        if (parameterLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            parameterLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            continue;
        }

        if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                calleeFunction,
                                                calleeFrameBase,
                                                parameterLayout->stackSlot,
                                                &destinationPlace)) {
            return ZR_FALSE;
        }

        sourceStackSlot = sourceArgumentStartSlot + parameterIndex;
        denseSourceValue = ZrCore_Stack_GetValue(sourceFrameBase + sourceStackSlot);
        sourceLayout = ZrCore_Function_FindFrameSlotLayout(sourceFunction, sourceStackSlot);
        if (sourceLayout == ZR_NULL) {
            sourceValue = denseSourceValue;
        } else if (sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
                   sourceLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
            SZrStackFramePlace sourcePlace;
            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    sourceFunction,
                                                    sourceFrameBase,
                                                    sourceStackSlot,
                                                    &sourcePlace)) {
                return ZR_FALSE;
            }
            sourceValue = (const SZrTypeValue *)sourcePlace.address;
            if (ZR_VALUE_IS_TYPE_NULL(sourceValue->type) &&
                denseSourceValue != ZR_NULL &&
                !ZR_VALUE_IS_TYPE_NULL(denseSourceValue->type)) {
                sourceValue = denseSourceValue;
            }
        }

        if (sourceValue == ZR_NULL) {
            return ZR_FALSE;
        }

        {
            SZrTypeValue *byteDestination = (SZrTypeValue *)destinationPlace.address;
            SZrTypeValue *denseDestination = ZrCore_Stack_GetValue(calleeFrameBase + parameterLayout->stackSlot);

            function_frame_reset_value_parameter_destination(state, byteDestination);
            ZrCore_Value_Copy(state, byteDestination, sourceValue);
            if (denseDestination != byteDestination &&
                !ZrCore_Value_SlotsOverlapNoProfile(byteDestination, denseDestination)) {
                function_frame_reset_value_parameter_destination(state, denseDestination);
                ZrCore_Value_Copy(state, denseDestination, byteDestination);
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool function_visit_frame_gc_values(struct SZrState *state,
                                              const SZrFunction *function,
                                              TZrStackValuePointer frameBase,
                                              FZrFunctionFrameTypeLayoutResolver resolver,
                                              TZrPtr resolverUserData,
                                              FZrFunctionFrameGcValueVisitor visitor,
                                              TZrPtr visitorUserData,
                                              TZrBool includeValueSlots) {
    if (function == ZR_NULL || frameBase == ZR_NULL || visitor == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        const SZrTypeLayout *typeLayout;
        SZrStackFramePlace place;

        if (includeValueSlots &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE) {
            if (slotLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
                continue;
            }
            if (!ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place)) {
                return ZR_FALSE;
            }
            visitor(state, (SZrTypeValue *)place.address, visitorUserData);
            continue;
        }

        if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }

        if (resolver == ZR_NULL) {
            return ZR_FALSE;
        }

        typeLayout = resolver(function, slotLayout->typeLayoutId, resolverUserData);
        if (!function_frame_slot_matches_layout_kind(slotLayout, typeLayout) ||
            !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place)) {
            return ZR_FALSE;
        }

        ZrCore_TypeLayout_VisitGcValues(state, typeLayout, place.address, visitor, visitorUserData);
    }

    return ZR_TRUE;
}

TZrBool ZrCore_Function_VisitInlineFrameGcValues(struct SZrState *state,
                                                 const SZrFunction *function,
                                                 TZrStackValuePointer frameBase,
                                                 FZrFunctionFrameTypeLayoutResolver resolver,
                                                 TZrPtr resolverUserData,
                                                 FZrFunctionFrameGcValueVisitor visitor,
                                                 TZrPtr visitorUserData) {
    return function_visit_frame_gc_values(state,
                                          function,
                                          frameBase,
                                          resolver,
                                          resolverUserData,
                                          visitor,
                                          visitorUserData,
                                          ZR_FALSE);
}

TZrBool ZrCore_Function_VisitFrameGcValues(struct SZrState *state,
                                           const SZrFunction *function,
                                           TZrStackValuePointer frameBase,
                                           FZrFunctionFrameTypeLayoutResolver resolver,
                                           TZrPtr resolverUserData,
                                           FZrFunctionFrameGcValueVisitor visitor,
                                           TZrPtr visitorUserData) {
    return function_visit_frame_gc_values(state,
                                          function,
                                          frameBase,
                                          resolver,
                                          resolverUserData,
                                          visitor,
                                          visitorUserData,
                                          ZR_TRUE);
}

TZrBool ZrCore_Function_DropInlineFrameValues(struct SZrState *state,
                                              const SZrFunction *function,
                                              TZrStackValuePointer frameBase,
                                              FZrFunctionFrameTypeLayoutResolver resolver,
                                              TZrPtr resolverUserData) {
    if (function == ZR_NULL || frameBase == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        const SZrTypeLayout *typeLayout;

        if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }

        if (resolver == ZR_NULL) {
            return ZR_FALSE;
        }

        typeLayout = resolver(function, slotLayout->typeLayoutId, resolverUserData);
        if (!function_frame_slot_matches_layout_kind(slotLayout, typeLayout)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        const SZrTypeLayout *typeLayout;
        SZrStackFramePlace place;

        if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }

        if (resolver == ZR_NULL) {
            return ZR_FALSE;
        }

        typeLayout = resolver(function, slotLayout->typeLayoutId, resolverUserData);
        if (!function_frame_slot_matches_layout_kind(slotLayout, typeLayout) ||
            !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place)) {
            return ZR_FALSE;
        }

        ZrCore_TypeLayout_DropInline(state, typeLayout, place.address);
    }

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[index];
        SZrStackFramePlace place;
        SZrTypeValue *byteValue;
        SZrTypeValue *denseValue;

        if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            slotLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
            !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slotLayout->stackSlot, &place)) {
            continue;
        }

        byteValue = (SZrTypeValue *)place.address;
        denseValue = ZrCore_Stack_GetValue(frameBase + slotLayout->stackSlot);
        function_frame_drop_value_slot_if_owner(state, byteValue);
        if (denseValue != byteValue &&
            !ZrCore_Value_SlotsOverlapNoProfile(byteValue, denseValue)) {
            function_frame_drop_value_slot_if_owner(state, denseValue);
        }
    }

    return ZR_TRUE;
}
