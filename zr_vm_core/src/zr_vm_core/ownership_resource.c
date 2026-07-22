#include "ownership_resource_internal.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

static void ownership_resource_reset_value(SZrTypeValue *value) {
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

static void ownership_resource_set_direct_unique(SZrTypeValue *value,
                                                  SZrRawObject *object) {
    if (value == ZR_NULL || object == ZR_NULL) {
        return;
    }

    value->type = (EZrValueType)object->type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_UNIQUE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

TZrBool ZrCore_OwnershipResource_IsObject(const SZrRawObject *object) {
    const SZrObject *zrObject;

    if (object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    zrObject = (const SZrObject *)object;
    return zrObject->prototype != ZR_NULL &&
           (zrObject->prototype->modifierFlags & ZR_TYPE_MODIFIER_FLAG_RESOURCE) != 0;
}

TZrBool ZrCore_OwnershipResource_IsDirectUniqueValue(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE &&
           value->ownershipControl == ZR_NULL &&
           ZrCore_OwnershipResource_IsObject(value->value.object);
}

TZrBool ZrCore_OwnershipResource_InitUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrRawObject *object) {
    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsObject(object)) {
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_RegisterOwnershipRoot(state, object)) {
        return ZR_FALSE;
    }

    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipResource_MoveUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    if (object->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_CONSTRUCTING) {
        object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    }
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

void ZrCore_OwnershipResource_CopyUnique(SZrTypeValue *destination,
                                         const SZrTypeValue *source) {
    if (destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return;
    }
    ownership_resource_set_direct_unique(destination, source->value.object);
}

void ZrCore_OwnershipResource_Drop(SZrState *state, SZrRawObject *object) {
    EZrResourceLifecycleState previousState;
    SZrTypeValue borrowedSelf;
    SZrMeta *destructor;

    if (state == ZR_NULL || !ZrCore_OwnershipResource_IsObject(object)) {
        return;
    }
    previousState = (EZrResourceLifecycleState)object->resourceLifecycleState;
    if (previousState == ZR_RESOURCE_LIFECYCLE_DROPPING ||
        previousState == ZR_RESOURCE_LIFECYCLE_DROPPED) {
        return;
    }

    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPING;
    destructor = ZrCore_Object_GetMetaRecursively(
            state->global, (SZrObject *)object, ZR_META_DESTRUCTOR);
    if (previousState == ZR_RESOURCE_LIFECYCLE_ALIVE &&
        destructor != ZR_NULL && destructor->function != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &borrowedSelf, object);
        borrowedSelf.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
        (void)ZrCore_Value_CallMetaMethod(
                state, &borrowedSelf, ZR_META_DESTRUCTOR, ZR_NULL, 0U);
    }
    ZrCore_Object_DropManagedFields(state, (SZrObject *)object);
    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPED;
    ZrCore_GcDomain_UnregisterOwnershipRoot(state, object);
}
