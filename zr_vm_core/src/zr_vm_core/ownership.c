//
// Created by Codex on 2026/3/30.
//

#include "zr_vm_core/ownership.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"

static TZrBool ownership_value_has_object(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->value.object != ZR_NULL;
}

static void ownership_reset_value_storage(SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    value->type = ZR_VALUE_TYPE_NULL;
    value->value.nativeObject.nativeUInt64 = 0;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

static void ownership_set_value_from_object(SZrTypeValue *value,
                                            SZrRawObject *object,
                                            EZrOwnershipValueKind kind,
                                            SZrOwnershipControl *control) {
    if (value == ZR_NULL || object == ZR_NULL) {
        return;
    }

    value->type = (EZrValueType)object->type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    value->ownershipKind = kind;
    value->ownershipControl = control;
    value->ownershipWeakRef = ZR_NULL;
}

static void ownership_free_control(struct SZrState *state, SZrOwnershipControl *control) {
    if (state == ZR_NULL || state->global == ZR_NULL || control == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  control,
                                  sizeof(SZrOwnershipControl),
                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
}

static void ownership_free_weak_ref(struct SZrState *state, SZrOwnershipWeakRef *weakRef) {
    if (state == ZR_NULL || state->global == ZR_NULL || weakRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  weakRef,
                                  sizeof(SZrOwnershipWeakRef),
                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
}

static void ownership_try_free_control(struct SZrState *state, SZrOwnershipControl *control) {
    if (state == ZR_NULL || control == ZR_NULL) {
        return;
    }

    if (control->strongRefCount != 0 || control->weakRefs != ZR_NULL) {
        return;
    }

    if (control->object != ZR_NULL) {
        control->object->ownershipControl = ZR_NULL;
    }
    ownership_free_control(state, control);
}

static SZrOwnershipControl *ownership_create_control(struct SZrState *state, SZrRawObject *object) {
    SZrOwnershipControl *control;

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    control = (SZrOwnershipControl *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                     sizeof(SZrOwnershipControl),
                                                                     ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (control == ZR_NULL) {
        return ZR_NULL;
    }

    control->object = object;
    control->strongRefCount = 0;
    control->isDetachedFromGc = ZR_FALSE;
    control->weakRefs = ZR_NULL;
    object->ownershipControl = control;
    return control;
}

static SZrOwnershipControl *ownership_get_or_create_control(struct SZrState *state, SZrRawObject *object) {
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    if (object->ownershipControl != ZR_NULL) {
        return object->ownershipControl;
    }

    return ownership_create_control(state, object);
}

static void ownership_detach_weak_ref(struct SZrState *state, SZrTypeValue *value) {
    SZrOwnershipWeakRef *weakRef;
    SZrOwnershipControl *control;
    SZrOwnershipWeakRef **cursor;

    if (state == ZR_NULL || value == ZR_NULL || value->ownershipWeakRef == ZR_NULL) {
        return;
    }

    weakRef = value->ownershipWeakRef;
    control = weakRef->control;
    if (control != ZR_NULL) {
        cursor = &control->weakRefs;
        while (*cursor != ZR_NULL) {
            if (*cursor == weakRef) {
                *cursor = weakRef->next;
                break;
            }
            cursor = &(*cursor)->next;
        }
    }

    value->ownershipWeakRef = ZR_NULL;
    ownership_free_weak_ref(state, weakRef);
    ownership_try_free_control(state, control);
}

static void ownership_expire_weak_refs(struct SZrState *state, SZrOwnershipControl *control) {
    SZrOwnershipWeakRef *weakRef;

    if (state == ZR_NULL || control == ZR_NULL) {
        return;
    }

    weakRef = control->weakRefs;
    control->weakRefs = ZR_NULL;
    while (weakRef != ZR_NULL) {
        SZrOwnershipWeakRef *next = weakRef->next;
        if (weakRef->slot != ZR_NULL) {
            ownership_reset_value_storage(weakRef->slot);
        }
        ownership_free_weak_ref(state, weakRef);
        weakRef = next;
    }
}

static TZrBool ownership_register_weak_ref(struct SZrState *state,
                                           SZrTypeValue *destination,
                                           SZrOwnershipControl *control,
                                           SZrRawObject *object) {
    SZrOwnershipWeakRef *weakRef;

    if (state == ZR_NULL || state->global == ZR_NULL || destination == ZR_NULL ||
        control == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    weakRef = (SZrOwnershipWeakRef *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                     sizeof(SZrOwnershipWeakRef),
                                                                     ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (weakRef == ZR_NULL) {
        return ZR_FALSE;
    }

    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_WEAK, control);
    weakRef->slot = destination;
    weakRef->control = control;
    weakRef->next = control->weakRefs;
    control->weakRefs = weakRef;
    destination->ownershipWeakRef = weakRef;
    return ZR_TRUE;
}

static TZrBool ownership_ignore_object_if_needed(struct SZrState *state, SZrOwnershipControl *control) {
    if (state == ZR_NULL || control == ZR_NULL || control->object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!control->isDetachedFromGc) {
        if (!ZrCore_GarbageCollector_IgnoreObject(state, control->object)) {
            return ZR_FALSE;
        }
        control->isDetachedFromGc = ZR_TRUE;
    }

    return ZR_TRUE;
}

static void ownership_return_control_to_gc(struct SZrState *state, SZrOwnershipControl *control) {
    if (state == ZR_NULL || control == ZR_NULL || control->object == ZR_NULL) {
        return;
    }

    if (control->isDetachedFromGc) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, control->object);
        control->isDetachedFromGc = ZR_FALSE;
    }
}

static TZrBool ownership_promote_plain_value_to_detached_shared(struct SZrState *state,
                                                                SZrOwnershipControl *control) {
    if (state == ZR_NULL || control == ZR_NULL || control->object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_ignore_object_if_needed(state, control)) {
        return ZR_FALSE;
    }

    if (control->strongRefCount == 0) {
        control->strongRefCount = 1;
    }
    return ZR_TRUE;
}

static void ownership_copy_plain_bits(SZrTypeValue *destination, const SZrTypeValue *source) {
    destination->value = source->value;
    destination->type = source->type;
    destination->isGarbageCollectable = source->isGarbageCollectable;
    destination->isNative = source->isNative;
    destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    destination->ownershipControl = ZR_NULL;
    destination->ownershipWeakRef = ZR_NULL;
}

static TZrBool ownership_prepare_destination(struct SZrState *state, SZrTypeValue *destination) {
    if (destination == ZR_NULL) {
        return ZR_FALSE;
    }

    if (destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        destination->ownershipWeakRef != ZR_NULL ||
        destination->ownershipControl != ZR_NULL) {
        ZrCore_Ownership_ReleaseValue(state, destination);
    } else {
        ownership_reset_value_storage(destination);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_InitUniqueValue(struct SZrState *state,
                                         SZrTypeValue *destination,
                                         SZrRawObject *object) {
    SZrOwnershipControl *control;

    if (state == ZR_NULL || destination == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    control = ownership_get_or_create_control(state, object);
    if (control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_ignore_object_if_needed(state, control)) {
        return ZR_FALSE;
    }

    control->strongRefCount = 1;
    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_UNIQUE, control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_UniqueValue(struct SZrState *state,
                                     SZrTypeValue *destination,
                                     SZrTypeValue *source) {
    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    ownership_prepare_destination(state, destination);
    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source) || source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    if (!ZrCore_Ownership_InitUniqueValue(state, destination, source->value.object)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_UsingValue(struct SZrState *state,
                                    SZrTypeValue *destination,
                                    SZrTypeValue *source) {
    if (!ZrCore_Ownership_UniqueValue(state, destination, source)) {
        return ZR_FALSE;
    }

    if (destination != ZR_NULL && destination->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE) {
        destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_USING;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_ShareValue(struct SZrState *state,
                                    SZrTypeValue *destination,
                                    SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        ownership_prepare_destination(state, destination);
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = ownership_get_or_create_control(state, object);
    if (control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE) {
        if (!ownership_promote_plain_value_to_detached_shared(state, control)) {
            return ZR_FALSE;
        }
    } else if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED) {
        control->strongRefCount++;
    } else if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
               source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_USING) {
        if (!ownership_ignore_object_if_needed(state, control)) {
            return ZR_FALSE;
        }
    } else if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
    if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
        source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_USING) {
        ownership_reset_value_storage(source);
    }
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_WeakValue(struct SZrState *state,
                                   SZrTypeValue *destination,
                                   SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        ownership_prepare_destination(state, destination);
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = ownership_get_or_create_control(state, object);
    if (control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (!ownership_register_weak_ref(state, destination, control, object)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_UpgradeValue(struct SZrState *state,
                                      SZrTypeValue *destination,
                                      SZrTypeValue *source) {
    SZrOwnershipControl *control;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source) || source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_WEAK) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    control = source->ownershipControl;
    if (control == ZR_NULL || control->object == ZR_NULL || control->strongRefCount == 0) {
        ownership_reset_value_storage(destination);
        return ZR_TRUE;
    }

    control->strongRefCount++;
    ownership_set_value_from_object(destination,
                                    control->object,
                                    ZR_OWNERSHIP_VALUE_KIND_SHARED,
                                    control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_ReturnToGcValue(struct SZrState *state,
                                         SZrTypeValue *destination,
                                         SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        ownership_prepare_destination(state, destination);
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = source->ownershipControl;
    if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK || control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED && control->strongRefCount != 1) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    ownership_return_control_to_gc(state, control);
    if (control->strongRefCount > 0) {
        control->strongRefCount--;
    }

    ZrCore_Value_InitAsRawObject(state, destination, object);
    source->ownershipControl = ZR_NULL;
    source->ownershipWeakRef = ZR_NULL;
    source->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    ownership_reset_value_storage(source);

    ownership_try_free_control(state, control);
    return ZR_TRUE;
}

void ZrCore_Ownership_ReleaseValue(struct SZrState *state, SZrTypeValue *value) {
    SZrOwnershipControl *control;
    EZrOwnershipValueKind kind;

    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK) {
        ownership_detach_weak_ref(state, value);
        ownership_reset_value_storage(value);
        return;
    }

    control = value->ownershipControl;
    kind = value->ownershipKind;
    ownership_reset_value_storage(value);
    if (control == ZR_NULL) {
        return;
    }

    if ((kind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
         kind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
         kind == ZR_OWNERSHIP_VALUE_KIND_USING) &&
        control->strongRefCount > 0) {
        control->strongRefCount--;
        if (control->strongRefCount == 0 && control->isDetachedFromGc) {
            ownership_return_control_to_gc(state, control);
            ownership_expire_weak_refs(state, control);
        }
    }

    ownership_try_free_control(state, control);
}

TZrUInt32 ZrCore_Ownership_GetStrongRefCount(struct SZrRawObject *object) {
    if (object == ZR_NULL || object->ownershipControl == ZR_NULL) {
        return 0;
    }

    return object->ownershipControl->strongRefCount;
}

void ZrCore_Ownership_AssignValue(struct SZrState *state,
                                  SZrTypeValue *destination,
                                  const SZrTypeValue *source) {
    SZrOwnershipControl *control;

    if (destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    if (destination == source) {
        return;
    }

    ownership_prepare_destination(state, destination);
    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return;
    }

    switch (source->ownershipKind) {
        case ZR_OWNERSHIP_VALUE_KIND_NONE:
            ownership_copy_plain_bits(destination, source);
            break;
        case ZR_OWNERSHIP_VALUE_KIND_SHARED:
            control = source->ownershipControl;
            if (control != ZR_NULL) {
                control->strongRefCount++;
            }
            ownership_set_value_from_object(destination, source->value.object,
                                            ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
            break;
        case ZR_OWNERSHIP_VALUE_KIND_WEAK:
            if (!ownership_register_weak_ref(state,
                                             destination,
                                             source->ownershipControl,
                                             source->value.object)) {
                ownership_reset_value_storage(destination);
            }
            break;
        case ZR_OWNERSHIP_VALUE_KIND_UNIQUE:
        case ZR_OWNERSHIP_VALUE_KIND_USING:
            control = source->ownershipControl;
            if (control != ZR_NULL) {
                control->strongRefCount++;
            }
            ownership_set_value_from_object(destination, source->value.object,
                                            ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
            break;
        default:
            ownership_copy_plain_bits(destination, source);
            break;
    }

    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
}

void ZrCore_Ownership_NotifyObjectReleased(struct SZrState *state, struct SZrRawObject *object) {
    SZrOwnershipControl *control;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    control = object->ownershipControl;
    if (control == ZR_NULL) {
        return;
    }

    ownership_expire_weak_refs(state, control);
    control->strongRefCount = 0;
    control->isDetachedFromGc = ZR_FALSE;
    control->object = ZR_NULL;
    object->ownershipControl = ZR_NULL;
    ownership_try_free_control(state, control);
}

static TZrBool ownership_native_get_argument(struct SZrState *state, SZrTypeValue **outResult, SZrTypeValue **outArg) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL || outResult == ZR_NULL || outArg == ZR_NULL) {
        return ZR_FALSE;
    }

    base = state->callInfoList->functionBase.valuePointer;
    if (base == ZR_NULL) {
        return ZR_FALSE;
    }

    *outResult = ZrCore_Stack_GetValue(base);
    *outArg = ZrCore_Stack_GetValue(base + 1);
    return ZR_TRUE;
}

TZrInt64 ZrCore_Ownership_NativeUnique(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    if (!ZrCore_Ownership_UniqueValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrCore_Ownership_NativeShared(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    ownership_prepare_destination(state, result);
    if (!ZrCore_Ownership_ShareValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrCore_Ownership_NativeWeak(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    ownership_prepare_destination(state, result);
    if (!ZrCore_Ownership_WeakValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrCore_Ownership_NativeUsing(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    if (!ZrCore_Ownership_UsingValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}
