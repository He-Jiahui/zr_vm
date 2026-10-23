/* Object storage writes own their fallible work and transient roots here.
 * The caller of TryRun owns this context: longjmp never invalidates its flags. */
#include "object/object_write_internal.h"
#include "object/object_internal.h"
#include "gc/gc_domain_internal.h"
#include "gc/gc_temporary_roots.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

typedef struct SZrObjectWrite {
    SZrObject *object;
    const SZrTypeValue *key;
    const SZrTypeValue *value;
    const TZrChar *fieldName;
    SZrTypeValue normalizedKey;
    SZrTypeValue preparedValue;
    TZrBool objectStackRooted;
    TZrBool keyStackRooted;
    TZrBool valueStackRooted;
    TZrBool skipWriteBarrier;
    TZrBool succeeded;
    TZrBool preparedValueTransferred;
    EZrThreadStatus failureStatus;
} SZrObjectWrite;

static TZrBool object_write_root_value(SZrState *state, const SZrTypeValue *value,
                                      TZrBool stackRooted) {
    if (stackRooted || !ZrCore_Value_IsGarbageCollectable(value) ||
        ZrCore_Value_GetRawObject(value) == ZR_NULL) return ZR_TRUE;
    return ZrCore_Gc_TemporaryRootPush(state, ZrCore_Value_GetRawObject(value));
}

static void object_write_run(SZrState *state, TZrPtr data) {
    SZrObjectWrite *write = (SZrObjectWrite *)data;
    const SZrTypeValue *storageKey = write->key;
    SZrHashKeyValuePair *pair;
    TZrBool mutationLocked;
    TZrBool newPair;

    if ((!write->objectStackRooted &&
         !ZrCore_Gc_TemporaryRootPush(state, &write->object->super)) ||
        !object_write_root_value(state, write->value, write->valueStackRooted)) return;

    if (write->fieldName != ZR_NULL) {
        SZrString *name = ZrCore_String_CreateFromNative(state, (TZrNativeString)write->fieldName);
        if (name == ZR_NULL) return;
        ZrCore_Value_InitAsRawObject(state, &write->normalizedKey, &name->super);
        write->normalizedKey.type = ZR_VALUE_TYPE_STRING;
        storageKey = &write->normalizedKey;
    } else {
        if (!object_write_root_value(state, write->key, write->keyStackRooted)) return;
        if (object_try_get_direct_storage_key_unchecked(write->key) == ZR_NULL &&
            write->key->type == ZR_VALUE_TYPE_STRING && write->key->value.object != ZR_NULL) {
            SZrString *input = (SZrString *)ZrCore_Value_GetRawObject(write->key);
            SZrString *canonical = ZrCore_String_Create(state, ZrCore_String_GetNativeString(input),
                                                       ZrCore_String_GetByteLength(input));
            if (canonical == ZR_NULL) return;
            ZrCore_Value_InitAsRawObject(state, &write->normalizedKey, &canonical->super);
            write->normalizedKey.type = ZR_VALUE_TYPE_STRING;
            storageKey = &write->normalizedKey;
        }
    }
    if (!object_write_root_value(state, storageKey,
                                (TZrBool)(storageKey == write->key && write->keyStackRooted))) return;
    if (!object_node_map_is_ready(write->object)) {
        ZrCore_Object_Init(state, write->object);
        if (!object_node_map_is_ready(write->object)) return;
    }
    if (!ZrCore_Object_PrepareGenericStorageWrite(state, write->object, storageKey,
                                                 &write->skipWriteBarrier)) return;
    mutationLocked = ZrCore_GcDomain_MutationBegin(state);
    pair = ZrCore_HashSet_Find(state, &write->object->nodeMap, storageKey);
    newPair = (TZrBool)(pair == ZR_NULL);
    if (newPair) {
        /* Copy can clone a plain struct and fail without throwing. Do not
         * publish even an empty pair until that fallible work has completed. */
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        ZrCore_Value_Copy(state, &write->preparedValue, write->value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE ||
            (!ZR_VALUE_IS_TYPE_NULL(write->value->type) &&
             ZR_VALUE_IS_TYPE_NULL(write->preparedValue.type))) {
            if (state->threadStatus != ZR_THREAD_STATUS_FINE)
                write->failureStatus = state->threadStatus;
            ZrCore_GcDomain_MutationEnd(state, mutationLocked);
            return;
        }
        if (!object_write_root_value(state, &write->preparedValue, ZR_FALSE)) {
            ZrCore_GcDomain_MutationEnd(state, mutationLocked);
            return;
        }
        pair = ZrCore_HashSet_Add(state, &write->object->nodeMap, storageKey);
    }
    if (pair != ZR_NULL) {
        /* The new-pair bridge first transfers these bits, with no intervening
         * fallible work. The indexed pair owns any retained reference from
         * that point, including if a later barrier exits nonlocally. */
        if (newPair) write->preparedValueTransferred = ZR_TRUE;
        ZrCore_Object_CommitStoragePair(state, write->object, storageKey,
                                       newPair ? &write->preparedValue : write->value,
                                       pair, newPair, write->skipWriteBarrier);
        write->succeeded = ZR_TRUE;
    }
    /* A Throw restores this level to the TryRun snapshot instead. */
    ZrCore_GcDomain_MutationEnd(state, mutationLocked);
}

static void object_write_discard_prepared_value(SZrState *state, TZrPtr data) {
    SZrObjectWrite *write = (SZrObjectWrite *)data;
    /* Only an untransferred, retained control reaches this callback. */
    ZrCore_Ownership_ReleaseValue(state, &write->preparedValue);
    ZrCore_Value_ResetAsNullNoProfile(&write->preparedValue);
}

static TZrBool object_write_execute(SZrState *state, SZrObjectWrite *write) {
    EZrThreadStatus status;
    EZrThreadStatus cleanupStatus;
    if (state == ZR_NULL || state->global == ZR_NULL || write->object == ZR_NULL ||
        write->value == ZR_NULL || (write->key == ZR_NULL && write->fieldName == ZR_NULL)) {
        if (state != ZR_NULL) state->threadStatus = ZR_THREAD_STATUS_INVALID;
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_ValidateValueWrite(state, &write->object->super, write->value) ||
        (write->key != ZR_NULL &&
         !ZrCore_GcDomain_ValidateValueWrite(state, &write->object->super, write->key))) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return ZR_FALSE;
    }
    write->failureStatus = ZR_THREAD_STATUS_MEMORY_ERROR;
    TZrSize rootDepth = state->temporaryRootLength;
    status = ZrCore_Exception_TryRun(state, object_write_run, write);
    /* Cleanup itself may release an ownership handle. Its local boundary
     * guarantees the temporary roots are restored even if that release throws. */
    cleanupStatus = ZR_THREAD_STATUS_FINE;
    if (!write->preparedValueTransferred &&
        write->preparedValue.ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE &&
        write->preparedValue.ownershipControl != ZR_NULL) {
        cleanupStatus = ZrCore_Exception_TryRun(state, object_write_discard_prepared_value, write);
    } else {
        /* Plain GC values acquired no ownership ref. CopyUnique's control-less
         * slot only mirrors the caller's direct owner: never drop it here. */
        ZrCore_Value_ResetAsNullNoProfile(&write->preparedValue);
    }
    ZrCore_Gc_TemporaryRootsRestore(state, rootDepth);
    if (status == ZR_THREAD_STATUS_FINE && cleanupStatus != ZR_THREAD_STATUS_FINE) status = cleanupStatus;
    if (status != ZR_THREAD_STATUS_FINE) {
        state->threadStatus = status;
        return ZR_FALSE;
    }
    state->threadStatus = write->succeeded ? ZR_THREAD_STATUS_FINE : write->failureStatus;
    return write->succeeded;
}

TZrBool ZrCore_Object_TrySetValueWithStorageOptions(
        SZrState *state, SZrObject *object, const SZrTypeValue *key,
        const SZrTypeValue *value, TZrBool objectStackRooted,
        TZrBool keyStackRooted, TZrBool valueStackRooted, TZrBool skipWriteBarrier) {
    SZrObjectWrite write = {0};
    write.object = object;
    write.key = key;
    write.value = value;
    write.objectStackRooted = objectStackRooted;
    write.keyStackRooted = keyStackRooted;
    write.valueStackRooted = valueStackRooted;
    write.skipWriteBarrier = skipWriteBarrier;
    return object_write_execute(state, &write);
}

TZrBool ZrCore_Object_TrySetValue(SZrState *state, SZrObject *object,
                                const SZrTypeValue *key, const SZrTypeValue *value) {
    return ZrCore_Object_TrySetValueWithStorageOptions(
            state, object, key, value, ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
}

TZrBool ZrCore_Object_TrySetFieldCString(SZrState *state, SZrObject *object,
                                       const TZrChar *fieldName, const SZrTypeValue *value) {
    SZrObjectWrite write = {0};
    write.object = object;
    write.fieldName = fieldName;
    write.value = value;
    return object_write_execute(state, &write);
}

void ZrCore_Object_SetValue(SZrState *state, SZrObject *object,
                           const SZrTypeValue *key, const SZrTypeValue *value) {
    if (!ZrCore_Object_TrySetValue(state, object, key, value) && state != ZR_NULL) {
        ZrCore_Exception_Throw(state, state->threadStatus);
    }
}
