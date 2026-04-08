#include "runtime_internal.h"

static const TZrChar *kTaskTransferValueField = "__zr_task_transfer_value";
static const TZrChar *kTaskTransferTakenField = "__zr_task_transfer_taken";

static ZrVmTaskSharedCell *zr_vm_task_shared_cell_from_handle(SZrState *state, SZrObject *handle) {
    const SZrTypeValue *fieldValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_NULL;
    }

    fieldValue = zr_vm_task_get_field_value(state, handle, ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        fieldValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return (ZrVmTaskSharedCell *)fieldValue->value.nativeObject.nativePointer;
}

static TZrBool zr_vm_task_shared_handle_is_weak(SZrState *state, SZrObject *handle) {
    return zr_vm_task_get_bool_field(state, handle, ZR_VM_TASK_SHARED_IS_WEAK_FIELD, ZR_FALSE);
}

static ZrVmTaskMutexCell *zr_vm_task_mutex_cell_from_handle(SZrState *state, SZrObject *handle) {
    const SZrTypeValue *fieldValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_NULL;
    }

    fieldValue = zr_vm_task_get_field_value(state, handle, ZR_VM_TASK_MUTEX_NATIVE_CELL_FIELD);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        fieldValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return (ZrVmTaskMutexCell *)fieldValue->value.nativeObject.nativePointer;
}

static TZrUInt32 zr_vm_task_mutex_handle_kind(SZrState *state, SZrObject *handle) {
    return (TZrUInt32)zr_vm_task_get_uint_field(state, handle, ZR_VM_TASK_MUTEX_KIND_FIELD, 0u);
}

static ZrVmTaskMutexCell *zr_vm_task_lock_cell_from_handle(SZrState *state, SZrObject *handle) {
    const SZrTypeValue *fieldValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_NULL;
    }

    fieldValue = zr_vm_task_get_field_value(state, handle, ZR_VM_TASK_LOCK_MUTEX_CELL_FIELD);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        fieldValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return (ZrVmTaskMutexCell *)fieldValue->value.nativeObject.nativePointer;
}

static TZrUInt32 zr_vm_task_lock_kind_from_handle(SZrState *state, SZrObject *handle) {
    return (TZrUInt32)zr_vm_task_get_uint_field(state, handle, ZR_VM_TASK_LOCK_KIND_FIELD, 0u);
}

static TZrBool zr_vm_task_lock_is_active(SZrState *state, SZrObject *handle) {
    return zr_vm_task_get_bool_field(state, handle, ZR_VM_TASK_LOCK_ACTIVE_FIELD, ZR_FALSE);
}

static TZrBool zr_vm_task_shared_cell_is_alive_locked(const ZrVmTaskSharedCell *cell) {
    return cell != ZR_NULL && cell->lifecycleState == ZR_VM_TASK_SHARED_CELL_LIFECYCLE_ALIVE &&
           cell->strongCount > 0u;
}

TZrBool zr_vm_task_shared_cell_is_alive_native(ZrVmTaskSharedCell *cell) {
    TZrBool alive = ZR_FALSE;

    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    alive = zr_vm_task_shared_cell_is_alive_locked(cell);
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return alive;
}

TZrBool zr_vm_task_shared_cell_add_strong_ref(ZrVmTaskSharedCell *cell) {
    TZrBool retained = ZR_FALSE;

    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (zr_vm_task_shared_cell_is_alive_locked(cell)) {
        cell->strongCount++;
        retained = ZR_TRUE;
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return retained;
}

TZrBool zr_vm_task_shared_cell_add_weak_ref(ZrVmTaskSharedCell *cell) {
    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    cell->weakCount++;
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_cell_add_weak_ref_if_alive(ZrVmTaskSharedCell *cell) {
    TZrBool retained = ZR_FALSE;

    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (zr_vm_task_shared_cell_is_alive_locked(cell)) {
        cell->weakCount++;
        retained = ZR_TRUE;
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return retained;
}

void zr_vm_task_shared_cell_release_strong(ZrVmTaskSharedCell *cell) {
    TZrBool freeCell = ZR_FALSE;
    ZrVmTaskTransportValue releasedValue;

    if (cell == ZR_NULL) {
        return;
    }

    memset(&releasedValue, 0, sizeof(releasedValue));
    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (cell->strongCount > 0u) {
        cell->strongCount--;
        if (cell->strongCount == 0u) {
            cell->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DESTROYING;
            releasedValue = cell->value;
            memset(&cell->value, 0, sizeof(cell->value));
            freeCell = cell->weakCount == 0u;
            if (freeCell) {
                cell->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DROPPED;
            }
        }
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    zr_vm_task_transport_clear(&releasedValue);
    if (!freeCell) {
        zr_vm_task_sync_mutex_lock(&cell->mutex);
        if (cell->strongCount == 0u && cell->lifecycleState == ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DESTROYING) {
            cell->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DROPPED;
        }
        zr_vm_task_sync_mutex_unlock(&cell->mutex);
    }
    if (freeCell) {
        zr_vm_task_sync_mutex_destroy(&cell->mutex);
        free(cell);
    }
}

void zr_vm_task_shared_cell_release_weak(ZrVmTaskSharedCell *cell) {
    TZrBool freeCell = ZR_FALSE;

    if (cell == ZR_NULL) {
        return;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (cell->weakCount > 0u) {
        cell->weakCount--;
        freeCell = (TZrBool)(cell->weakCount == 0u && cell->strongCount == 0u);
        if (freeCell) {
            cell->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DROPPED;
        }
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    if (freeCell) {
        zr_vm_task_sync_mutex_destroy(&cell->mutex);
        free(cell);
    }
}

static ZrVmTaskSharedCell *zr_vm_task_shared_cell_new(SZrState *state, const SZrTypeValue *value) {
    ZrVmTaskSharedCell *cell;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cell = (ZrVmTaskSharedCell *)calloc(1, sizeof(*cell));
    if (cell == ZR_NULL) {
        return ZR_NULL;
    }

    zr_vm_task_sync_mutex_init(&cell->mutex);
    cell->strongCount = 1u;
    cell->weakCount = 0u;
    cell->lifecycleState = ZR_VM_TASK_SHARED_CELL_LIFECYCLE_ALIVE;
    if (!zr_vm_task_transport_encode_value(state,
                                           value,
                                           &cell->value,
                                           "Shared only stores Send + Sync values and thread transport handles")) {
        zr_vm_task_sync_mutex_destroy(&cell->mutex);
        free(cell);
        return ZR_NULL;
    }
    return cell;
}

TZrBool zr_vm_task_shared_make_value(SZrState *state,
                                     ZrVmTaskSharedCell *cell,
                                     TZrBool isWeak,
                                     SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue cellValue;
    const TZrChar *typeName = isWeak ? "WeakShared" : "Shared";

    if (state == ZR_NULL || cell == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state, typeName);
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetNativePointer(state, &cellValue, cell);
    zr_vm_task_set_value_field(state, handle, ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD, &cellValue);
    zr_vm_task_set_bool_field(state, handle, ZR_VM_TASK_SHARED_IS_WEAK_FIELD, isWeak);
    return zr_vm_task_finish_object(state, result, handle);
}

TZrBool zr_vm_task_shared_try_get_cell(SZrState *state,
                                       const SZrTypeValue *value,
                                       ZrVmTaskSharedCell **outCell,
                                       TZrBool *outIsWeak) {
    SZrObject *handle;

    if (outCell != ZR_NULL) {
        *outCell = ZR_NULL;
    }
    if (outIsWeak != ZR_NULL) {
        *outIsWeak = ZR_FALSE;
    }
    if (state == ZR_NULL || value == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = ZR_CAST_OBJECT(state, value->value.object);
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outCell != ZR_NULL) {
        *outCell = zr_vm_task_shared_cell_from_handle(state, handle);
    }
    if (outIsWeak != ZR_NULL) {
        *outIsWeak = zr_vm_task_shared_handle_is_weak(state, handle);
    }
    return outCell == ZR_NULL || *outCell != ZR_NULL;
}

TZrBool zr_vm_task_mutex_cell_add_ref(ZrVmTaskMutexCell *cell) {
    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    cell->refCount++;
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return ZR_TRUE;
}

void zr_vm_task_mutex_cell_release(ZrVmTaskMutexCell *cell) {
    TZrBool freeCell = ZR_FALSE;
    ZrVmTaskTransportValue releasedValue;

    if (cell == ZR_NULL) {
        return;
    }

    memset(&releasedValue, 0, sizeof(releasedValue));
    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (cell->refCount > 0u) {
        cell->refCount--;
    }
    if (cell->refCount == 0u && !cell->writerActive && cell->readerCount == 0u) {
        releasedValue = cell->value;
        memset(&cell->value, 0, sizeof(cell->value));
        freeCell = ZR_TRUE;
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    zr_vm_task_transport_clear(&releasedValue);
    if (freeCell) {
        zr_vm_task_sync_condition_destroy(&cell->condition);
        zr_vm_task_sync_mutex_destroy(&cell->mutex);
        free(cell);
    }
}

static ZrVmTaskMutexCell *zr_vm_task_mutex_cell_new(SZrState *state,
                                                    const SZrTypeValue *value,
                                                    TZrUInt32 kind,
                                                    const TZrChar *contextMessage) {
    ZrVmTaskMutexCell *cell;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    cell = (ZrVmTaskMutexCell *)calloc(1, sizeof(*cell));
    if (cell == ZR_NULL) {
        return ZR_NULL;
    }

    zr_vm_task_sync_mutex_init(&cell->mutex);
    zr_vm_task_sync_condition_init(&cell->condition);
    cell->refCount = 1u;
    cell->readerCount = 0u;
    cell->writerActive = ZR_FALSE;
    cell->kind = (TZrUInt8)kind;
    if (!zr_vm_task_transport_encode_value(state, value, &cell->value, contextMessage)) {
        zr_vm_task_sync_condition_destroy(&cell->condition);
        zr_vm_task_sync_mutex_destroy(&cell->mutex);
        free(cell);
        return ZR_NULL;
    }

    return cell;
}

TZrBool zr_vm_task_mutex_make_value(SZrState *state,
                                    ZrVmTaskMutexCell *cell,
                                    TZrUInt32 kind,
                                    SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue cellValue;

    if (state == ZR_NULL || cell == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state,
                                         kind == ZR_VM_TASK_MUTEX_KIND_SHARED ? "SharedMutex" : "UniqueMutex");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetNativePointer(state, &cellValue, cell);
    zr_vm_task_set_value_field(state, handle, ZR_VM_TASK_MUTEX_NATIVE_CELL_FIELD, &cellValue);
    zr_vm_task_set_uint_field(state, handle, ZR_VM_TASK_MUTEX_KIND_FIELD, kind);
    return zr_vm_task_finish_object(state, result, handle);
}

TZrBool zr_vm_task_mutex_try_get_cell(SZrState *state,
                                      const SZrTypeValue *value,
                                      ZrVmTaskMutexCell **outCell,
                                      TZrUInt32 *outKind) {
    SZrObject *handle;

    if (outCell != ZR_NULL) {
        *outCell = ZR_NULL;
    }
    if (outKind != ZR_NULL) {
        *outKind = 0u;
    }
    if (state == ZR_NULL || value == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = ZR_CAST_OBJECT(state, value->value.object);
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outCell != ZR_NULL) {
        *outCell = zr_vm_task_mutex_cell_from_handle(state, handle);
    }
    if (outKind != ZR_NULL) {
        *outKind = zr_vm_task_mutex_handle_kind(state, handle);
    }
    return outCell == ZR_NULL || *outCell != ZR_NULL;
}

static TZrBool zr_vm_task_lock_make_value(SZrState *state,
                                          ZrVmTaskMutexCell *cell,
                                          TZrUInt32 lockKind,
                                          SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue cellValue;

    if (state == ZR_NULL || cell == ZR_NULL || result == ZR_NULL || !zr_vm_task_mutex_cell_add_ref(cell)) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state,
                                         lockKind == ZR_VM_TASK_LOCK_KIND_SHARED ? "SharedLock" : "Lock");
    if (handle == ZR_NULL) {
        zr_vm_task_mutex_cell_release(cell);
        return ZR_FALSE;
    }

    ZrLib_Value_SetNativePointer(state, &cellValue, cell);
    zr_vm_task_set_value_field(state, handle, ZR_VM_TASK_LOCK_MUTEX_CELL_FIELD, &cellValue);
    zr_vm_task_set_uint_field(state, handle, ZR_VM_TASK_LOCK_KIND_FIELD, lockKind);
    zr_vm_task_set_bool_field(state, handle, ZR_VM_TASK_LOCK_ACTIVE_FIELD, ZR_TRUE);
    return zr_vm_task_finish_object(state, result, handle);
}

static TZrBool zr_vm_task_lock_snapshot_value(SZrState *state,
                                              SZrObject *guard,
                                              ZrVmTaskTransportValue *snapshot) {
    ZrVmTaskMutexCell *cell;

    if (state == ZR_NULL || guard == ZR_NULL || snapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    cell = zr_vm_task_lock_cell_from_handle(state, guard);
    if (cell == ZR_NULL || !zr_vm_task_lock_is_active(state, guard)) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (!zr_vm_task_transport_clone_value(&cell->value, snapshot)) {
        zr_vm_task_sync_mutex_unlock(&cell->mutex);
        return ZR_FALSE;
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_mutex_construct_internal(ZrLibCallContext *context,
                                                   SZrTypeValue *result,
                                                   TZrUInt32 kind,
                                                   const TZrChar *typeName,
                                                   const TZrChar *contextMessage) {
    SZrObject *handle;
    SZrTypeValue *value;
    ZrVmTaskMutexCell *cell;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "zr.thread mutex primitives require supportMultithread = true")) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, typeName);
    }
    cell = zr_vm_task_mutex_cell_new(context->state, value, kind, contextMessage);
    if (handle == ZR_NULL || cell == ZR_NULL) {
        zr_vm_task_mutex_cell_release(cell);
        return ZR_FALSE;
    }

    {
        SZrTypeValue cellValue;

        ZrLib_Value_SetNativePointer(context->state, &cellValue, cell);
        zr_vm_task_set_value_field(context->state, handle, ZR_VM_TASK_MUTEX_NATIVE_CELL_FIELD, &cellValue);
        zr_vm_task_set_uint_field(context->state, handle, ZR_VM_TASK_MUTEX_KIND_FIELD, kind);
    }
    return zr_vm_task_finish_object(context->state, result, handle);
}

static TZrBool zr_vm_task_mutex_lock_internal(ZrLibCallContext *context,
                                              SZrTypeValue *result,
                                              TZrUInt32 expectedMutexKind,
                                              TZrUInt32 guardKind) {
    ZrVmTaskMutexCell *cell;
    TZrUInt32 actualKind;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_mutex_cell_from_handle(context->state, zr_vm_task_self_object(context));
    actualKind = zr_vm_task_mutex_handle_kind(context->state, zr_vm_task_self_object(context));
    if (cell == ZR_NULL || actualKind != expectedMutexKind) {
        return zr_vm_task_raise_runtime_error(context->state, "Thread mutex cell is invalid");
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (guardKind == ZR_VM_TASK_LOCK_KIND_SHARED) {
        while (cell->writerActive) {
            zr_vm_task_sync_condition_wait(&cell->condition, &cell->mutex, 10u);
        }
        cell->readerCount++;
    } else {
        while (cell->writerActive || cell->readerCount > 0u) {
            zr_vm_task_sync_condition_wait(&cell->condition, &cell->mutex, 10u);
        }
        cell->writerActive = ZR_TRUE;
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    if (!zr_vm_task_lock_make_value(context->state, cell, guardKind, result)) {
        zr_vm_task_sync_mutex_lock(&cell->mutex);
        if (guardKind == ZR_VM_TASK_LOCK_KIND_SHARED) {
            if (cell->readerCount > 0u) {
                cell->readerCount--;
            }
        } else {
            cell->writerActive = ZR_FALSE;
        }
        zr_vm_task_sync_condition_signal(&cell->condition);
        zr_vm_task_sync_mutex_unlock(&cell->mutex);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_vm_task_lock_unlock_internal(ZrLibCallContext *context,
                                               SZrTypeValue *result,
                                               TZrUInt32 expectedKind) {
    SZrObject *guard;
    ZrVmTaskMutexCell *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    guard = zr_vm_task_self_object(context);
    cell = zr_vm_task_lock_cell_from_handle(context->state, guard);
    if (cell == ZR_NULL || !zr_vm_task_lock_is_active(context->state, guard) ||
        zr_vm_task_lock_kind_from_handle(context->state, guard) != expectedKind) {
        return zr_vm_task_raise_runtime_error(context->state, "Lock guard is not active");
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (expectedKind == ZR_VM_TASK_LOCK_KIND_SHARED) {
        if (cell->readerCount == 0u) {
            zr_vm_task_sync_mutex_unlock(&cell->mutex);
            return zr_vm_task_raise_runtime_error(context->state, "Shared lock state is inconsistent");
        }
        cell->readerCount--;
    } else {
        if (!cell->writerActive) {
            zr_vm_task_sync_mutex_unlock(&cell->mutex);
            return zr_vm_task_raise_runtime_error(context->state, "Exclusive lock state is inconsistent");
        }
        cell->writerActive = ZR_FALSE;
    }
    zr_vm_task_sync_condition_signal(&cell->condition);
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    zr_vm_task_set_bool_field(context->state, guard, ZR_VM_TASK_LOCK_ACTIVE_FIELD, ZR_FALSE);
    zr_vm_task_set_null_field(context->state, guard, ZR_VM_TASK_LOCK_MUTEX_CELL_FIELD);
    zr_vm_task_mutex_cell_release(cell);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue *value;
    ZrVmTaskSharedCell *cell;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "Shared requires supportMultithread = true")) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "Shared");
    }
    cell = zr_vm_task_shared_cell_new(context->state, value);
    if (handle == ZR_NULL || cell == ZR_NULL) {
        zr_vm_task_shared_cell_release_strong(cell);
        return ZR_FALSE;
    }

    {
        SZrTypeValue cellValue;

        ZrLib_Value_SetNativePointer(context->state, &cellValue, cell);
        zr_vm_task_set_value_field(context->state, handle, ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD, &cellValue);
        zr_vm_task_set_bool_field(context->state, handle, ZR_VM_TASK_SHARED_IS_WEAK_FIELD, ZR_FALSE);
    }
    if (!zr_vm_task_finish_object(context->state, result, handle)) {
        zr_vm_task_shared_cell_release_strong(cell);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_load(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskSharedCell *cell;
    ZrVmTaskTransportValue snapshot;
    TZrBool alive;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context));
    if (cell == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    zr_vm_task_sync_mutex_lock(&cell->mutex);
    alive = zr_vm_task_shared_cell_is_alive_locked(cell);
    if (alive) {
        alive = zr_vm_task_transport_clone_value(&cell->value, &snapshot);
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    if (!alive) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    if (!zr_vm_task_transport_decode_value(context->state, &snapshot, result)) {
        zr_vm_task_transport_clear(&snapshot);
        return ZR_FALSE;
    }
    zr_vm_task_transport_clear(&snapshot);
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_store(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskSharedCell *cell;
    SZrTypeValue *value;
    ZrVmTaskTransportValue encodedValue;
    ZrVmTaskTransportValue previousValue;
    TZrBool alive;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context));
    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    memset(&encodedValue, 0, sizeof(encodedValue));
    memset(&previousValue, 0, sizeof(previousValue));
    if (!zr_vm_task_transport_encode_value(context->state,
                                           value,
                                           &encodedValue,
                                           "Shared only stores Send + Sync values and thread transport handles")) {
        return ZR_FALSE;
    }

    alive = ZR_FALSE;
    if (cell != ZR_NULL) {
        zr_vm_task_sync_mutex_lock(&cell->mutex);
        if (zr_vm_task_shared_cell_is_alive_locked(cell)) {
            previousValue = cell->value;
            cell->value = encodedValue;
            memset(&encodedValue, 0, sizeof(encodedValue));
            alive = ZR_TRUE;
        }
        zr_vm_task_sync_mutex_unlock(&cell->mutex);
    }
    if (!alive) {
        zr_vm_task_transport_clear(&encodedValue);
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    zr_vm_task_transport_clear(&previousValue);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_clone(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskSharedCell *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_add_strong_ref(cell)) {
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    return zr_vm_task_shared_make_value(context->state, cell, ZR_FALSE, result);
}

TZrBool zr_vm_task_shared_downgrade(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskSharedCell *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_add_weak_ref_if_alive(cell)) {
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    return zr_vm_task_shared_make_value(context->state, cell, ZR_TRUE, result);
}

TZrBool zr_vm_task_shared_release(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    ZrVmTaskSharedCell *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_vm_task_self_object(context);
    cell = zr_vm_task_shared_cell_from_handle(context->state, self);
    if (cell != ZR_NULL) {
        zr_vm_task_shared_cell_release_strong(cell);
        zr_vm_task_set_null_field(context->state, self, ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD);
    }

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_shared_cell_is_alive_native(
                                zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context))));
    return ZR_TRUE;
}

TZrBool zr_vm_task_weak_shared_upgrade(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskSharedCell *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_add_strong_ref(cell)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    return zr_vm_task_shared_make_value(context->state, cell, ZR_FALSE, result);
}

TZrBool zr_vm_task_weak_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_shared_cell_is_alive_native(
                                zr_vm_task_shared_cell_from_handle(context->state, zr_vm_task_self_object(context))));
    return ZR_TRUE;
}

TZrBool zr_vm_task_transfer_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue *value;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "Transfer requires supportMultithread = true")) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "Transfer");
    }
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_set_value_field(context->state, handle, kTaskTransferValueField, value);
    zr_vm_task_set_bool_field(context->state, handle, kTaskTransferTakenField, ZR_FALSE);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_transfer_take(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_vm_task_self_object(context);
    if (zr_vm_task_get_bool_field(context->state, self, kTaskTransferTakenField, ZR_FALSE)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    zr_vm_task_copy_value_or_null(context->state,
                                  zr_vm_task_get_field_value(context->state, self, kTaskTransferValueField),
                                  result);
    zr_vm_task_set_bool_field(context->state, self, kTaskTransferTakenField, ZR_TRUE);
    zr_vm_task_set_null_field(context->state, self, kTaskTransferValueField);
    return ZR_TRUE;
}

TZrBool zr_vm_task_transfer_is_taken(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_get_bool_field(context->state,
                                                  zr_vm_task_self_object(context),
                                                  kTaskTransferTakenField,
                                                  ZR_FALSE));
    return ZR_TRUE;
}

TZrBool zr_vm_task_unique_mutex_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_mutex_construct_internal(context,
                                               result,
                                               ZR_VM_TASK_MUTEX_KIND_UNIQUE,
                                               "UniqueMutex",
                                               "UniqueMutex only stores Send values and thread transport handles");
}

TZrBool zr_vm_task_unique_mutex_lock(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_mutex_lock_internal(context,
                                          result,
                                          ZR_VM_TASK_MUTEX_KIND_UNIQUE,
                                          ZR_VM_TASK_LOCK_KIND_EXCLUSIVE);
}

TZrBool zr_vm_task_shared_mutex_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_mutex_construct_internal(context,
                                               result,
                                               ZR_VM_TASK_MUTEX_KIND_SHARED,
                                               "SharedMutex",
                                               "SharedMutex only stores Send + Sync values and thread transport handles");
}

TZrBool zr_vm_task_shared_mutex_read(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_mutex_lock_internal(context,
                                          result,
                                          ZR_VM_TASK_MUTEX_KIND_SHARED,
                                          ZR_VM_TASK_LOCK_KIND_SHARED);
}

TZrBool zr_vm_task_shared_mutex_write(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_mutex_lock_internal(context,
                                          result,
                                          ZR_VM_TASK_MUTEX_KIND_SHARED,
                                          ZR_VM_TASK_LOCK_KIND_EXCLUSIVE);
}

TZrBool zr_vm_task_lock_load(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrVmTaskTransportValue snapshot;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_vm_task_lock_snapshot_value(context->state, zr_vm_task_self_object(context), &snapshot)) {
        return zr_vm_task_raise_runtime_error(context->state, "Lock guard is not active");
    }

    if (!zr_vm_task_transport_decode_value(context->state, &snapshot, result)) {
        zr_vm_task_transport_clear(&snapshot);
        return ZR_FALSE;
    }
    zr_vm_task_transport_clear(&snapshot);
    return ZR_TRUE;
}

TZrBool zr_vm_task_lock_store(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *guard;
    ZrVmTaskMutexCell *cell;
    SZrTypeValue *value;
    ZrVmTaskTransportValue encodedValue;
    ZrVmTaskTransportValue previousValue;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    guard = zr_vm_task_self_object(context);
    if (zr_vm_task_lock_kind_from_handle(context->state, guard) != ZR_VM_TASK_LOCK_KIND_EXCLUSIVE) {
        return zr_vm_task_raise_runtime_error(context->state, "SharedLock does not allow mutation");
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    memset(&encodedValue, 0, sizeof(encodedValue));
    memset(&previousValue, 0, sizeof(previousValue));
    if (!zr_vm_task_transport_encode_value(context->state,
                                           value,
                                           &encodedValue,
                                           "Lock only stores Send values and thread transport handles")) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_lock_cell_from_handle(context->state, guard);
    if (cell == ZR_NULL || !zr_vm_task_lock_is_active(context->state, guard)) {
        zr_vm_task_transport_clear(&encodedValue);
        return zr_vm_task_raise_runtime_error(context->state, "Lock guard is not active");
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (!cell->writerActive) {
        zr_vm_task_sync_mutex_unlock(&cell->mutex);
        zr_vm_task_transport_clear(&encodedValue);
        return zr_vm_task_raise_runtime_error(context->state, "Exclusive lock state is inconsistent");
    }
    previousValue = cell->value;
    cell->value = encodedValue;
    memset(&encodedValue, 0, sizeof(encodedValue));
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    zr_vm_task_transport_clear(&previousValue);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_lock_unlock(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_lock_unlock_internal(context, result, ZR_VM_TASK_LOCK_KIND_EXCLUSIVE);
}

TZrBool zr_vm_task_shared_lock_load(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_lock_load(context, result);
}

TZrBool zr_vm_task_shared_lock_unlock(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_lock_unlock_internal(context, result, ZR_VM_TASK_LOCK_KIND_SHARED);
}
