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
