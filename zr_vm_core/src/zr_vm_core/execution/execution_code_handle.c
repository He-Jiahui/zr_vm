#include "execution_backend_internal.h"

#include <stdint.h>

/*
 * Code records are deliberately kept in a separate translation unit from
 * queue/state-machine code.  A record is reclaimable only after both the
 * execution lease and the dependency/map lease have reached zero.  Backend
 * callbacks are copied while the service lock is held, then invoked after the
 * lock is released; a backend is therefore free to query the service from a
 * callback without recursively taking the lock.
 */

/* 在持锁路径按 service、slot、code 与完整 generation 身份定位 lease 的当前记录。
 * 调用方已持有 service lock；只拒绝 FREE，因此旧 lease 可继续查询 RETIRED 记录，身份校验不等于句柄副本各自拥有 lease。 */
static SZrExecutionCodeRecord *zr_execution_backend_find_code_locked_local(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        TZrUInt32 *slotIndex) {
    SZrExecutionCodeRecord *record;
    if (service == ZR_NULL || handle == ZR_NULL ||
        handle->magic != ZR_EXECUTION_BACKEND_MAGIC ||
        handle->serviceIdentity != service->serviceIdentity ||
        handle->slotIndex >= service->codeCapacity ||
        handle->codeIdentity == 0u || !handle->leased) {
        return ZR_NULL;
    }
    record = &service->codes[handle->slotIndex];
    if (record->state == ZR_EXECUTION_BACKEND_CODE_FREE ||
        record->info.codeIdentity != handle->codeIdentity ||
        !zr_execution_backend_generation_equal(&record->info.generationKey,
                                               &handle->generationKey)) {
        return ZR_NULL;
    }
    if (slotIndex != ZR_NULL) *slotIndex = handle->slotIndex;
    return record;
}

static TZrUInt64 zr_execution_backend_handle_ticket_id(
        const SZrExecutionCodeHandle *handle) {
    return handle != ZR_NULL ? handle->codeIdentity : 0u;
}

/* 把 backend 返回结果归入此 code 的诊断，再向查询或回收调用方返回状态。
 * 只覆盖 status/codeIdentity；其他诊断由调用路径保留，不能据此恢复失败操作。 */
static EZrExecutionBackendStatus zr_execution_backend_finish_callback(
        EZrExecutionBackendStatus callbackStatus,
        SZrExecutionBackendDiagnostic *diagnostic,
        TZrUInt64 codeIdentity) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = callbackStatus;
        diagnostic->codeIdentity = codeIdentity;
    }
    return callbackStatus;
}

/* 清理 ProcessNext/Complete 拒收的 backend 产物，避免未入 code table 的资源无人交还。
 * descriptor/code 为调用方快照；userData 需覆盖完整回调序列，先撤图再尝试 retire，任一失败统一 RETIRE_FAILED；不创建可重试 code record。TODO: Complete 将作业置终态并解锁后到本函数登记 callback 之间的 userData 窗口，需沿 Unregister/FinalizeShutdown 的实际并发 owner 契约核查。 */
EZrExecutionBackendStatus zr_execution_backend_dispose_unpublished(
        SZrExecutionBackendService *service,
        const SZrExecutionBackendDescriptor *descriptor,
        const SZrExecutionBackendCodeInfo *code,
        SZrExecutionBackendDiagnostic *diagnostic) {
    EZrExecutionBackendStatus unregisterStatus;
    EZrExecutionBackendStatus retireStatus;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    TZrBool callbackPinned = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_execution_backend_service_shape_valid(service) ||
        descriptor == ZR_NULL || code == ZR_NULL || code->codeIdentity == 0u) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, code != ZR_NULL ? code->codeIdentity : 0u, 0u, 0u);
    }

    /* 未发布结果不在代码表中，作业可能已终态；用一个 callback 计数覆盖两次清理，阻止并发销毁 userData。 */
    /* Keep the descriptor's userData alive across the complete unregister /
     * retire sequence.  Complete() may have already moved its job to a
     * terminal state, so the callback counter is the remaining destruction
     * barrier against a concurrent Unregister/FinalizeShutdown. */
    zr_execution_backend_lock(service);
    if (!zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight, 0u,
                code->codeIdentity, 0u, 0u);
    }
    callbackPinned = ZR_TRUE;
    zr_execution_backend_unlock(service);

    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    if (descriptor->vtable.unregisterMaps != ZR_NULL) {
        unregisterStatus = descriptor->vtable.unregisterMaps(
                code, descriptor->userData, &callbackDiagnostic);
    } else {
        unregisterStatus = ZR_EXECUTION_BACKEND_STATUS_OK;
    }

    /* Even if map removal reports an error, ask the backend to retire the
     * code.  This gives backends one deterministic cleanup opportunity and
     * avoids leaking an unpublished result supplied by a worker. */
    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    if (descriptor->vtable.retire != ZR_NULL) {
        retireStatus = descriptor->vtable.retire(
                code, descriptor->userData, &callbackDiagnostic);
    } else {
        retireStatus = ZR_EXECUTION_BACKEND_STATUS_OK;
    }
    if (callbackPinned) zr_execution_backend_callback_leave(service);
    if (unregisterStatus != ZR_EXECUTION_BACKEND_STATUS_OK) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = unregisterStatus;
            diagnostic->actual = retireStatus;
        }
        return zr_execution_backend_finish_callback(
                ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED, diagnostic,
                code->codeIdentity);
    }
    if (retireStatus != ZR_EXECUTION_BACKEND_STATUS_OK) {
        return zr_execution_backend_finish_callback(
                ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED, diagnostic,
                code->codeIdentity);
    }
    return zr_execution_backend_finish_callback(
            ZR_EXECUTION_BACKEND_STATUS_OK, diagnostic, code->codeIdentity);
}

/* 为 lease 修改、视图、入口和 map 查询共用句柄校验，区分坏 magic、未 leased 与身份失效。
 * 已持 service lock；输出 record 只可在锁内使用，成功不增加引用计数，不核对 leaseCount 非零。 */
static EZrExecutionBackendStatus zr_execution_backend_validate_handle_locked(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        SZrExecutionCodeRecord **record,
        SZrExecutionBackendDiagnostic *diagnostic) {
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, zr_execution_backend_handle_ticket_id(handle), 0u, 0u);
    }
    if (handle->magic != ZR_EXECUTION_BACKEND_MAGIC) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, handle->magic, 0u,
                zr_execution_backend_handle_ticket_id(handle), 0u, 0u);
    }
    if (!handle->leased) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
                1u, 0u, 0u, zr_execution_backend_handle_ticket_id(handle), 0u, 0u);
    }
    *record = zr_execution_backend_find_code_locked_local(service, handle, ZR_NULL);
    if (*record == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND, diagnostic,
                1u, 0u, 0u, zr_execution_backend_handle_ticket_id(handle), 0u, 0u);
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

static void zr_execution_backend_clear_handle(SZrExecutionCodeHandle *handle) {
    if (handle != ZR_NULL) memset(handle, 0, sizeof(*handle));
}

static void zr_execution_backend_reset_code_record_locked(
        SZrExecutionCodeRecord *record) {
    if (record != ZR_NULL) memset(record, 0, sizeof(*record));
}

/**
 * @brief 从已发布 ticket 取得执行 lease，用于后续入口/map 查询并延迟退役回收。
 * @note handle 必须是新输出槽，函数先清空；仅接受 PUBLISHED job/code，失败不加 lease，成功需 ReleaseCode；不可把 struct 复制当新 lease。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireCode(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    SZrExecutionCodeRecord *code;
    if (handle != ZR_NULL) zr_execution_backend_clear_handle(handle);
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || ticket == ZR_NULL ||
        handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, ticket != ZR_NULL ? ticket->ticketId : 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    job = zr_execution_backend_find_job_locked(service, ticket, ZR_NULL);
    if (job == ZR_NULL ||
        !zr_execution_backend_generation_equal(&job->ticket.generationKey,
                                               &ticket->generationKey)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET, diagnostic,
                ZR_EXECUTION_BACKEND_JOB_PUBLISHED, ZR_EXECUTION_BACKEND_JOB_FREE,
                ticket->ticketId, 0u, 0u, 0u);
    }
    if (job->ticket.state != ZR_EXECUTION_BACKEND_JOB_PUBLISHED ||
        job->codeSlot >= service->codeCapacity) {
        EZrExecutionBackendStatus status =
                job->ticket.state == ZR_EXECUTION_BACKEND_JOB_CANCELLED
                        ? ZR_EXECUTION_BACKEND_STATUS_CANCELLED
                        : ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED;
        TZrUInt32 actualState = job->ticket.state;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                status, diagnostic, ZR_EXECUTION_BACKEND_JOB_PUBLISHED,
                actualState, ticket->ticketId, 0u, 0u, 0u);
    }
    code = &service->codes[job->codeSlot];
    if (code->state != ZR_EXECUTION_BACKEND_CODE_PUBLISHED) {
        TZrUInt32 actualState = code->state;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED,
                diagnostic, ZR_EXECUTION_BACKEND_CODE_PUBLISHED, actualState,
                ticket->ticketId, code->info.codeIdentity, 0u, 0u);
    }
    if (code->leaseCount == UINT32_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, code->leaseCount, ticket->ticketId,
                code->info.codeIdentity, 0u, 0u);
    }
    /* 计数与完整身份同在锁内发布；句柄只代表这次 Acquire 的一份 lease，复制其字节不会再加计数。 */
    ++code->leaseCount;
    handle->magic = ZR_EXECUTION_BACKEND_MAGIC;
    handle->slotIndex = job->codeSlot;
    handle->serviceIdentity = service->serviceIdentity;
    handle->codeIdentity = code->info.codeIdentity;
    handle->generationKey = code->info.generationKey;
    handle->leased = ZR_TRUE;
    handle->dependencyLeased = ZR_FALSE;
    zr_execution_backend_unlock(service);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->ticketId = ticket->ticketId;
        diagnostic->codeIdentity = handle->codeIdentity;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * @brief 在有效执行句柄上加一份 map/import/deopt 依赖 lease，显式延长依赖有效期。
 * @note 同一 handle 仅可加一次，记录计数溢出前拒绝；成功后需该 handle ReleaseDependencyLease；退休状态仍按既有身份接受。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    EZrExecutionBackendStatus status;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    status = zr_execution_backend_validate_handle_locked(
            service, handle, &record, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_unlock(service);
        return status;
    }
    if (handle->dependencyLeased) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
                0u, 1u, 0u, record->info.codeIdentity, 0u, 0u);
    }
    if (record->dependencyLeaseCount == UINT32_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, record->dependencyLeaseCount, 0u,
                record->info.codeIdentity, 0u, 0u);
    }
    ++record->dependencyLeaseCount;
    handle->dependencyLeased = ZR_TRUE;
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->codeIdentity = record->info.codeIdentity;
    }
    zr_execution_backend_unlock(service);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * @brief 先归还该句柄的依赖保护，为之后释放执行 lease 和 collect 创造条件。
 * @note 必须 dependencyLeased 且记录依赖计数非零；失败保持 lease，成功只改依赖计数/标记，不释放代码。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    EZrExecutionBackendStatus status;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    status = zr_execution_backend_validate_handle_locked(
            service, handle, &record, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_unlock(service);
        return status;
    }
    if (!handle->dependencyLeased || record->dependencyLeaseCount == 0u) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
                1u, 0u, 0u, record->info.codeIdentity, 0u, 0u);
    }
    --record->dependencyLeaseCount;
    handle->dependencyLeased = ZR_FALSE;
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->codeIdentity = record->info.codeIdentity;
    }
    zr_execution_backend_unlock(service);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * @brief 归还一次执行 lease 并使本句柄失效，实际 backend 释放由后续 CollectRetired 完成。
 * @note 记录上任何 dependencyLeaseCount 非零都拒绝，包含其他 handle 的依赖；失败保留 handle/计数，成功不自动 collect。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseCode(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    EZrExecutionBackendStatus status;
    TZrUInt64 codeIdentity;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    status = zr_execution_backend_validate_handle_locked(
            service, handle, &record, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_unlock(service);
        return status;
    }
    codeIdentity = record->info.codeIdentity;
    /* 门禁看整个 record 的依赖计数；即使依赖属于另一个 handle，也不能先归还这次执行 lease。 */
    if (handle->dependencyLeased || record->dependencyLeaseCount != 0u) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE, diagnostic,
                0u, record->dependencyLeaseCount, 0u, codeIdentity, 0u, 0u);
    }
    if (record->leaseCount == 0u) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
                1u, 0u, 0u, codeIdentity, 0u, 0u);
    }
    --record->leaseCount;
    zr_execution_backend_clear_handle(handle);
    zr_execution_backend_unlock(service);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->codeIdentity = codeIdentity;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * @brief 取得此有效 lease 对应代码状态和两种计数的标量快照，供持有者观察退休与引用平衡。
 * @note view 先清空；成功复制不增加 lease，快照不是 backend 资源所有权或并发稳定性的证明。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryCode(
        const SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        SZrExecutionCodeView *view,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    EZrExecutionBackendStatus status;
    if (view != ZR_NULL) memset(view, 0, sizeof(*view));
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL ||
        view == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    zr_execution_backend_lock_const(service);
    status = zr_execution_backend_validate_handle_locked(
            (SZrExecutionBackendService *)service, handle, &record, diagnostic);
    if (status == ZR_EXECUTION_BACKEND_STATUS_OK) {
        view->info = record->info;
        view->state = record->state;
        view->leaseCount = record->leaseCount;
        view->dependencyLeaseCount = record->dependencyLeaseCount;
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
            diagnostic->codeIdentity = record->info.codeIdentity;
        }
    }
    zr_execution_backend_unlock((SZrExecutionBackendService *)service);
    return status;
}

/**
 * @brief 把 leased code 的标量快照交给已注册 backend 查询运行时入口地址。
 * @note 调用方需保留并协调同一执行 lease 覆盖查询与入口使用；回调计数保护 userData，不是额外 code lease；失败保留回调可能写出的地址，不能使用失败输出。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_LookupEntry(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionBackendCodeInfo code;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    if (entryAddress != ZR_NULL) *entryAddress = (TZrNativePtr)0;
    zr_execution_backend_diag_clear(diagnostic);
    if (entryAddress == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&code, 0, sizeof(code));
    zr_execution_backend_lock(service);
    status = zr_execution_backend_validate_handle_locked(
            service, handle, &record, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_unlock(service);
        return status;
    }
    if (record->registrationSlot >= service->registrationCapacity ||
        !service->registrations[record->registrationSlot].active) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED, diagnostic,
                1u, 0u, 0u, record->info.codeIdentity, 0u, 0u);
    }
    /* callback 计数保护复制的 descriptor/userData，锁外调用允许回查 service；执行 lease 仍由调用方维持。 */
    descriptor = service->registrations[record->registrationSlot].descriptor;
    code = record->info;
    if (descriptor.vtable.lookupEntry == ZR_NULL) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION, diagnostic,
                ZR_EXECUTION_BACKEND_OPERATION_DIRECT_CALL, 0u, 0u,
                code.codeIdentity, 0u, 0u);
    }
    if (!zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight, 0u,
                code.codeIdentity, 0u, 0u);
    }
    zr_execution_backend_unlock(service);
    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    status = descriptor.vtable.lookupEntry(
            &code, entryAddress, descriptor.userData, &callbackDiagnostic);
    zr_execution_backend_callback_leave(service);
    if (diagnostic != ZR_NULL) *diagnostic = callbackDiagnostic;
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        return zr_execution_backend_finish_callback(status, diagnostic,
                                                     code.codeIdentity);
    }
    if (*entryAddress == (TZrNativePtr)0) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID, diagnostic,
                1u, 0u, 0u, code.codeIdentity, 1u, 0u);
    }
    return zr_execution_backend_finish_callback(
            ZR_EXECUTION_BACKEND_STATUS_OK, diagnostic, code.codeIdentity);
}

/**
 * @brief 查询该代码已登记的指定 map hash；可由 backend 提供或使用 code 元数据快照。
 * @note 必须有效 leased handle 和 map bit；本查询不自动取得 dependency lease；零 hash 拒绝，失败后的回调输出不可视为有效图。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryMap(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record = ZR_NULL;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionBackendCodeInfo code;
    TZrUInt32 mapFlag = 0u;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    if (mapHash != ZR_NULL) *mapHash = 0u;
    zr_execution_backend_diag_clear(diagnostic);
    if (mapHash == ZR_NULL || !zr_execution_backend_map_kind_valid(mapKind)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, handle != ZR_NULL ? handle->codeIdentity : 0u, 0u, 0u);
    }
    if (!zr_execution_backend_service_shape_valid(service) || handle == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&code, 0, sizeof(code));
    zr_execution_backend_lock(service);
    status = zr_execution_backend_validate_handle_locked(
            service, handle, &record, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_unlock(service);
        return status;
    }
    if (!zr_execution_backend_map_flag_for_kind(mapKind, &mapFlag) ||
        (record->info.mapRegistrationFlags & mapFlag) == 0u) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
                diagnostic, mapFlag, record->info.mapRegistrationFlags, 0u,
                record->info.codeIdentity, 0u, 0u);
    }
    if (record->registrationSlot >= service->registrationCapacity ||
        !service->registrations[record->registrationSlot].active) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED, diagnostic,
                1u, 0u, 0u, record->info.codeIdentity, 0u, 0u);
    }
    code = record->info;
    descriptor = service->registrations[record->registrationSlot].descriptor;
    if (descriptor.vtable.queryMap != ZR_NULL &&
        !zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight, 0u,
                code.codeIdentity, 0u, 0u);
    }
    zr_execution_backend_unlock(service);

    /* 没有 map 回调时返回记录中的标量见证；这条路径不创建图资源或额外依赖 lease。 */
    if (descriptor.vtable.queryMap == ZR_NULL) {
        *mapHash = zr_execution_backend_code_map_hash(&code, mapKind);
        if (*mapHash == 0u) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
                    diagnostic, 1u, 0u, 0u, code.codeIdentity, 0u, 0u);
        }
        return zr_execution_backend_finish_callback(
                ZR_EXECUTION_BACKEND_STATUS_OK, diagnostic, code.codeIdentity);
    }
    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    status = descriptor.vtable.queryMap(
            &code, mapKind, mapHash, descriptor.userData, &callbackDiagnostic);
    zr_execution_backend_callback_leave(service);
    if (diagnostic != ZR_NULL) *diagnostic = callbackDiagnostic;
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        return zr_execution_backend_finish_callback(status, diagnostic,
                                                     code.codeIdentity);
    }
    if (*mapHash == 0u) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
                diagnostic, 1u, 0u, 0u, code.codeIdentity, 1u, 0u);
    }
    return zr_execution_backend_finish_callback(
            ZR_EXECUTION_BACKEND_STATUS_OK, diagnostic, code.codeIdentity);
}

/* 对一份无执行/依赖 lease 的退休记录撤图和 retire，成功后才归还 code/job slot。
 * RETIRED/RETIRE_FAILED 才可认领；RECLAIMING 阻止重复收集；撤图失败不调用 retire，失败保留记录供重试，已撤图成功则不重复撤图。 */
EZrExecutionBackendStatus zr_execution_backend_collect_code_slot(
        SZrExecutionBackendService *service,
        TZrUInt32 slotIndex,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCodeRecord *record;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionBackendCodeInfo code;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    TZrUInt32 jobSlot;
    TZrUInt64 codeIdentity;
    TZrBool mapsRegistered;
    EZrExecutionBackendStatus unregisterStatus;
    TZrBool callbackPinned = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_execution_backend_service_shape_valid(service) ||
        slotIndex >= service->codeCapacity) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&code, 0, sizeof(code));
    zr_execution_backend_lock(service);
    record = &service->codes[slotIndex];
    if (record->state != ZR_EXECUTION_BACKEND_CODE_RETIRED &&
        record->state != ZR_EXECUTION_BACKEND_CODE_RETIRE_FAILED) {
        zr_execution_backend_unlock(service);
        return ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND;
    }
    if (record->leaseCount != 0u || record->dependencyLeaseCount != 0u) {
        TZrUInt32 leases = record->leaseCount;
        if (UINT32_MAX - leases < record->dependencyLeaseCount) {
            leases = UINT32_MAX;
        } else {
            leases += record->dependencyLeaseCount;
        }
        codeIdentity = record->info.codeIdentity;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE, diagnostic,
                0u, leases, 0u, codeIdentity, 0u, 0u);
    }
    if (record->registrationSlot >= service->registrationCapacity ||
        !service->registrations[record->registrationSlot].active) {
        codeIdentity = record->info.codeIdentity;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED, diagnostic,
                1u, 0u, 0u, codeIdentity, 0u, 0u);
    }
    descriptor = service->registrations[record->registrationSlot].descriptor;
    /* 在解锁调用 backend 前认领记录，另一 collector 会跳过 RECLAIMING；回调返回后仍按 slot/code 身份核对提交。 */
    code = record->info;
    jobSlot = record->jobSlot;
    codeIdentity = code.codeIdentity;
    mapsRegistered = record->mapsRegistered;
    if (!zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight, 0u,
                codeIdentity, 0u, 0u);
    }
    callbackPinned = ZR_TRUE;
    record->state = ZR_EXECUTION_BACKEND_CODE_RECLAIMING;
    zr_execution_backend_unlock(service);

    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    unregisterStatus = ZR_EXECUTION_BACKEND_STATUS_OK;
    status = ZR_EXECUTION_BACKEND_STATUS_OK;
    if (mapsRegistered && descriptor.vtable.unregisterMaps != ZR_NULL) {
        unregisterStatus = descriptor.vtable.unregisterMaps(
                &code, descriptor.userData, &callbackDiagnostic);
    } else if (mapsRegistered) {
        unregisterStatus = ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
    }
    status = unregisterStatus;
    if (status == ZR_EXECUTION_BACKEND_STATUS_OK) {
        memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
        if (descriptor.vtable.retire != ZR_NULL) {
            status = descriptor.vtable.retire(
                    &code, descriptor.userData, &callbackDiagnostic);
        } else {
            status = ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
        }
    }
    /* 只在完整清理成功后归还槽；失败保留重试状态，撤图已成功时不重复撤图，retire 尚未成功时不清 record。 */
    if (callbackPinned) zr_execution_backend_callback_leave(service);
    zr_execution_backend_lock(service);
    record = &service->codes[slotIndex];
    if (record->state == ZR_EXECUTION_BACKEND_CODE_RECLAIMING &&
        record->info.codeIdentity == codeIdentity) {
        if (status == ZR_EXECUTION_BACKEND_STATUS_OK) {
            zr_execution_backend_reset_code_record_locked(record);
            if (service->codeCount != 0u) --service->codeCount;
            if (jobSlot < service->jobCapacity &&
                service->jobs[jobSlot].codeSlot == slotIndex) {
                zr_execution_backend_reset_job_locked(&service->jobs[jobSlot]);
                if (service->jobCount != 0u) --service->jobCount;
            }
        } else {
            record->state = ZR_EXECUTION_BACKEND_CODE_RETIRE_FAILED;
            record->mapsRegistered = (TZrBool)(unregisterStatus !=
                                               ZR_EXECUTION_BACKEND_STATUS_OK);
        }
    }
    zr_execution_backend_unlock(service);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        return zr_execution_backend_finish_callback(
                ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED, diagnostic,
                codeIdentity);
    }
    return zr_execution_backend_finish_callback(
            ZR_EXECUTION_BACKEND_STATUS_OK, diagnostic, codeIdentity);
}

/**
 * @brief 让显式 owner 或 FinalizeShutdown 扫描退休记录，统计本次成功回收并报告首次硬失败。
 * @note 跳过仍有 lease 或当前不处于可回收状态的 slot；失败 outCollected 保留已完成数量，无全扫描事务回滚；不保证其他并发 collector 的回收计数。
 */
EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CollectRetired(
        SZrExecutionBackendService *service,
        TZrUInt32 *outCollected,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 collected = 0u;
    EZrExecutionBackendStatus status;
    if (outCollected != ZR_NULL) *outCollected = 0u;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) ||
        outCollected == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    for (index = 0u; index < service->codeCapacity; ++index) {
        status = zr_execution_backend_collect_code_slot(service, index, diagnostic);
        if (status == ZR_EXECUTION_BACKEND_STATUS_OK) {
            ++collected;
        } else if (status == ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE ||
                   status == ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND) {
            continue;
        } else {
            *outCollected = collected;
            return status;
        }
    }
    *outCollected = collected;
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->actual = collected;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}
