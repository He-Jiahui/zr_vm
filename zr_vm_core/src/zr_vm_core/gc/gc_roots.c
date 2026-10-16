/* Shared ignored-root registry and independently owned temporary leases. */
#include "gc/gc_internal.h"
#include "gc/gc_domain_internal.h"

/* All helpers below require the domain mutation lock. Raw allocations here
 * neither collect nor poll; reserve finishes before object metadata changes. */
static TZrBool root_insert(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    TZrSize index;
    if (garbage_collector_ignore_registry_contains(collector, object)) return ZR_TRUE;
    if (collector->ignoredObjectCount == ZR_MAX_SIZE ||
        !garbage_collector_ensure_ignore_registry_capacity(state->global,
                collector->ignoredObjectCount + 1)) return ZR_FALSE;
    index = collector->ignoredObjectCount++;
    collector->ignoredObjects[index] = object;
    object->garbageCollectMark.ignoredRegistryIndex = index;
    garbage_collector_mark_ignored_root_if_needed_fast(state, object);
    return ZR_TRUE;
}

static TZrBool root_acquire(SZrState *state, SZrRawObject *object) {
    if (object->garbageCollectMark.temporaryRootRefCount == ZR_MAX_SIZE ||
        !root_insert(state, object)) return ZR_FALSE;
    object->garbageCollectMark.temporaryRootRefCount++;
    return ZR_TRUE;
}

static void root_release(SZrGlobalState *global, SZrRawObject *object) {
    if (object->garbageCollectMark.temporaryRootRefCount == 0) return;
    object->garbageCollectMark.temporaryRootRefCount--;
    if (object->garbageCollectMark.temporaryRootRefCount == 0 &&
        !object->garbageCollectMark.explicitIgnoredRoot)
        garbage_collector_ignore_registry_remove_object(global->garbageCollector, object);
}

TZrBool ZrCore_Gc_RootLeaseAcquire(SZrState *state, SZrRawObject *object, SZrGcRootLease *lease) {
    TZrBool succeeded;
    if (lease == ZR_NULL || lease->object != ZR_NULL) return ZR_FALSE;
    if (object == ZR_NULL) return ZR_TRUE;
    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL ||
        !ZrCore_GcDomain_SharedMutationBegin(state)) return ZR_FALSE;
    succeeded = root_acquire(state, object);
    if (succeeded) lease->object = object;
    ZrCore_GcDomain_MutationEnd(state, ZR_TRUE);
    return succeeded;
}

void ZrCore_Gc_RootLeaseRelease(SZrGlobalState *global, SZrGcRootLease *lease) {
    if (lease == ZR_NULL || lease->object == ZR_NULL || global == ZR_NULL ||
        global->garbageCollector == ZR_NULL) return;
    ZrCore_GcDomain_MutationLock(global->gcDomain);
    root_release(global, lease->object);
    lease->object = ZR_NULL;
    ZrCore_GcDomain_MutationUnlock(global->gcDomain);
}

TZrBool ZrCore_GarbageCollector_IgnoreObject(SZrState *state, SZrRawObject *object) {
    TZrBool succeeded;
    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL ||
        !ZrCore_GcDomain_SharedMutationBegin(state)) return ZR_FALSE;
    succeeded = root_insert(state, object);
    if (succeeded) object->garbageCollectMark.explicitIgnoredRoot = ZR_TRUE;
    ZrCore_GcDomain_MutationEnd(state, ZR_TRUE);
    return succeeded;
}

TZrBool ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(SZrGlobalState *global,
        SZrState *state, SZrRawObject *object, TZrBool *outAddedByCaller) {
    TZrBool succeeded = ZR_TRUE;
    if (outAddedByCaller != ZR_NULL) *outAddedByCaller = ZR_FALSE;
    if (global == ZR_NULL || state == ZR_NULL || state->global != global ||
        global->garbageCollector == ZR_NULL || object == ZR_NULL ||
        !ZrCore_GcDomain_SharedMutationBegin(state)) return ZR_FALSE;
    if (!garbage_collector_ignore_registry_contains(global->garbageCollector, object)) {
        succeeded = root_insert(state, object);
        if (succeeded) {
            object->garbageCollectMark.explicitIgnoredRoot = ZR_TRUE;
            if (outAddedByCaller != ZR_NULL) *outAddedByCaller = ZR_TRUE;
        }
    }
    ZrCore_GcDomain_MutationEnd(state, ZR_TRUE);
    return succeeded;
}

TZrBool ZrCore_GarbageCollector_UnignoreObject(SZrGlobalState *global, SZrRawObject *object) {
    TZrBool contained;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) return ZR_FALSE;
    ZrCore_GcDomain_MutationLock(global->gcDomain);
    contained = garbage_collector_ignore_registry_contains(global->garbageCollector, object);
    object->garbageCollectMark.explicitIgnoredRoot = ZR_FALSE;
    if (object->garbageCollectMark.temporaryRootRefCount == 0)
        garbage_collector_ignore_registry_remove_object(global->garbageCollector, object);
    ZrCore_GcDomain_MutationUnlock(global->gcDomain);
    return contained;
}

TZrBool ZrCore_GarbageCollector_IsObjectIgnored(SZrGlobalState *global, SZrRawObject *object) {
    TZrBool contained;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) return ZR_FALSE;
    ZrCore_GcDomain_MutationLock(global->gcDomain);
    contained = garbage_collector_ignore_registry_contains(global->garbageCollector, object);
    ZrCore_GcDomain_MutationUnlock(global->gcDomain);
    return contained;
}

static void reset_native_pin(SZrGcNativeCallPin *pin) {
    pin->object = ZR_NULL;
    pin->pinKind = ZR_GARBAGE_COLLECT_PIN_KIND_NONE;
    pin->ignoredAddedByCaller = ZR_FALSE;
    pin->pinKindAddedByCaller = ZR_FALSE;
}

TZrBool ZrCore_Gc_NativeCallPinObject(SZrState *state, SZrRawObject *object, SZrGcNativeCallPin *pin) {
    TZrBool succeeded = ZR_FALSE;
    TZrBool addedRoot;
    TZrBool ownsPin;
    if (pin == ZR_NULL) return ZR_FALSE;
    /* Native callers historically pass uninitialized stack tokens. */
    reset_native_pin(pin);
    if (object == ZR_NULL) return ZR_TRUE;
    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL ||
        !ZrCore_GcDomain_SharedMutationBegin(state)) return ZR_FALSE;
    if (object->garbageCollectMark.nativeCallPinRefCount == ZR_MAX_SIZE) goto done;
    addedRoot = !garbage_collector_ignore_registry_contains(state->global->garbageCollector, object);
    if (!root_acquire(state, object)) goto done;
    ownsPin = (TZrBool)((object->garbageCollectMark.pinFlags &
                        ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0);
    if (object->garbageCollectMark.nativeCallPinRefCount == 0) {
        if (!garbage_collector_try_pin_object_locked(state, object,
                    ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE)) {
            root_release(state->global, object);
            goto done;
        }
        object->garbageCollectMark.nativeCallOwnsPinKind = ownsPin;
    }
    object->garbageCollectMark.nativeCallPinRefCount++;
    pin->object = object;
    pin->pinKind = ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE;
    pin->ignoredAddedByCaller = addedRoot;
    pin->pinKindAddedByCaller = ownsPin;
    succeeded = ZR_TRUE;
done:
    ZrCore_GcDomain_MutationEnd(state, ZR_TRUE);
    return succeeded;
}

TZrBool ZrCore_Gc_NativeCallPinValue(SZrState *state, const SZrTypeValue *value,
                                    SZrGcNativeCallPin *pin) {
    return ZrCore_Gc_NativeCallPinObject(state,
            value != ZR_NULL && ZrCore_Value_IsGarbageCollectable(value)
                    ? ZrCore_Value_GetRawObject(value) : ZR_NULL, pin);
}

void ZrCore_Gc_NativeCallUnpin(SZrGlobalState *global, SZrGcNativeCallPin *pin) {
    SZrRawObject *object;
    if (pin == ZR_NULL || pin->object == ZR_NULL || global == ZR_NULL ||
        global->garbageCollector == ZR_NULL) return;
    ZrCore_GcDomain_MutationLock(global->gcDomain);
    object = pin->object;
    if (object->garbageCollectMark.nativeCallPinRefCount > 0) {
        object->garbageCollectMark.nativeCallPinRefCount--;
        if (object->garbageCollectMark.nativeCallPinRefCount == 0) {
            if (object->garbageCollectMark.nativeCallOwnsPinKind)
                object->garbageCollectMark.pinFlags &=
                        (TZrUInt32)~(TZrUInt32)ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE;
            object->garbageCollectMark.nativeCallOwnsPinKind = ZR_FALSE;
        }
        root_release(global, object);
    }
    reset_native_pin(pin);
    ZrCore_GcDomain_MutationUnlock(global->gcDomain);
}
