#include "zr_vm_core/property_reference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#define ZR_PROPERTY_REFERENCE_BASE_MEMBER "__zr_property_ref_base"
#define ZR_PROPERTY_REFERENCE_KIND_FIELD "__zr_property_ref_kind"
#define ZR_PROPERTY_REFERENCE_KEY_MEMBER "__zr_property_ref_key"
#define ZR_PROPERTY_REFERENCE_OWNER_MEMBER "__zr_property_ref_owner"
#define ZR_PROPERTY_REFERENCE_DESCRIPTOR_MEMBER "__zr_property_ref_descriptor"
#define ZR_PROPERTY_REFERENCE_FRAME_FUNCTION_MEMBER "__zr_property_ref_frame_function"
#define ZR_PROPERTY_REFERENCE_FRAME_OFFSET_MEMBER "__zr_property_ref_frame_offset"
#define ZR_PROPERTY_REFERENCE_FRAME_SLOT_MEMBER "__zr_property_ref_frame_slot"
#define ZR_PROPERTY_REFERENCE_FRAME_FIELD_MEMBER "__zr_property_ref_frame_field"

typedef enum EZrPropertyReferenceKind {
    ZR_PROPERTY_REFERENCE_KIND_MEMBER = 1,
    ZR_PROPERTY_REFERENCE_KIND_INDEX = 2,
    ZR_PROPERTY_REFERENCE_KIND_FRAME_MEMBER = 3
} EZrPropertyReferenceKind;

static SZrObjectPrototype *property_reference_receiver_prototype(
        SZrState *state,
        const SZrTypeValue *base) {
    SZrObject *object;

    if (state == ZR_NULL || base == ZR_NULL || base->value.object == ZR_NULL ||
        (base->type != ZR_VALUE_TYPE_OBJECT &&
         base->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    object = ZR_CAST_OBJECT(state, base->value.object);
    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return (SZrObjectPrototype *)object;
    }
    return object->prototype;
}

static TZrBool property_reference_resolve_symbol_descriptor(
        SZrState *state,
        const SZrTypeValue *base,
        SZrString *symbol,
        SZrObjectPrototype **ownerPrototype,
        TZrUInt32 *descriptorIndex) {
    SZrObjectPrototype *candidate;

    if (ownerPrototype == ZR_NULL || descriptorIndex == ZR_NULL ||
        symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    candidate = property_reference_receiver_prototype(state, base);
    while (candidate != ZR_NULL) {
        const SZrMemberDescriptor *descriptor =
                ZrCore_ObjectPrototype_FindMemberDescriptor(
                        candidate, symbol, ZR_FALSE);

        if (descriptor != ZR_NULL && candidate->memberDescriptors != ZR_NULL &&
            descriptor >= candidate->memberDescriptors &&
            descriptor < candidate->memberDescriptors +
                                 candidate->memberDescriptorCount) {
            *ownerPrototype = candidate;
            *descriptorIndex = (TZrUInt32)(descriptor -
                                           candidate->memberDescriptors);
            return ZR_TRUE;
        }
        candidate = candidate->superPrototype;
    }
    return ZR_FALSE;
}

static SZrString *property_reference_member_name(
        SZrState *state,
        const TZrChar *name) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
}

static TZrBool property_reference_set_member(
        SZrState *state,
        SZrTypeValue *reference,
        const TZrChar *name,
        const SZrTypeValue *value) {
    SZrString *memberName = property_reference_member_name(state, name);

    return (TZrBool)(memberName != ZR_NULL &&
                     ZrCore_Object_SetMember(state, reference, memberName, value));
}

static TZrBool property_reference_get_member(
        SZrState *state,
        SZrTypeValue *reference,
        const TZrChar *name,
        SZrTypeValue *value) {
    SZrString *memberName = property_reference_member_name(state, name);

    if (memberName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(value);
    return ZrCore_Object_GetMember(state, reference, memberName, value);
}

static TZrBool property_reference_create_shell(
        SZrState *state,
        const SZrTypeValue *base,
        EZrPropertyReferenceKind kind,
        SZrTypeValue *result) {
    SZrObject *object;
    SZrTypeValue kindValue;

    if (state == ZR_NULL || base == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    object = ZrCore_Object_New(state, ZR_NULL);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, object);
    ZrCore_Value_InitAsRawObject(
            state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Value_InitAsUInt(state, &kindValue, (TZrUInt64)kind);
    return (TZrBool)(property_reference_set_member(
                             state,
                             result,
                             ZR_PROPERTY_REFERENCE_BASE_MEMBER,
                             base) &&
                     property_reference_set_member(
                             state,
                             result,
                             ZR_PROPERTY_REFERENCE_KIND_FIELD,
                             &kindValue));
}

TZrBool ZrCore_PropertyReference_CreateMember(
        SZrState *state,
        SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 receiverSlot,
        const SZrTypeValue *base,
        TZrUInt32 memberEntryIndex,
        SZrTypeValue *result) {
    const SZrFunctionMemberEntry *entry;
    SZrFunction *prototypeOwner;
    SZrObjectPrototype *ownerPrototype;
    SZrTypeValue ownerValue;
    SZrTypeValue descriptorValue;

    if (function == ZR_NULL || function->memberEntries == ZR_NULL ||
        memberEntryIndex >= function->memberEntryLength) {
        return ZR_FALSE;
    }
    entry = &function->memberEntries[memberEntryIndex];
    if (entry->symbol != ZR_NULL && frameBase != ZR_NULL) {
        const SZrFunction *sourceFunction;
        SZrFunctionStackAnchor sourceFrameBase;
        TZrUInt32 sourceStackSlot;
        const SZrFunctionFrameSlotLayout *sourceLayout;
        SZrFunctionFrameFieldLayout fieldLayout;

        TZrBool resolvedFrame = ZrCore_Function_ResolveFrameSlotReferenceAnchor(
                    state,
                    function,
                    frameBase,
                    receiverSlot,
                    &sourceFunction,
                    &sourceFrameBase,
                    &sourceStackSlot);
        if (resolvedFrame &&
            sourceFunction != ZR_NULL &&
            (sourceLayout = ZrCore_Function_FindFrameSlotLayout(
                     sourceFunction, sourceStackSlot)) != ZR_NULL &&
            ZrCore_Function_ResolvePrototypeFrameFieldLayout(
                    state,
                    sourceFunction,
                    sourceLayout->typeLayoutId,
                    entry->symbol,
                    &fieldLayout)) {
            SZrTypeValue functionValue;
            SZrTypeValue frameOffsetValue;
            SZrTypeValue frameSlotValue;
            SZrTypeValue fieldValue;

            if (!property_reference_create_shell(
                        state,
                        base,
                        ZR_PROPERTY_REFERENCE_KIND_FRAME_MEMBER,
                        result)) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(
                    state,
                    &functionValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER((SZrFunction *)sourceFunction));
            ZrCore_Value_InitAsInt(
                    state,
                    &frameOffsetValue,
                    (TZrInt64)sourceFrameBase.offset);
            ZrCore_Value_InitAsUInt(
                    state, &frameSlotValue, (TZrUInt64)sourceStackSlot);
            ZrCore_Value_InitAsRawObject(
                    state,
                    &fieldValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(entry->symbol));
            return (TZrBool)(property_reference_set_member(
                                     state,
                                     result,
                                     ZR_PROPERTY_REFERENCE_FRAME_FUNCTION_MEMBER,
                                     &functionValue) &&
                             property_reference_set_member(
                                     state,
                                     result,
                                     ZR_PROPERTY_REFERENCE_FRAME_OFFSET_MEMBER,
                                     &frameOffsetValue) &&
                             property_reference_set_member(
                                     state,
                                     result,
                                     ZR_PROPERTY_REFERENCE_FRAME_SLOT_MEMBER,
                                     &frameSlotValue) &&
                             property_reference_set_member(
                                     state,
                                     result,
                                     ZR_PROPERTY_REFERENCE_FRAME_FIELD_MEMBER,
                                     &fieldValue));
        }
    }
    if (entry->entryKind == ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR) {
        prototypeOwner = function;
        while (prototypeOwner != ZR_NULL &&
               (prototypeOwner->prototypeData == ZR_NULL ||
                prototypeOwner->prototypeCount == 0U)) {
            prototypeOwner = prototypeOwner->ownerFunction;
        }
        if (prototypeOwner == ZR_NULL ||
            entry->prototypeIndex >= prototypeOwner->prototypeCount) {
            return ZR_FALSE;
        }
        if (prototypeOwner->prototypeInstances == ZR_NULL ||
            entry->prototypeIndex >= prototypeOwner->prototypeInstancesLength ||
            prototypeOwner->prototypeInstances[entry->prototypeIndex] == ZR_NULL) {
            ZrCore_Module_CreatePrototypesFromData(
                    state, ZR_NULL, prototypeOwner);
        }
        if (prototypeOwner->prototypeInstances == ZR_NULL ||
            entry->prototypeIndex >= prototypeOwner->prototypeInstancesLength) {
            return ZR_FALSE;
        }
        ownerPrototype =
                prototypeOwner->prototypeInstances[entry->prototypeIndex];
        if (ownerPrototype == ZR_NULL ||
            ownerPrototype->memberDescriptors == ZR_NULL ||
            entry->descriptorIndex >= ownerPrototype->memberDescriptorCount) {
            return ZR_FALSE;
        }
    } else if (entry->entryKind == ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL) {
        TZrUInt32 resolvedDescriptorIndex;

        if (!property_reference_resolve_symbol_descriptor(
                    state,
                    base,
                    entry->symbol,
                    &ownerPrototype,
                    &resolvedDescriptorIndex)) {
            return ZR_FALSE;
        }
        entry = ZR_NULL;
        ZrCore_Value_InitAsUInt(
                state, &descriptorValue, (TZrUInt64)resolvedDescriptorIndex);
    } else {
        return ZR_FALSE;
    }
    if (!property_reference_create_shell(
                state, base, ZR_PROPERTY_REFERENCE_KIND_MEMBER, result)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &ownerValue, ZR_CAST_RAW_OBJECT_AS_SUPER(ownerPrototype));
    if (entry != ZR_NULL) {
        ZrCore_Value_InitAsUInt(
                state, &descriptorValue, (TZrUInt64)entry->descriptorIndex);
    }
    return (TZrBool)(property_reference_set_member(
                             state,
                             result,
                             ZR_PROPERTY_REFERENCE_OWNER_MEMBER,
                             &ownerValue) &&
                     property_reference_set_member(
                             state,
                             result,
                             ZR_PROPERTY_REFERENCE_DESCRIPTOR_MEMBER,
                             &descriptorValue));
}

TZrBool ZrCore_PropertyReference_CreateIndex(
        SZrState *state,
        const SZrTypeValue *base,
        const SZrTypeValue *key,
        SZrTypeValue *result) {
    return (TZrBool)(key != ZR_NULL &&
                     property_reference_create_shell(
                             state, base, ZR_PROPERTY_REFERENCE_KIND_INDEX, result) &&
                     property_reference_set_member(
                             state,
                             result,
                             ZR_PROPERTY_REFERENCE_KEY_MEMBER,
                             key));
}

static TZrBool property_reference_get_payload(
        SZrState *state,
        SZrTypeValue *reference,
        SZrTypeValue *base,
        EZrPropertyReferenceKind *kind) {
    SZrTypeValue kindValue;

    if (state == ZR_NULL || reference == ZR_NULL || base == ZR_NULL ||
        kind == ZR_NULL || reference->type != ZR_VALUE_TYPE_OBJECT ||
        reference->value.object == ZR_NULL ||
        !property_reference_get_member(
                state, reference, ZR_PROPERTY_REFERENCE_BASE_MEMBER, base) ||
        !property_reference_get_member(
                state, reference, ZR_PROPERTY_REFERENCE_KIND_FIELD, &kindValue) ||
        kindValue.type != ZR_VALUE_TYPE_UINT64 ||
        (kindValue.value.nativeObject.nativeUInt64 !=
                         (TZrUInt64)ZR_PROPERTY_REFERENCE_KIND_MEMBER &&
         kindValue.value.nativeObject.nativeUInt64 !=
                         (TZrUInt64)ZR_PROPERTY_REFERENCE_KIND_INDEX &&
         kindValue.value.nativeObject.nativeUInt64 !=
                         (TZrUInt64)ZR_PROPERTY_REFERENCE_KIND_FRAME_MEMBER)) {
        return ZR_FALSE;
    }
    *kind = (EZrPropertyReferenceKind)kindValue.value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static TZrBool property_reference_get_frame_binding(
        SZrState *state,
        SZrTypeValue *reference,
        SZrFunction **function,
        TZrStackValuePointer *frameBase,
        TZrUInt32 *receiverSlot,
        SZrString **memberName) {
    SZrTypeValue functionValue;
    SZrTypeValue frameOffsetValue;
    SZrTypeValue frameSlotValue;
    SZrTypeValue fieldValue;
    SZrFunctionStackAnchor frameAnchor;

    if (function == ZR_NULL || frameBase == ZR_NULL ||
        receiverSlot == ZR_NULL || memberName == ZR_NULL ||
        !property_reference_get_member(
                state,
                reference,
                ZR_PROPERTY_REFERENCE_FRAME_FUNCTION_MEMBER,
                &functionValue) ||
        !property_reference_get_member(
                state,
                reference,
                ZR_PROPERTY_REFERENCE_FRAME_OFFSET_MEMBER,
                &frameOffsetValue) ||
        !property_reference_get_member(
                state,
                reference,
                ZR_PROPERTY_REFERENCE_FRAME_SLOT_MEMBER,
                &frameSlotValue) ||
        !property_reference_get_member(
                state,
                reference,
                ZR_PROPERTY_REFERENCE_FRAME_FIELD_MEMBER,
                &fieldValue) ||
        functionValue.type != ZR_VALUE_TYPE_FUNCTION ||
        functionValue.value.object == ZR_NULL ||
        frameOffsetValue.type != ZR_VALUE_TYPE_INT64 ||
        frameSlotValue.type != ZR_VALUE_TYPE_UINT64 ||
        frameSlotValue.value.nativeObject.nativeUInt64 >
                (TZrUInt64)((TZrUInt32)-1) ||
        fieldValue.type != ZR_VALUE_TYPE_STRING ||
        fieldValue.value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    *function = ZR_CAST_FUNCTION(state, functionValue.value.object);
    frameAnchor.offset =
            (TZrMemoryOffset)frameOffsetValue.value.nativeObject.nativeInt64;
    *frameBase = ZrCore_Function_StackAnchorRestore(state, &frameAnchor);
    *receiverSlot =
            (TZrUInt32)frameSlotValue.value.nativeObject.nativeUInt64;
    *memberName = ZR_CAST_STRING(state, fieldValue.value.object);
    return (TZrBool)(*function != ZR_NULL && *frameBase != ZR_NULL &&
                     *memberName != ZR_NULL);
}

static TZrBool property_reference_get_member_binding(
        SZrState *state,
        SZrTypeValue *reference,
        SZrObjectPrototype **ownerPrototype,
        TZrUInt32 *descriptorIndex) {
    SZrTypeValue ownerValue;
    SZrTypeValue descriptorValue;

    if (ownerPrototype == ZR_NULL || descriptorIndex == ZR_NULL ||
        !property_reference_get_member(
                state, reference, ZR_PROPERTY_REFERENCE_OWNER_MEMBER, &ownerValue) ||
        !property_reference_get_member(
                state,
                reference,
                ZR_PROPERTY_REFERENCE_DESCRIPTOR_MEMBER,
                &descriptorValue) ||
        ownerValue.type != ZR_VALUE_TYPE_OBJECT ||
        ownerValue.value.object == ZR_NULL ||
        descriptorValue.type != ZR_VALUE_TYPE_UINT64 ||
        descriptorValue.value.nativeObject.nativeUInt64 >
                (TZrUInt64)((TZrUInt32)-1)) {
        return ZR_FALSE;
    }
    *ownerPrototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(
            state, ownerValue.value.object);
    *descriptorIndex = (TZrUInt32)descriptorValue.value.nativeObject.nativeUInt64;
    return (TZrBool)(*ownerPrototype != ZR_NULL &&
                     (*ownerPrototype)->memberDescriptors != ZR_NULL &&
                     *descriptorIndex < (*ownerPrototype)->memberDescriptorCount);
}

TZrBool ZrCore_PropertyReference_Load(
        SZrState *state,
        SZrTypeValue *reference,
        SZrTypeValue *result) {
    SZrTypeValue base;
    EZrPropertyReferenceKind kind;

    if (result == ZR_NULL ||
        !property_reference_get_payload(state, reference, &base, &kind)) {
        return ZR_FALSE;
    }
    if (kind == ZR_PROPERTY_REFERENCE_KIND_FRAME_MEMBER) {
        SZrFunction *function;
        TZrStackValuePointer frameBase;
        TZrUInt32 receiverSlot;
        SZrString *memberName = ZR_NULL;

        TZrBool resolved = property_reference_get_frame_binding(
                                 state,
                                 reference,
                                 &function,
                                 &frameBase,
                                 &receiverSlot,
                                 &memberName);
        TZrBool loaded = resolved &&
                         ZrCore_Function_GetFrameSlotInlineMemberValue(
                                 state, function, frameBase, receiverSlot, memberName, result);
        return loaded;
    }
    if (kind == ZR_PROPERTY_REFERENCE_KIND_MEMBER) {
        SZrObjectPrototype *ownerPrototype;
        TZrUInt32 descriptorIndex;

        return (TZrBool)(property_reference_get_member_binding(
                                 state,
                                 reference,
                                 &ownerPrototype,
                                 &descriptorIndex) &&
                         ZrCore_Object_GetMemberCachedDescriptorUnchecked(
                                 state,
                                 &base,
                                 ownerPrototype,
                                 descriptorIndex,
                                 result));
    }
    {
        SZrTypeValue key;

        return (TZrBool)(property_reference_get_member(
                                 state,
                                 reference,
                                 ZR_PROPERTY_REFERENCE_KEY_MEMBER,
                                 &key) &&
                         ZrCore_Object_GetByIndex(state, &base, &key, result));
    }
}

TZrBool ZrCore_PropertyReference_Store(
        SZrState *state,
        SZrTypeValue *reference,
        const SZrTypeValue *value) {
    SZrTypeValue base;
    EZrPropertyReferenceKind kind;

    if (value == ZR_NULL ||
        !property_reference_get_payload(state, reference, &base, &kind)) {
        return ZR_FALSE;
    }
    if (kind == ZR_PROPERTY_REFERENCE_KIND_FRAME_MEMBER) {
        SZrFunction *function;
        TZrStackValuePointer frameBase;
        TZrUInt32 receiverSlot;
        SZrString *memberName = ZR_NULL;

        TZrBool resolved = property_reference_get_frame_binding(
                                 state,
                                 reference,
                                 &function,
                                 &frameBase,
                                 &receiverSlot,
                                 &memberName);
        TZrBool stored = resolved &&
                         ZrCore_Function_SetFrameSlotInlineMemberValue(
                                 state, function, frameBase, receiverSlot, memberName, value);
        return stored;
    }
    if (kind == ZR_PROPERTY_REFERENCE_KIND_MEMBER) {
        SZrObjectPrototype *ownerPrototype;
        TZrUInt32 descriptorIndex;

        return (TZrBool)(property_reference_get_member_binding(
                                 state,
                                 reference,
                                 &ownerPrototype,
                                 &descriptorIndex) &&
                         ZrCore_Object_SetMemberCachedDescriptorUnchecked(
                                 state,
                                 &base,
                                 ownerPrototype,
                                 descriptorIndex,
                                 value));
    }
    {
        SZrTypeValue key;

        return (TZrBool)(property_reference_get_member(
                                 state,
                                 reference,
                                 ZR_PROPERTY_REFERENCE_KEY_MEMBER,
                                 &key) &&
                         ZrCore_Object_SetByIndex(state, &base, &key, value));
    }
}
