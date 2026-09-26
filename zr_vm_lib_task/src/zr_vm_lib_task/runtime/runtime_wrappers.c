#include "runtime/runtime_internal.h"

/* wrappers 将共享 cell、transfer、mutex 和“atomic”值存入 VM 私有字段；这些并非 C11 原子对象。 */
static const TZrChar *kTaskSharedCellField = "__zr_task_shared_cell";
static const TZrChar *kTaskSharedValueField = "__zr_task_shared_value";
static const TZrChar *kTaskSharedStrongCountField = "__zr_task_shared_strong_count";
static const TZrChar *kTaskSharedAliveField = "__zr_task_shared_alive";
static const TZrChar *kTaskTransferValueField = "__zr_task_transfer_value";
static const TZrChar *kTaskTransferTakenField = "__zr_task_transfer_taken";
static const TZrChar *kTaskMutexCellField = "__zr_task_mutex_cell";
static const TZrChar *kTaskMutexValueField = "__zr_task_mutex_value";
static const TZrChar *kTaskMutexLockedField = "__zr_task_mutex_locked";
static const TZrChar *kTaskAtomicValueField = "__zr_task_atomic_value";

/* 统一解析 wrapper 到 cell，调用方按 NULL 处理已释放或错误 receiver。 */
static SZrObject *zr_vm_task_shared_cell(SZrState *state, SZrObject *handle) {
    return zr_vm_task_get_object_field(state, handle, kTaskSharedCellField);
}

/* Shared 的逻辑存活同时要求 alive 标志和正 strongCount。 */
static TZrBool zr_vm_task_shared_cell_is_alive(SZrState *state, SZrObject *cell) {
    return cell != ZR_NULL && zr_vm_task_get_bool_field(state, cell, kTaskSharedAliveField, ZR_FALSE) &&
           zr_vm_task_get_int_field(state, cell, kTaskSharedStrongCountField, 0) > 0;
}

/* clone/upgrade 在构造新 strong handle 前增加引用计数；失败时调用方应回滚。 */
static TZrBool zr_vm_task_shared_cell_add_ref(SZrState *state, SZrObject *cell) {
    TZrInt64 strongCount;

    if (state == ZR_NULL || cell == ZR_NULL || !zr_vm_task_shared_cell_is_alive(state, cell)) {
        return ZR_FALSE;
    }

    strongCount = zr_vm_task_get_int_field(state, cell, kTaskSharedStrongCountField, 0);
    zr_vm_task_set_int_field(state, cell, kTaskSharedStrongCountField, strongCount + 1);
    return ZR_TRUE;
}

/* 最后一强引用清空 payload 并标记 cell 死亡；释放依赖显式 release。 */
static void zr_vm_task_shared_cell_release(SZrState *state, SZrObject *cell) {
    TZrInt64 strongCount;

    if (state == ZR_NULL || cell == ZR_NULL) {
        return;
    }

    strongCount = zr_vm_task_get_int_field(state, cell, kTaskSharedStrongCountField, 0);
    if (strongCount <= 1) {
        zr_vm_task_set_int_field(state, cell, kTaskSharedStrongCountField, 0);
        zr_vm_task_set_bool_field(state, cell, kTaskSharedAliveField, ZR_FALSE);
        zr_vm_task_set_null_field(state, cell, kTaskSharedValueField);
        return;
    }

    zr_vm_task_set_int_field(state, cell, kTaskSharedStrongCountField, strongCount - 1);
}

/* 将已加引用的 cell 包装成新的 Shared VM 对象。 */
static TZrBool zr_vm_task_shared_handle_from_cell(SZrState *state, SZrObject *cell, SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue cellValue;

    if (state == ZR_NULL || cell == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state, "Shared");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &cellValue, cell, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(state, handle, kTaskSharedCellField, &cellValue);
    return zr_vm_task_finish_object(state, result, handle);
}

/* 弱句柄只保留 cell 引用，不改变 strongCount；升级时再尝试加引用。 */
static TZrBool zr_vm_task_weak_handle_from_cell(SZrState *state, SZrObject *cell, SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue cellValue;

    if (state == ZR_NULL || cell == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = zr_vm_task_new_typed_object(state, "WeakShared");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &cellValue, cell, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(state, handle, kTaskSharedCellField, &cellValue);
    return zr_vm_task_finish_object(state, result, handle);
}

/* Shared 构造要求项目允许多线程，并建立一个带初始 strong 引用的逻辑 cell。 */
TZrBool zr_vm_task_shared_construct(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    SZrObject *cell;
    SZrTypeValue *value;
    SZrTypeValue cellValue;

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
    cell = ZrLib_Object_New(context->state);
    if (handle == ZR_NULL || cell == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_set_value_field(context->state, cell, kTaskSharedValueField, value);
    zr_vm_task_set_int_field(context->state, cell, kTaskSharedStrongCountField, 1);
    zr_vm_task_set_bool_field(context->state, cell, kTaskSharedAliveField, ZR_TRUE);
    ZrLib_Value_SetObject(context->state, &cellValue, cell, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(context->state, handle, kTaskSharedCellField, &cellValue);
    return zr_vm_task_finish_object(context->state, result, handle);
}

/* 读取活 cell 的值；死亡 cell 对脚本表现为 null。 */
TZrBool zr_vm_task_shared_load(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_is_alive(context->state, cell)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    return zr_vm_task_copy_value_or_null(context->state,
                                         zr_vm_task_get_field_value(context->state, cell, kTaskSharedValueField),
                                         result);
}

/* 覆盖活 cell payload；死亡句柄通过 runtime error 拒绝写入。 */
TZrBool zr_vm_task_shared_store(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;
    SZrTypeValue *value;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_is_alive(context->state, cell)) {
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, 1, 1);
    }

    zr_vm_task_set_value_field(context->state, cell, kTaskSharedValueField, value);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 复制 strong handle；先增计数，再分配 wrapper，故分配失败存在计数回滚责任。 */
/* BUG: add_ref 成功后若新句柄创建失败或抛异常，clone 未回滚 strongCount；cell 会错误地保持逻辑存活。 */
TZrBool zr_vm_task_shared_clone(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_add_ref(context->state, cell)) {
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    return zr_vm_task_shared_handle_from_cell(context->state, cell, result);
}

/* 将活 strong handle 降为不持有强引用的 WeakShared。 */
TZrBool zr_vm_task_shared_downgrade(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_is_alive(context->state, cell)) {
        return zr_vm_task_raise_runtime_error(context->state, "Shared handle is no longer alive");
    }

    return zr_vm_task_weak_handle_from_cell(context->state, cell, result);
}

/* 显式幂等释放当前 handle 的 strong 引用，并清掉其 cell 槽位。 */
/* TODO: 强句柄仅靠 release 调整逻辑计数；若强句柄被 GC 丢弃而弱句柄仍持 cell，应核对升级语义是否仍允许成功。 */
TZrBool zr_vm_task_shared_release(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_vm_task_self_object(context);
    cell = zr_vm_task_shared_cell(context->state, self);
    if (cell != ZR_NULL) {
        zr_vm_task_shared_cell_release(context->state, cell);
        zr_vm_task_set_null_field(context->state, self, kTaskSharedCellField);
    }

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* 查询当前 strong handle 是否仍关联活 cell。 */
TZrBool zr_vm_task_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_shared_cell_is_alive(context->state,
                                                        zr_vm_task_shared_cell(context->state,
                                                                               zr_vm_task_self_object(context))));
    return ZR_TRUE;
}

/* 尝试将弱句柄升级为 strong；cell 已死亡时返回 null 而非抛异常。 */
/* BUG: add_ref 成功后若新强句柄创建失败或抛异常，upgrade 未回滚 strongCount；弱句柄后续可观察到错误的存活状态。 */
TZrBool zr_vm_task_weak_shared_upgrade(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *cell;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    cell = zr_vm_task_shared_cell(context->state, zr_vm_task_self_object(context));
    if (!zr_vm_task_shared_cell_add_ref(context->state, cell)) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    return zr_vm_task_shared_handle_from_cell(context->state, cell, result);
}

/* 弱句柄只观察存活状态，不延长 cell 生命周期。 */
TZrBool zr_vm_task_weak_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_shared_cell_is_alive(context->state,
                                                        zr_vm_task_shared_cell(context->state,
                                                                               zr_vm_task_self_object(context))));
    return ZR_TRUE;
}

/* Transfer 保存一次性 payload，构造同样要求项目开启多线程能力。 */
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

/* 首次 take 复制 payload 后清槽并标记 taken；重复 take 稳定返回 null。 */
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

/* 查询 Transfer 是否已经消费。 */
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

/* Mutex 是 VM 字段上的协作锁状态，需多线程项目权限但不创建 OS mutex。 */
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

/* 读取 Mutex 当前值；设计上不要求先持锁。 */
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

/* 非阻塞地取得逻辑锁，已锁时报告错误并不排队。 */
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

/* 用新值覆盖 cell 后清除逻辑锁；实现没有 owner token，任意调用方可解锁。 */
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

/* 查询逻辑锁标志；内部 helper 调用必须先检查 context。 */
/* TODO: 当前实现先解引用 context 再做 NULL 检查，若 native binding 直接传空 context 会崩；确认是否应统一加前置保护。 */
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

/* AtomicBool 只在 VM 回调线程内操作 bool 字段；名称不代表 C11 原子同步。 */
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

/* 读取 AtomicBool 当前字段值。 */
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

/* 严格 bool 写入 AtomicBool 字段。 */
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

/* 在当前 bool 等于 expected 时写入 desired，并返回是否交换。 */
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

/* AtomicInt 构造使用严格整数域初始化 VM 字段。 */
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

/* 读取 AtomicInt 字段，不提供独立线程原子性。 */
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

/* 严格整数写入 AtomicInt 字段。 */
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

/* 比较整数值并原样保存 desired；signed/unsigned 混用由 value_equals 统一数值比较。 */
/* BUG: 构造/store 将输入收敛到 signed int64，但 CAS 可原样写入 UINT64_MAX；load 返回 UInt，后续 get_int_field 的越界转换语义不稳定。 */
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

/* 返回旧 signed 值并写入 current + delta；调用方需避免超出 int64 范围。 */
/* BUG: 构造/store 可写入 INT64_MAX，delta=1 时此处有符号溢出属于未定义行为；应核对范围策略。 */
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

/* 返回旧 signed 值并写入 current - delta；调用方需避免超出 int64 范围。 */
/* BUG: 构造/store 可写入 INT64_MIN，delta=1 时此处有符号溢出属于未定义行为；应核对范围策略。 */
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

/* AtomicUInt 构造接受 uint 或非负 signed 值，随后按 unsigned 字段保存。 */
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

/* 读取 AtomicUInt 字段。 */
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

/* 严格非负整数写入 AtomicUInt 字段。 */
TZrBool zr_vm_task_atomic_uint_store(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt64 value;

    if (context == ZR_NULL || result == ZR_NULL || !zr_vm_task_read_strict_uint(context, 0, &value)) {
        return ZR_FALSE;
    }

    zr_vm_task_set_uint_field(context->state, zr_vm_task_self_object(context), kTaskAtomicValueField, value);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/* AtomicUInt 的 CAS 应保持 unsigned 域，但当前仅检查“任意整数”。 */
/* BUG: 负 signed desired 可被原样写入 AtomicUInt；随后 get_uint_field 对负值回退为 0，破坏构造/store 保证的非负类型域。 */
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

/* 返回旧 uint 值并按 C unsigned 规则回绕加法。 */
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

/* 返回旧 uint 值并按 C unsigned 规则回绕减法。 */
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
