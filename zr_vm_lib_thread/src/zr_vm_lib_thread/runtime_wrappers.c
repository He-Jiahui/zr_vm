#include "runtime_internal.h"

static const TZrChar *kTaskTransferValueField = "__zr_task_transfer_value";
static const TZrChar *kTaskTransferTakenField = "__zr_task_transfer_taken";
static const TZrChar *kTaskMutexCellField = "__zr_task_mutex_cell";
static const TZrChar *kTaskMutexValueField = "__zr_task_mutex_value";
static const TZrChar *kTaskMutexLockedField = "__zr_task_mutex_locked";
static const TZrChar *kTaskAtomicValueField = "__zr_task_atomic_value";

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

TZrBool zr_vm_task_shared_cell_is_alive_native(ZrVmTaskSharedCell *cell) {
    TZrBool alive = ZR_FALSE;

    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    alive = (TZrBool)(cell->alive && cell->strongCount > 0u);
    zr_vm_task_sync_mutex_unlock(&cell->mutex);
    return alive;
}

TZrBool zr_vm_task_shared_cell_add_strong_ref(ZrVmTaskSharedCell *cell) {
    TZrBool retained = ZR_FALSE;

    if (cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&cell->mutex);
    if (cell->alive && cell->strongCount > 0u) {
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
            cell->alive = ZR_FALSE;
            releasedValue = cell->value;
            memset(&cell->value, 0, sizeof(cell->value));
            freeCell = cell->weakCount == 0u;
        }
    }
    zr_vm_task_sync_mutex_unlock(&cell->mutex);

    zr_vm_task_transport_clear(&releasedValue);
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
    cell->alive = ZR_TRUE;
    if (!zr_vm_task_transport_encode_value(state,
                                           value,
                                           &cell->value,
                                           "Shared only stores sendable values and thread transport handles")) {
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
    alive = (TZrBool)(cell->alive && cell->strongCount > 0u);
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
                                           "Shared only stores sendable values and thread transport handles")) {
        return ZR_FALSE;
    }

    alive = ZR_FALSE;
    if (cell != ZR_NULL) {
        zr_vm_task_sync_mutex_lock(&cell->mutex);
        if (cell->alive && cell->strongCount > 0u) {
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
    if (!zr_vm_task_shared_cell_is_alive_native(cell) || !zr_vm_task_shared_cell_add_weak_ref(cell)) {
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

TZrBool zr_vm_task_mutex_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    SZrObject *cell;
    SZrTypeValue *value;
    SZrTypeValue cellValue;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "Mutex requires supportMultithread = true")) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "Mutex");
    }
    cell = ZrLib_Object_New(context->state);
    if (handle == ZR_NULL || cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_set_value_field(context->state, cell, kTaskMutexValueField, value);
    zr_vm_task_set_bool_field(context->state, cell, kTaskMutexLockedField, ZR_FALSE);
    ZrLib_Value_SetObject(context->state, &cellValue, cell, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(context->state, handle, kTaskMutexCellField, &cellValue);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_mutex_load(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_get_object_field(context->state, zr_vm_task_self_object(context), kTaskMutexCellField);
    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state, cell, kTaskMutexValueField),
                                         result);
}

TZrBool zr_vm_task_mutex_lock(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_get_object_field(context->state, zr_vm_task_self_object(context), kTaskMutexCellField);
    if (cell == ZR_NULL) {
        return zr_vm_task_raise_runtime_error(context->state, "Mutex cell is missing");
    }
    if (zr_vm_task_get_bool_field(context->state, cell, kTaskMutexLockedField, ZR_FALSE)) {
        return zr_vm_task_raise_runtime_error(context->state, "Mutex is already locked");
    }

    zr_vm_task_set_bool_field(context->state, cell, kTaskMutexLockedField, ZR_TRUE);
    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state, cell, kTaskMutexValueField),
                                         result);
}

TZrBool zr_vm_task_mutex_unlock(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;
    SZrTypeValue *value;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_get_object_field(context->state, zr_vm_task_self_object(context), kTaskMutexCellField);
    if (cell == ZR_NULL) {
        return zr_vm_task_raise_runtime_error(context->state, "Mutex cell is missing");
    }
    if (!zr_vm_task_get_bool_field(context->state, cell, kTaskMutexLockedField, ZR_FALSE)) {
        return zr_vm_task_raise_runtime_error(context->state, "Mutex is not locked");
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    zr_vm_task_set_value_field(context->state, cell, kTaskMutexValueField, value);
    zr_vm_task_set_bool_field(context->state, cell, kTaskMutexLockedField, ZR_FALSE);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_mutex_is_locked(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell = zr_vm_task_get_object_field(context->state, zr_vm_task_self_object(context), kTaskMutexCellField);

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        cell != ZR_NULL && zr_vm_task_get_bool_field(context->state, cell, kTaskMutexLockedField, ZR_FALSE));
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_bool_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    TZrBool value;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "AtomicBool requires supportMultithread = true") ||
        !ZrLib_CallContext_ReadBool(context, 0, &value)) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "AtomicBool");
    }
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, &fieldValue, value);
    zr_vm_task_set_value_field(context->state, handle, kTaskAtomicValueField, &fieldValue);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_atomic_bool_load(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state,
                                                                    zr_vm_task_self_object(context),
                                                                    kTaskAtomicValueField),
                                         result);
}

TZrBool zr_vm_task_atomic_bool_store(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrBool value;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadBool(context, 0, &value)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, &fieldValue, value);
    zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, &fieldValue);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_bool_compare_exchange(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrBool expected;
    TZrBool desired;
    const SZrTypeValue *current;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadBool(context, 0, &expected) ||
        !ZrLib_CallContext_ReadBool(context, 1, &desired)) {
        return ZR_FALSE;
    }

    current = zr_vm_task_get_field_value(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField);
    if (current != ZR_NULL && current->type == ZR_VALUE_TYPE_BOOL &&
        current->value.nativeObject.nativeBool == expected) {
        ZrLib_Value_SetBool(context->state, &fieldValue, desired);
        zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, &fieldValue);
        ZrLib_Value_SetBool(context->state, result, ZR_TRUE);
        return ZR_TRUE;
    }

    ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_int_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    TZrInt64 value;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "AtomicInt requires supportMultithread = true") ||
        !zr_vm_task_read_strict_int(context, 0, &value)) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "AtomicInt");
    }
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, &fieldValue, value);
    zr_vm_task_set_value_field(context->state, handle, kTaskAtomicValueField, &fieldValue);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_atomic_int_load(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state,
                                                                    zr_vm_task_self_object(context),
                                                                    kTaskAtomicValueField),
                                         result);
}

TZrBool zr_vm_task_atomic_int_store(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 value;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_int(context, 0, &value)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state, &fieldValue, value);
    zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, &fieldValue);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_int_compare_exchange(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *expected;
    SZrTypeValue *desired;
    const SZrTypeValue *current;
    TZrBool matched;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    expected = ZrLib_CallContext_Argument(context, 0);
    desired = ZrLib_CallContext_Argument(context, 1);
    if (expected == ZR_NULL || desired == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 2, 2);
    }
    if (!zr_vm_task_is_integer_value(expected) || !zr_vm_task_is_integer_value(desired)) {
        ZrLib_CallContext_RaiseTypeError(context, 0, "int");
    }

    current = zr_vm_task_get_field_value(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField);
    matched = zr_vm_task_value_equals(current, expected);
    if (matched) {
        zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, desired);
    }

    ZrLib_Value_SetBool(context->state, result, matched);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_int_fetch_add(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 delta;
    TZrInt64 current;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_int(context, 0, &delta)) {
        return ZR_FALSE;
    }

    current = zr_vm_task_get_int_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, 0);
    ZrLib_Value_SetInt(context->state, result, current);
    ZrLib_Value_SetInt(context->state, &fieldValue, current + delta);
    zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, &fieldValue);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_int_fetch_sub(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 delta;
    TZrInt64 current;
    SZrTypeValue fieldValue;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_int(context, 0, &delta)) {
        return ZR_FALSE;
    }

    current = zr_vm_task_get_int_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, 0);
    ZrLib_Value_SetInt(context->state, result, current);
    ZrLib_Value_SetInt(context->state, &fieldValue, current - delta);
    zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, &fieldValue);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_uint_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    TZrUInt64 value;

    if (context == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "AtomicUInt requires supportMultithread = true") ||
        !zr_vm_task_read_strict_uint(context, 0, &value)) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_resolve_construct_target(context);
    if (handle == ZR_NULL) {
        handle = zr_vm_task_new_typed_object(context->state, "AtomicUInt");
    }
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_set_uint_field(context->state, handle, kTaskAtomicValueField, value);
    return zr_vm_task_finish_object(context->state, result, handle);
}

TZrBool zr_vm_task_atomic_uint_load(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state,
                                                                    zr_vm_task_self_object(context),
                                                                    kTaskAtomicValueField),
                                         result);
}

TZrBool zr_vm_task_atomic_uint_store(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 value;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_uint(context, 0, &value)) {
        return ZR_FALSE;
    }

    zr_vm_task_set_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, value);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_uint_compare_exchange(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *expected;
    SZrTypeValue *desired;
    const SZrTypeValue *current;
    TZrBool matched;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    expected = ZrLib_CallContext_Argument(context, 0);
    desired = ZrLib_CallContext_Argument(context, 1);
    if (expected == ZR_NULL || desired == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 2, 2);
    }
    if (!zr_vm_task_is_integer_value(expected) || !zr_vm_task_is_integer_value(desired)) {
        ZrLib_CallContext_RaiseTypeError(context, 0, "uint");
    }

    current = zr_vm_task_get_field_value(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField);
    matched = zr_vm_task_value_equals(current, expected);
    if (matched) {
        zr_vm_task_set_value_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, desired);
    }

    ZrLib_Value_SetBool(context->state, result, matched);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_uint_fetch_add(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 delta;
    TZrUInt64 current;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_uint(context, 0, &delta)) {
        return ZR_FALSE;
    }

    current = zr_vm_task_get_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, 0);
    ZrCore_Value_InitAsUInt(context->state, result, current);
    zr_vm_task_set_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, current + delta);
    return ZR_TRUE;
}

TZrBool zr_vm_task_atomic_uint_fetch_sub(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 delta;
    TZrUInt64 current;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_uint(context, 0, &delta)) {
        return ZR_FALSE;
    }

    current = zr_vm_task_get_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, 0);
    ZrCore_Value_InitAsUInt(context->state, result, current);
    zr_vm_task_set_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, current - delta);
    return ZR_TRUE;
}
