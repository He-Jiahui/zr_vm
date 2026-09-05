//
// Created by Codex on 2026/3/30.
//

#include "zr_vm_core/ownership.h"

#include "ownership_resource_internal.h"
#include "ownership_shared_internal.h"
#include "gc/gc_domain_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
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

static void ownership_set_weak_value(SZrTypeValue *value,
                                     EZrValueType targetType,
                                     SZrOwnershipControl *control) {
    if (value == ZR_NULL || control == ZR_NULL) {
        return;
    }
    value->type = targetType;
    value->value.nativeObject.nativePointer = control;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_WEAK;
    value->ownershipControl = control;
    value->ownershipWeakRef = ZR_NULL;
}

static void ownership_notify_strong_ref_delta(SZrState *state,
                                              SZrRawObject *object,
                                              TZrInt32 delta) {
    if (state == ZR_NULL ||
        state->global == ZR_NULL ||
        object == ZR_NULL ||
        state->global->ownershipStrongRefObserver == ZR_NULL ||
        delta == 0) {
        return;
    }

    state->global->ownershipStrongRefObserver(state,
                                              object,
                                              delta,
                                              state->global->ownershipStrongRefObserverUserData);
}

static TZrBool ownership_add_strong_ref(SZrState *state, SZrOwnershipControl *control) {
    SZrRawObject *object = control != ZR_NULL ? control->object : ZR_NULL;
    if (!ZrCore_OwnershipShared_RetainStrong(state, control)) {
        return ZR_FALSE;
    }
    ownership_notify_strong_ref_delta(state, object, 1);
    return ZR_TRUE;
}

static TZrBool ownership_set_initial_strong_ref(SZrState *state, SZrOwnershipControl *control) {
    TZrUInt32 previousCount;
    SZrRawObject *object = control != ZR_NULL ? control->object : ZR_NULL;

    if (!ZrCore_OwnershipShared_SetInitialStrong(state, control, &previousCount)) {
        return ZR_FALSE;
    }
    if (previousCount == 0) {
        ownership_notify_strong_ref_delta(state, object, 1);
    } else if (previousCount > 1) {
        ownership_notify_strong_ref_delta(state, object, -((TZrInt32)(previousCount - 1)));
    }
    return ZR_TRUE;
}

static TZrBool ownership_release_strong_ref(SZrState *state,
                                            SZrOwnershipControl *control,
                                            SZrRawObject **outFinalObject) {
    SZrRawObject *object = control != ZR_NULL ? control->object : ZR_NULL;
    TZrUInt32 previousCount = control != ZR_NULL ? control->strongRefCount : 0U;
    TZrBool isFinal;

    if (control == ZR_NULL || control->strongRefCount == 0U) {
        return ZR_FALSE;
    }
    isFinal = ZrCore_OwnershipShared_ReleaseStrong(state, control, outFinalObject);
    if (control->strongRefCount + 1U == previousCount) {
        ownership_notify_strong_ref_delta(state, object, -1);
    }
    return isFinal;
}

static TZrBool ownership_ignore_object_if_needed(struct SZrState *state, SZrOwnershipControl *control) {
    if (state == ZR_NULL || control == ZR_NULL || control->object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_OwnershipResource_IsObject(control->object)) {
        return ZrCore_GcDomain_RegisterOwnershipRoot(state, control->object);
    }

    if (!control->ownsGcIgnore) {
        if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, control->object)) {
            control->ownsGcIgnore = ZR_TRUE;
            return ZR_TRUE;
        }
        if (!ZrCore_GarbageCollector_IgnoreObject(state, control->object)) {
            return ZR_FALSE;
        }
        control->ownsGcIgnore = ZR_TRUE;
    }

    return ZR_TRUE;
}

static void ownership_return_object_to_gc(struct SZrState *state,
                                          SZrOwnershipControl *control,
                                          SZrRawObject *object) {
    if (state == ZR_NULL || control == ZR_NULL || object == ZR_NULL) {
        return;
    }

    if (control->ownsGcIgnore) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, object);
        control->ownsGcIgnore = ZR_FALSE;
    }
}

static ZR_FORCE_INLINE TZrBool ownership_value_is_plain_primitive(const SZrTypeValue *value) {
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
              (value->ownershipControl == ZR_NULL && value->ownershipWeakRef == ZR_NULL));
    return !value->isGarbageCollectable && value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE;
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

static TZrBool ownership_copy_plain_value(SZrState *state, SZrTypeValue *destination, const SZrTypeValue *source) {
    SZrObject *sourceObject;
    SZrObject *clonedStruct;

    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    if (state != ZR_NULL &&
        source->type == ZR_VALUE_TYPE_OBJECT &&
        source->isGarbageCollectable &&
        source->value.object != ZR_NULL) {
        sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject != ZR_NULL && sourceObject->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT) {
            clonedStruct = ZrCore_Object_CloneStruct(state, sourceObject);
            if (clonedStruct == ZR_NULL) {
                ownership_reset_value_storage(destination);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(clonedStruct));
            destination->type = ZR_VALUE_TYPE_OBJECT;
            return ZR_TRUE;
        }
    }

    ownership_copy_plain_bits(destination, source);
    return ZR_TRUE;
}

static TZrBool ownership_prepare_destination(struct SZrState *state, SZrTypeValue *destination) {
    ZR_ASSERT(destination != ZR_NULL);

    if (destination->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE) {
        ZR_ASSERT(destination->ownershipControl == ZR_NULL);
        ZR_ASSERT(destination->ownershipWeakRef == ZR_NULL);
        ownership_reset_value_storage(destination);
        return ZR_TRUE;
    }

    ZrCore_Ownership_ReleaseValue(state, destination);
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

    if (ZrCore_OwnershipResource_IsObject(object)) {
        return ZrCore_OwnershipResource_InitUnique(state, destination, object);
    }

    control = ZrCore_OwnershipShared_GetOrCreateControl(state, object);
    if (control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_ignore_object_if_needed(state, control)) {
        return ZR_FALSE;
    }

    if (!ownership_set_initial_strong_ref(state, control)) {
        return ZR_FALSE;
    }
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

    if (destination == source) {
        return ZR_TRUE;
    }
    ownership_prepare_destination(state, destination);
    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZrCore_OwnershipResource_MoveUnique(state, destination, source);
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

TZrBool ZrCore_Ownership_BorrowValue(struct SZrState *state,
                                     SZrTypeValue *destination,
                                     SZrTypeValue *source) {
    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    ownership_set_value_from_object(destination,
                                    source->value.object,
                                    ZR_OWNERSHIP_VALUE_KIND_BORROWED,
                                    source->ownershipControl);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_LoanValue(struct SZrState *state,
                                   SZrTypeValue *destination,
                                   SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destination == source) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZrCore_OwnershipResource_LoanUnique(state, destination, source);
    }

    if (!ownership_value_has_object(source) ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_UNIQUE) {
        return ZR_FALSE;
    }

    control = source->ownershipControl;
    object = source->value.object;
    if (control == ZR_NULL || object == ZR_NULL) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    ownership_reset_value_storage(source);
    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_LOANED, control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_ReturnLoanValue(struct SZrState *state,
                                         SZrTypeValue *destination,
                                         SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destination == source) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (ZrCore_OwnershipResource_IsDirectLoanedValue(source)) {
        return ZrCore_OwnershipResource_ReturnLoan(state, destination, source);
    }

    if (!ownership_value_has_object(source) ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_LOANED) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    control = source->ownershipControl;
    object = source->value.object;
    if (control == ZR_NULL || object == ZR_NULL) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    ownership_reset_value_storage(source);
    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_UNIQUE, control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
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

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source) ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_UNIQUE) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = source->ownershipControl;
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (control == ZR_NULL) {
        control = ZrCore_OwnershipShared_GetOrCreateControl(state, object);
        if (control == ZR_NULL || !ownership_set_initial_strong_ref(state, control)) {
            return ZR_FALSE;
        }
    }

    if (!ownership_ignore_object_if_needed(state, control)) {
        return ZR_FALSE;
    }

    ownership_reset_value_storage(source);
    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_SharePlainValue(struct SZrState *state,
                                         SZrTypeValue *destination,
                                         SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source) ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    object = source->value.object;
    control = ZrCore_OwnershipShared_GetOrCreateControl(state, object);
    if (control == ZR_NULL) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    if (!ownership_ignore_object_if_needed(state, control)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    if (!ownership_add_strong_ref(state, control)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }
    ownership_set_value_from_object(destination, object, ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_DegradeValue(struct SZrState *state,
                                   SZrTypeValue *destination,
                                   SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ZR_TRUE;
    }

    if (!ownership_value_has_object(source) ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_SHARED) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = source->ownershipControl;
    if (control == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_OwnershipShared_RetainWeak(state, control)) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }
    ownership_set_weak_value(destination, source->type, control);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_WakeValue(struct SZrState *state,
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

    if (source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_WEAK) {
        ownership_reset_value_storage(destination);
        return ZR_FALSE;
    }

    control = source->ownershipControl;
    if (control == ZR_NULL ||
        !control->objectIsAlive ||
        control->dropInProgress ||
        control->object == ZR_NULL ||
        control->strongRefCount == 0U) {
        ownership_reset_value_storage(destination);
        return ZR_TRUE;
    }

    if (!ownership_add_strong_ref(state, control)) {
        ownership_reset_value_storage(destination);
        return ZR_TRUE;
    }
    ownership_set_value_from_object(destination,
                                    control->object,
                                    ZR_OWNERSHIP_VALUE_KIND_SHARED,
                                    control);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_DetachValue(struct SZrState *state,
                                     SZrTypeValue *destination,
                                     SZrTypeValue *source) {
    return ZrCore_Ownership_ReturnToGcValue(state, destination, source);
}

TZrBool ZrCore_Ownership_ReturnToGcValue(struct SZrState *state,
                                         SZrTypeValue *destination,
                                         SZrTypeValue *source) {
    SZrOwnershipControl *control;
    SZrRawObject *object;
    SZrRawObject *finalObject = ZR_NULL;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        return ownership_prepare_destination(state, destination);
    }

    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    if (!ownership_value_has_object(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    control = source->ownershipControl;
    if (control == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_UNIQUE &&
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_SHARED) {
        return ZR_FALSE;
    }

    if (source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED && control->strongRefCount != 1) {
        return ZR_FALSE;
    }

    ownership_return_object_to_gc(state, control, object);
    if (!ownership_release_strong_ref(state, control, &finalObject) ||
        finalObject != object) {
        return ZR_FALSE;
    }

    ownership_reset_value_storage(source);
    ZrCore_Value_InitAsRawObject(state, destination, object);
    ZrCore_OwnershipShared_FinishFinalStrong(state, control, object);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_IntoGcBoxValue(struct SZrState *state,
                                        SZrTypeValue *destination,
                                        SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destination == source ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }
    if (!ownership_prepare_destination(state, destination)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    if (!ZrCore_GcDomain_ObjectBelongsToState(state, object)) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_UnregisterOwnershipRoot(state, object);
    object->isGcBox = ZR_TRUE;
    ownership_reset_value_storage(source);
    ZrCore_Value_InitAsRawObject(state, destination, object);
    return ZR_TRUE;
}

TZrBool ZrCore_Ownership_IsGcBoxObject(const SZrRawObject *object) {
    return object != ZR_NULL && object->isGcBox &&
           ZrCore_OwnershipResource_IsObject(object) &&
           object->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_ALIVE;
}

void ZrCore_Ownership_ReleaseValue(struct SZrState *state, SZrTypeValue *value) {
    SZrOwnershipControl *control;
    EZrOwnershipValueKind kind;
    SZrRawObject *object;

    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if ((value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
         value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK) &&
        !ZrCore_OwnershipShared_IsInIsolationDomain(state, value->ownershipControl)) {
        return;
    }

    if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK) {
        control = value->ownershipControl;
        ownership_reset_value_storage(value);
        ZrCore_OwnershipShared_ReleaseWeak(state, control);
        return;
    }

    if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_BORROWED) {
        ownership_reset_value_storage(value);
        return;
    }

    control = value->ownershipControl;
    kind = value->ownershipKind;
    object = ownership_value_has_object(value) ? value->value.object : ZR_NULL;
    ownership_reset_value_storage(value);
    if (control == ZR_NULL &&
        (kind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
         kind == ZR_OWNERSHIP_VALUE_KIND_LOANED) &&
        ZrCore_OwnershipResource_IsObject(object)) {
        ZrCore_OwnershipResource_Drop(state, object);
        return;
    }
    if (control == ZR_NULL) {
        return;
    }

    if ((kind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
         kind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
         kind == ZR_OWNERSHIP_VALUE_KIND_LOANED) &&
        control->strongRefCount > 0) {
        SZrRawObject *finalObject = ZR_NULL;
        if (ownership_release_strong_ref(state, control, &finalObject)) {
            if (ZrCore_OwnershipResource_IsObject(finalObject)) {
                ZrCore_OwnershipResource_Drop(state, finalObject);
                control->ownsGcIgnore = ZR_FALSE;
            } else {
                ownership_return_object_to_gc(state, control, finalObject);
            }
            ZrCore_OwnershipShared_FinishFinalStrong(
                    state, control, finalObject);
        }
    }
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
    TZrBool destinationNeedsPrepare;
    TZrBool sourceIsPlainValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    ZR_ASSERT(source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
              (source->ownershipControl == ZR_NULL && source->ownershipWeakRef == ZR_NULL));
    ZR_ASSERT(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
              (destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL));

    if (destination == source) {
        return;
    }

    sourceIsPlainValue = (TZrBool)(source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE);
    destinationNeedsPrepare = (TZrBool)(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE);

    if (!destinationNeedsPrepare && ownership_value_is_plain_primitive(source)) {
        *destination = *source;
        return;
    }

    if (destinationNeedsPrepare) {
        ownership_prepare_destination(state, destination);
    }

    if (sourceIsPlainValue) {
        if (!source->isGarbageCollectable) {
            ownership_copy_plain_bits(destination, source);
            return;
        }

        if (!ownership_copy_plain_value(state, destination, source)) {
            ownership_reset_value_storage(destination);
        }
        if (destination->isGarbageCollectable) {
            ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
        }
        return;
    }

    switch (source->ownershipKind) {
        case ZR_OWNERSHIP_VALUE_KIND_SHARED:
            control = source->ownershipControl;
            if (control == ZR_NULL || !ownership_add_strong_ref(state, control)) {
                ownership_reset_value_storage(destination);
                break;
            }
            ownership_set_value_from_object(destination, source->value.object,
                                            ZR_OWNERSHIP_VALUE_KIND_SHARED, control);
            break;
        case ZR_OWNERSHIP_VALUE_KIND_WEAK:
            control = source->ownershipControl;
            if (!ZrCore_OwnershipShared_RetainWeak(state, control)) {
                ownership_reset_value_storage(destination);
            } else {
                ownership_set_weak_value(destination, source->type, control);
            }
            break;
        case ZR_OWNERSHIP_VALUE_KIND_BORROWED:
            ownership_set_value_from_object(destination,
                                            source->value.object,
                                            ZR_OWNERSHIP_VALUE_KIND_BORROWED,
                                            source->ownershipControl);
            break;
        case ZR_OWNERSHIP_VALUE_KIND_UNIQUE:
        case ZR_OWNERSHIP_VALUE_KIND_LOANED:
            control = source->ownershipControl;
            if (ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
                ZrCore_OwnershipResource_CopyUnique(destination, source);
                break;
            }
            if (control != ZR_NULL) {
                if (!ownership_add_strong_ref(state, control)) {
                    ownership_reset_value_storage(destination);
                    break;
                }
            }
            ownership_set_value_from_object(destination,
                                            source->value.object,
                                            source->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED
                                                    ? ZR_OWNERSHIP_VALUE_KIND_LOANED
                                                    : ZR_OWNERSHIP_VALUE_KIND_SHARED,
                                            control);
            break;
        default:
            ownership_copy_plain_bits(destination, source);
            break;
    }

    if (destination->isGarbageCollectable) {
        ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    }
}

void ZrCore_Ownership_NotifyObjectReleased(struct SZrState *state, struct SZrRawObject *object) {
    SZrOwnershipControl *control;
    TZrUInt32 strongRefCount;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    if (object->isGcBox) {
        object->isGcBox = ZR_FALSE;
        ZrCore_RawObject_MarkAsInit(state, object);
        if (ZrCore_GcDomain_RegisterOwnershipRoot(state, object)) {
            ZrCore_OwnershipResource_Drop(state, object);
        }
    }

    control = object->ownershipControl;
    if (control == ZR_NULL) {
        return;
    }

    strongRefCount = control->strongRefCount;
    if (strongRefCount > 0U) {
        ownership_notify_strong_ref_delta(
                state, object, -((TZrInt32)strongRefCount));
    }
    ZrCore_OwnershipShared_InvalidateObject(state, control, object);
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

TZrInt64 ZrCore_Ownership_NativeShare(struct SZrState *state) {
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

TZrInt64 ZrCore_Ownership_NativeSharePlain(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    ownership_prepare_destination(state, result);
    if (!ZrCore_Ownership_SharePlainValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}

TZrInt64 ZrCore_Ownership_NativeDegrade(struct SZrState *state) {
    SZrTypeValue *result;
    SZrTypeValue *arg;

    if (!ownership_native_get_argument(state, &result, &arg)) {
        return 0;
    }

    ownership_prepare_destination(state, result);
    if (!ZrCore_Ownership_DegradeValue(state, result, arg)) {
        ownership_reset_value_storage(result);
    }
    state->stackTop.valuePointer = state->callInfoList->functionBase.valuePointer + 1;
    return 1;
}
