#include "execution_backend_internal.h"

#include <stdint.h>

/* Registration, cancellation, invalidation and service lifetime. */


void ZrCore_ExecutionBackend_DiagnosticClear(
        SZrExecutionBackendDiagnostic *diagnostic) {
    zr_execution_backend_diag_clear(diagnostic);
}

TZrBool ZrCore_ExecutionBackend_GenerationKeyIsValid(
        const SZrExecutionGenerationKey *key) {
    return zr_execution_backend_key_valid(key, ZR_TRUE);
}

TZrBool ZrCore_ExecutionBackend_GenerationKeyEqual(
        const SZrExecutionGenerationKey *left,
        const SZrExecutionGenerationKey *right) {
    return zr_execution_backend_generation_equal(left, right);
}

const TZrChar *ZrCore_ExecutionBackend_StatusName(
        EZrExecutionBackendStatus status) {
    switch (status) {
        case ZR_EXECUTION_BACKEND_STATUS_OK: return "ok";
        case ZR_EXECUTION_BACKEND_STATUS_PENDING: return "pending";
        case ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC: return "fallback-execbc";
        case ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT: return "fallback-aot";
        case ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC: return "invalid-magic";
        case ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA: return "invalid-schema";
        case ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS: return "invalid-flags";
        case ZR_EXECUTION_BACKEND_STATUS_CAPACITY: return "capacity";
        case ZR_EXECUTION_BACKEND_STATUS_ALREADY_REGISTERED: return "already-registered";
        case ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED: return "not-registered";
        case ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE: return "backend-unavailable";
        case ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET: return "unsupported-target";
        case ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION: return "unsupported-operation";
        case ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED: return "immutable-input-required";
        case ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH: return "contract-mismatch";
        case ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION: return "stale-generation";
        case ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET: return "stale-ticket";
        case ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE: return "invalid-state";
        case ZR_EXECUTION_BACKEND_STATUS_CANCELLED: return "cancelled";
        case ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED: return "compile-failed";
        case ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID: return "code-invalid";
        case ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND: return "code-not-found";
        case ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED: return "code-not-published";
        case ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE: return "map-registration-incomplete";
        case ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE: return "active-lease";
        case ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED: return "retire-failed";
        case ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN: return "shutting-down";
        case ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT: return "in-flight";
        case ZR_EXECUTION_BACKEND_STATUS_RESUME_UNAVAILABLE: return "resume-unavailable";
        case ZR_EXECUTION_BACKEND_STATUS_RESUME_FAILED: return "resume-failed";
        case ZR_EXECUTION_BACKEND_STATUS_OVERFLOW: return "overflow";
        case ZR_EXECUTION_BACKEND_STATUS_COUNT:
        default: return "backend-error";
    }
}

/* 在占用注册槽前区分版本、标志、目标和能力拒绝原因；必需清理回调缺失不能成为有效注册。 */
static EZrExecutionBackendStatus zr_execution_backend_validate_descriptor(
        const SZrExecutionBackendDescriptor *descriptor,
        SZrExecutionBackendDiagnostic *diagnostic) {
    zr_execution_backend_diag_clear(diagnostic);
    if (descriptor == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (descriptor->magic != ZR_EXECUTION_BACKEND_MAGIC) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, descriptor->magic, 0u, 0u, 0u, 0u);
    }
    if (descriptor->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        descriptor->target.schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA, diagnostic,
                ZR_EXECUTION_BACKEND_SCHEMA_VERSION,
                descriptor->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION
                    ? descriptor->schemaVersion : descriptor->target.schemaVersion,
                0u, 0u, 0u, 0u);
    }
    if (!zr_execution_backend_descriptor_shape_valid(descriptor)) {
        if ((descriptor->flags & ~ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK) != 0u) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS, diagnostic,
                    ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK,
                    descriptor->flags, 0u, 0u, 0u, 0u);
        }
        if (!zr_execution_backend_target_kind_valid(descriptor->backendKind) ||
            descriptor->backendKind != descriptor->target.kind) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET, diagnostic,
                    descriptor->backendKind, descriptor->target.kind,
                    0u, 0u, 0u, 0u);
        }
        if (descriptor->supportedOperations == 0u ||
            (descriptor->supportedOperations &
             ~ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK) != 0u) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION, diagnostic,
                    ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK,
                    descriptor->supportedOperations, 0u, 0u, 0u, 0u);
        }
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    if (descriptor->reserved != 0u || descriptor->target.reserved != 0u) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH, diagnostic,
                0u, 0u, 0u, 0u, 0u,
                descriptor->reserved != 0u ? descriptor->reserved
                                           : descriptor->target.reserved);
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/* 注销前保留仍可能编译、发布或关联代码的作业注册；失败与取消作业不单独阻止注销。 */
static TZrBool zr_execution_backend_job_references_registration(
        const SZrExecutionCompileJob *job, TZrUInt32 registrationSlot) {
    if (job == ZR_NULL || job->ticket.magic != ZR_EXECUTION_BACKEND_MAGIC) {
        return ZR_FALSE;
    }
    if (job->registrationSlot != registrationSlot) return ZR_FALSE;
    return (TZrBool)(job->ticket.state == ZR_EXECUTION_BACKEND_JOB_QUEUED ||
                     job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING ||
                     job->ticket.state == ZR_EXECUTION_BACKEND_JOB_READY ||
                     job->ticket.state == ZR_EXECUTION_BACKEND_JOB_PUBLISHED);
}

/* 任何非 FREE 记录仍需要原注册的查询或清理回调，不能以退休替代资源归还。 */
static TZrBool zr_execution_backend_code_references_registration(
        const SZrExecutionCodeRecord *code, TZrUInt32 registrationSlot) {
    if (code == ZR_NULL || code->state == ZR_EXECUTION_BACKEND_CODE_FREE) {
        return ZR_FALSE;
    }
    return (TZrBool)(code->registrationSlot == registrationSlot);
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Init(
        SZrExecutionBackendService *service,
        TZrUInt64 serviceIdentity,
        SZrExecutionBackendRegistrationRecord *registrations,
        TZrUInt32 registrationCapacity,
        SZrExecutionCompileJob *jobs,
        TZrUInt32 jobCapacity,
        SZrExecutionCodeRecord *codes,
        TZrUInt32 codeCapacity,
        FZrExecutionBackendResumeInterpreter resumeInterpreter,
        TZrPtr resumeUserData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    zr_execution_backend_diag_clear(diagnostic);
    if (service == ZR_NULL || serviceIdentity == 0u || registrations == ZR_NULL ||
        registrationCapacity == 0u || jobs == ZR_NULL || jobCapacity == 0u ||
        codes == ZR_NULL || codeCapacity == 0u ||
        (void *)registrations == (void *)jobs ||
        (void *)registrations == (void *)codes || (void *)jobs == (void *)codes) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    memset(service, 0, sizeof(*service));
    service->magic = ZR_EXECUTION_BACKEND_MAGIC;
    service->schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    service->serviceIdentity = serviceIdentity;
    service->nextRegistrationIdentity = 1u;
    service->nextTicketId = 1u;
    service->registrations = registrations;
    service->registrationCapacity = registrationCapacity;
    service->jobs = jobs;
    service->jobCapacity = jobCapacity;
    service->codes = codes;
    service->codeCapacity = codeCapacity;
    service->resumeInterpreter = resumeInterpreter;
    service->resumeUserData = resumeUserData;
    memset(registrations, 0,
           (TZrSize)registrationCapacity * sizeof(*registrations));
    memset(jobs, 0, (TZrSize)jobCapacity * sizeof(*jobs));
    memset(codes, 0, (TZrSize)codeCapacity * sizeof(*codes));
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

void ZrCore_ExecutionBackendService_Deinit(
        SZrExecutionBackendService *service) {
    if (service == ZR_NULL || !service->destroyed) return;
    service->magic = 0u;
    service->schemaVersion = 0u;
    service->registrations = ZR_NULL;
    service->jobs = ZR_NULL;
    service->codes = ZR_NULL;
    service->registrationCapacity = 0u;
    service->jobCapacity = 0u;
    service->codeCapacity = 0u;
    service->registrationCount = 0u;
    service->jobCount = 0u;
    service->codeCount = 0u;
    service->queryTargetInFlight = 0u;
    service->backendCallbackInFlight = 0u;
    service->invalidatedKeyCount = 0u;
    service->resumeInterpreter = ZR_NULL;
    service->resumeUserData = ZR_NULL;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Register(
        SZrExecutionBackendService *service,
        const SZrExecutionBackendDescriptor *descriptor,
        SZrExecutionBackendRegistration *registration,
        SZrExecutionBackendDiagnostic *diagnostic) {
    EZrExecutionBackendStatus status;
    TZrUInt32 index;
    TZrUInt32 slot = UINT32_MAX;
    TZrUInt64 identity;
    if (registration != ZR_NULL) memset(registration, 0, sizeof(*registration));
    status = zr_execution_backend_validate_descriptor(descriptor, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) return status;
    if (!zr_execution_backend_service_shape_valid(service)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, 0u, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    if (service->shuttingDown || service->destroyed) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
    for (index = 0u; index < service->registrationCapacity; ++index) {
        SZrExecutionBackendRegistrationRecord *record =
                &service->registrations[index];
        if (record->active &&
            record->descriptor.backendKind == descriptor->backendKind &&
            record->descriptor.target.targetTripleHash ==
                    descriptor->target.targetTripleHash &&
            record->descriptor.target.layoutHash == descriptor->target.layoutHash &&
            record->descriptor.target.capabilityHash ==
                    descriptor->target.capabilityHash) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_ALREADY_REGISTERED, diagnostic,
                    descriptor->backendKind, record->descriptor.backendKind,
                    0u, 0u, descriptor->target.targetTripleHash,
                    record->descriptor.target.targetTripleHash);
        }
        if (!record->active && slot == UINT32_MAX) slot = index;
    }
    if (slot == UINT32_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CAPACITY, diagnostic,
                service->registrationCapacity, service->registrationCount,
                0u, 0u, 0u, 0u);
    }
    identity = service->nextRegistrationIdentity;
    if (identity == 0u || identity == UINT64_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, (TZrUInt32)identity, 0u, 0u, 0u, 0u);
    }
    ++service->nextRegistrationIdentity;
    service->registrations[slot].registrationIdentity = identity;
    service->registrations[slot].active = ZR_TRUE;
    service->registrations[slot].descriptor = *descriptor;
    ++service->registrationCount;
    if (registration != ZR_NULL) {
        registration->serviceIdentity = service->serviceIdentity;
        registration->registrationIdentity = identity;
        registration->backendKind = descriptor->backendKind;
    }
    zr_execution_backend_unlock(service);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Unregister(
        SZrExecutionBackendService *service,
        TZrUInt64 registrationIdentity,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionBackendDescriptor descriptor;
    TZrUInt32 registrationSlot = UINT32_MAX;
    TZrUInt32 index;
    EZrExecutionBackendStatus status = ZR_EXECUTION_BACKEND_STATUS_OK;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) ||
        registrationIdentity == 0u) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    memset(&descriptor, 0, sizeof(descriptor));
    zr_execution_backend_lock(service);
    if (service->shuttingDown || service->destroyed) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (zr_execution_backend_find_registration_locked(
                service, registrationIdentity, &registrationSlot) == ZR_NULL) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED, diagnostic,
                0u, 0u, 0u, 0u, registrationIdentity, 0u);
    }
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (zr_execution_backend_job_references_registration(
                    &service->jobs[index], registrationSlot)) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                    ZR_EXECUTION_BACKEND_JOB_FREE,
                    service->jobs[index].ticket.state,
                    service->jobs[index].ticket.ticketId, 0u, 0u, 0u);
        }
    }
    for (index = 0u; index < service->codeCapacity; ++index) {
        if (zr_execution_backend_code_references_registration(
                    &service->codes[index], registrationSlot)) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE, diagnostic,
                    ZR_EXECUTION_BACKEND_CODE_FREE, service->codes[index].state,
                    0u, service->codes[index].info.codeIdentity, 0u, 0u);
        }
    }
    if (service->backendCallbackInFlight != 0u) {
        TZrUInt32 callbacks = service->backendCallbackInFlight;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                0u, callbacks, 0u, 0u, 0u, 0u);
    }
    if (!zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight, 0u, 0u, 0u, 0u);
    }
    descriptor = service->registrations[registrationSlot].descriptor;
    memset(&service->registrations[registrationSlot], 0,
           sizeof(service->registrations[registrationSlot]));
    if (service->registrationCount != 0u) --service->registrationCount;
    zr_execution_backend_unlock(service);
    if (descriptor.vtable.destroy != ZR_NULL) {
        descriptor.vtable.destroy(descriptor.userData);
    }
    zr_execution_backend_callback_leave(service);
    return status;
}

/* 锁内保存取消意图及回调快照，锁外发送取消，再按 ticketId 和状态核对结果；失败或 PENDING 不冒充取消完成。 */
static EZrExecutionBackendStatus zr_execution_backend_cancel_compiling_one(
        SZrExecutionBackendService *service,
        TZrUInt32 jobSlot,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileTicket ticket;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    EZrExecutionBackendStatus status;
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&ticket, 0, sizeof(ticket));
    zr_execution_backend_lock(service);
    if (jobSlot >= service->jobCapacity ||
        service->jobs[jobSlot].ticket.state != ZR_EXECUTION_BACKEND_JOB_COMPILING) {
        zr_execution_backend_unlock(service);
        return ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND;
    }
    service->jobs[jobSlot].cancelRequested = ZR_TRUE;
    ticket = service->jobs[jobSlot].ticket;
    descriptor = service->registrations[service->jobs[jobSlot].registrationSlot].descriptor;
    if (!zr_execution_backend_callback_enter_locked(service)) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight,
                ticket.ticketId, 0u, 0u, 0u);
    }
    zr_execution_backend_unlock(service);
    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    if (descriptor.vtable.cancelCompile != ZR_NULL) {
        status = descriptor.vtable.cancelCompile(&ticket, descriptor.userData,
                                                 &callbackDiagnostic);
    } else {
        status = ZR_EXECUTION_BACKEND_STATUS_OK;
    }
    zr_execution_backend_lock(service);
    zr_execution_backend_callback_leave_locked(service);
    if (jobSlot < service->jobCapacity &&
        service->jobs[jobSlot].ticket.ticketId == ticket.ticketId &&
        service->jobs[jobSlot].ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
        if (status == ZR_EXECUTION_BACKEND_STATUS_OK ||
            status == ZR_EXECUTION_BACKEND_STATUS_CANCELLED) {
            service->jobs[jobSlot].ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
            service->jobs[jobSlot].lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        }
    }
    zr_execution_backend_unlock(service);
    if (diagnostic != ZR_NULL) {
        *diagnostic = callbackDiagnostic;
        diagnostic->status = status;
        diagnostic->ticketId = ticket.ticketId;
    }
    return status;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Cancel(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    TZrUInt32 jobSlot;
    EZrExecutionBackendJobState state;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || ticket == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, ticket != ZR_NULL ? ticket->ticketId : 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    job = zr_execution_backend_find_job_locked(service, ticket, &jobSlot);
    if (job == ZR_NULL) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET, diagnostic,
                1u, 0u, ticket->ticketId, 0u, 0u, 0u);
    }
    state = job->ticket.state;
    if (state == ZR_EXECUTION_BACKEND_JOB_QUEUED) {
        job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
        job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CANCELLED, diagnostic,
                ZR_EXECUTION_BACKEND_JOB_QUEUED,
                ZR_EXECUTION_BACKEND_JOB_CANCELLED, ticket->ticketId, 0u, 0u, 0u);
    }
    if (state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_cancel_compiling_one(service, jobSlot, diagnostic);
    }
    if (state == ZR_EXECUTION_BACKEND_JOB_CANCELLED) {
        zr_execution_backend_unlock(service);
        return ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
    }
    zr_execution_backend_unlock(service);
    return zr_execution_backend_fail(
            ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
            ZR_EXECUTION_BACKEND_JOB_COMPILING, state, ticket->ticketId, 0u, 0u, 0u);
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_InvalidateGeneration(
        SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 compiling;
    TZrBool sawMatch = ZR_FALSE;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) ||
        !zr_execution_backend_key_valid(key, ZR_TRUE)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    if (service->destroyed) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
/* 墓碑先于记录扫描提交；容量拒绝发生在取消或退休之前，避免部分失效却不能阻止重新入队。 */
    if (!zr_execution_backend_generation_invalidated_locked(service, key)) {
        if (service->invalidatedKeyCount >=
                ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_CAPACITY, diagnostic,
                    ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY,
                    service->invalidatedKeyCount, 0u, 0u, 0u, 0u);
        }
        service->invalidatedKeys[service->invalidatedKeyCount++] = *key;
    }
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (zr_execution_backend_generation_equal(
                    &service->jobs[index].request.generationKey, key)) {
            sawMatch = ZR_TRUE;
            break;
        }
    }
    if (!sawMatch) {
        for (index = 0u; index < service->codeCapacity; ++index) {
            if (service->codes[index].state != ZR_EXECUTION_BACKEND_CODE_FREE &&
                zr_execution_backend_generation_equal(
                        &service->codes[index].info.generationKey, key)) {
                sawMatch = ZR_TRUE;
                break;
            }
        }
    }
    zr_execution_backend_mark_jobs_cancelled_locked(service, key, &compiling);
    zr_execution_backend_mark_codes_retired_locked(service, key, ZR_NULL);
    zr_execution_backend_unlock(service);

    /* Cancellation callbacks are dispatched one at a time, outside the lock;
     * this permits a backend to call back into QueryTicket safely. */
    for (;;) {
        TZrUInt32 pendingSlot = UINT32_MAX;
        zr_execution_backend_lock(service);
        for (index = 0u; index < service->jobCapacity; ++index) {
            if (service->jobs[index].ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING &&
                service->jobs[index].cancelRequested &&
                zr_execution_backend_generation_equal(
                        &service->jobs[index].request.generationKey, key)) {
                pendingSlot = index;
                break;
            }
        }
        zr_execution_backend_unlock(service);
        if (pendingSlot == UINT32_MAX) break;
        {
            EZrExecutionBackendStatus cancelStatus =
                    zr_execution_backend_cancel_compiling_one(
                            service, pendingSlot, diagnostic);
            if (cancelStatus != ZR_EXECUTION_BACKEND_STATUS_OK &&
                cancelStatus != ZR_EXECUTION_BACKEND_STATUS_CANCELLED) {
                break;
            }
            zr_execution_backend_lock(service);
            if (service->jobs[pendingSlot].ticket.state ==
                    ZR_EXECUTION_BACKEND_JOB_COMPILING) {
                zr_execution_backend_unlock(service);
                break;
            }
            zr_execution_backend_unlock(service);
        }
    }
/* 返回 OK 的契约是失效标记完成；取消回调失败或仍编译不在这里转换成整体失败，资源回收需单独观察。 */
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->expectedKey = *key;
        diagnostic->actualKey = *key;
        diagnostic->actual = sawMatch ? 1u : 0u;
    }
    (void)compiling;
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Shutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 compiling = 0u;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, 0u, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    if (service->destroyed) {
        zr_execution_backend_unlock(service);
        return ZR_EXECUTION_BACKEND_STATUS_OK;
    }
    service->shuttingDown = ZR_TRUE;
    for (index = 0u; index < service->jobCapacity; ++index) {
        SZrExecutionCompileJob *job = &service->jobs[index];
        if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_QUEUED) {
            job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
            job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        } else if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            job->cancelRequested = ZR_TRUE;
            ++compiling;
        }
    }
    for (index = 0u; index < service->codeCapacity; ++index) {
        SZrExecutionCodeRecord *code = &service->codes[index];
        if (code->state == ZR_EXECUTION_BACKEND_CODE_READY ||
            code->state == ZR_EXECUTION_BACKEND_CODE_PUBLISHED) {
            code->state = ZR_EXECUTION_BACKEND_CODE_RETIRED;
        }
    }
    zr_execution_backend_unlock(service);

    /* Ask each compiling backend to cancel without holding the service lock. */
    for (;;) {
        TZrUInt32 pendingSlot = UINT32_MAX;
        zr_execution_backend_lock(service);
        for (index = 0u; index < service->jobCapacity; ++index) {
            if (service->jobs[index].ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING &&
                service->jobs[index].cancelRequested) {
                pendingSlot = index;
                break;
            }
        }
        zr_execution_backend_unlock(service);
        if (pendingSlot == UINT32_MAX) break;
        (void)zr_execution_backend_cancel_compiling_one(service, pendingSlot, diagnostic);
        /* A backend may return PENDING from cancel. Avoid spinning; the worker
         * must complete the ticket and the caller can invoke Shutdown again. */
        zr_execution_backend_lock(service);
        if (service->jobs[pendingSlot].ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            zr_execution_backend_unlock(service);
            break;
        }
        zr_execution_backend_unlock(service);
    }
    /* Cancellation callbacks may have completed synchronously.  Re-read the
     * state instead of returning IN_FLIGHT solely because a job was compiling
     * when shutdown began. */
    compiling = 0u;
    zr_execution_backend_lock(service);
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (service->jobs[index].ticket.state ==
                ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            ++compiling;
        }
    }
    if (service->queryTargetInFlight != 0u) {
        compiling += service->queryTargetInFlight;
    }
    if (service->backendCallbackInFlight != 0u) {
        if (compiling > UINT32_MAX - service->backendCallbackInFlight) {
            compiling = UINT32_MAX;
        } else {
            compiling += service->backendCallbackInFlight;
        }
    }
    zr_execution_backend_unlock(service);
    if (compiling != 0u) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                0u, compiling, 0u, 0u, 0u, 0u);
    }
    if (diagnostic != ZR_NULL) diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_FinalizeShutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 collected = 0u;
    EZrExecutionBackendStatus status;
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, 0u, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    if (!service->shuttingDown) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (service->jobs[index].ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                    ZR_EXECUTION_BACKEND_JOB_READY,
                    ZR_EXECUTION_BACKEND_JOB_COMPILING,
                    service->jobs[index].ticket.ticketId, 0u, 0u, 0u);
        }
    }
    if (service->queryTargetInFlight != 0u ||
        service->backendCallbackInFlight != 0u) {
        TZrUInt32 pending = service->queryTargetInFlight;
        if (pending <= UINT32_MAX - service->backendCallbackInFlight) {
            pending += service->backendCallbackInFlight;
        } else {
            pending = UINT32_MAX;
        }
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                0u, pending, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_unlock(service);
    status = ZrCore_ExecutionBackendService_CollectRetired(
            service, &collected, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) return status;

    zr_execution_backend_lock(service);
    if (service->queryTargetInFlight != 0u ||
        service->backendCallbackInFlight != 0u) {
        TZrUInt32 pendingQueries = service->queryTargetInFlight;
        if (pendingQueries <= UINT32_MAX - service->backendCallbackInFlight) {
            pendingQueries += service->backendCallbackInFlight;
        } else {
            pendingQueries = UINT32_MAX;
        }
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT, diagnostic,
                0u, pendingQueries, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_unlock(service);

    /* Registration callbacks own the code/map implementation.  Keep those
     * callbacks alive while any retired record still has a lease; otherwise a
     * later ReleaseCode/CollectRetired could no longer reach the owner. */
/* 所有代码记录清空后才销毁 owner；RETIRE_FAILED 也占 codeCount，不能以关闭请求替代资源清理。 */
    zr_execution_backend_lock(service);
    if (service->codeCount != 0u) {
        TZrUInt32 activeCodes = service->codeCount;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE, diagnostic,
                0u, activeCodes, 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_unlock(service);

    for (;;) {
        SZrExecutionBackendDescriptor descriptor;
        TZrUInt32 registrationSlot = UINT32_MAX;
        memset(&descriptor, 0, sizeof(descriptor));
        zr_execution_backend_lock(service);
        if (service->backendCallbackInFlight == UINT32_MAX) {
            zr_execution_backend_unlock(service);
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                    UINT32_MAX, service->backendCallbackInFlight,
                    0u, 0u, 0u, 0u);
        }
        for (index = 0u; index < service->registrationCapacity; ++index) {
            if (service->registrations[index].active) {
                registrationSlot = index;
                descriptor = service->registrations[index].descriptor;
                ++service->backendCallbackInFlight;
                memset(&service->registrations[index], 0,
                       sizeof(service->registrations[index]));
                if (service->registrationCount != 0u) --service->registrationCount;
                break;
            }
        }
        zr_execution_backend_unlock(service);
        if (registrationSlot == UINT32_MAX) break;
        if (descriptor.vtable.destroy != ZR_NULL) descriptor.vtable.destroy(descriptor.userData);
        zr_execution_backend_callback_leave(service);
    }
    zr_execution_backend_lock(service);
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (service->jobs[index].ticket.state != ZR_EXECUTION_BACKEND_JOB_FREE) {
            zr_execution_backend_reset_job_locked(&service->jobs[index]);
        }
    }
    if (service->codeCount != 0u || service->registrationCount != 0u) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE, diagnostic,
                0u, service->codeCount, 0u, 0u, 0u, 0u);
    }
    service->destroyed = ZR_TRUE;
    service->shuttingDown = ZR_FALSE;
    zr_execution_backend_unlock(service);
    (void)collected;
    if (diagnostic != ZR_NULL) diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

TZrBool ZrCore_ExecutionBackendService_ResumeInterpreter(
        SZrExecutionBackendService *service,
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    FZrExecutionBackendResumeInterpreter resume;
    TZrPtr userData;
    TZrBool callbackPinned = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_execution_backend_service_shape_valid(service) || request == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    zr_execution_backend_lock(service);
/* 恢复入口未检查 shuttingDown/destroyed；TODO: 宿主接入 ResumeInterpreter 时核对最终关闭与恢复的并发约束，当前 mock 只覆盖关闭前的串行恢复。 */
    resume = service->resumeInterpreter;
    userData = service->resumeUserData;
    if (resume != ZR_NULL) {
        callbackPinned = zr_execution_backend_callback_enter_locked(service);
    }
    zr_execution_backend_unlock(service);
    if (resume == ZR_NULL) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        return ZR_FALSE;
    }
    if (!callbackPinned) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        }
        return ZR_FALSE;
    }
    if (!resume(request, userData, diagnostic)) {
        zr_execution_backend_callback_leave(service);
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
        }
        return ZR_FALSE;
    }
    zr_execution_backend_callback_leave(service);
    return ZR_TRUE;
}
