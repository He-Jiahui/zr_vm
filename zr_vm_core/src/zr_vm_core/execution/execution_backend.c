#include "execution_backend_internal.h"

#include <stdint.h>

/*
 * The service owns no worker thread. CompileAsync only snapshots an input and
 * queues it; a backend worker (or the host scheduler) drives ProcessNext and
 * Complete. This keeps the frame thread non-blocking and makes cancellation
 * and generation checks explicit at every hand-off.
 */

static volatile TZrUInt32 zr_execution_backend_default_lock;
static SZrExecutionBackendService *zr_execution_backend_default_service;

static void zr_execution_backend_default_lock_enter(void) {
#if defined(_MSC_VER)
    while (_InterlockedExchange((volatile long *)&zr_execution_backend_default_lock,
                                1L) != 0L) {
    }
#else
    while (__sync_lock_test_and_set(&zr_execution_backend_default_lock, 1u) != 0u) {
    }
#endif
}

static void zr_execution_backend_default_lock_leave(void) {
#if defined(_MSC_VER)
    _InterlockedExchange((volatile long *)&zr_execution_backend_default_lock, 0L);
#else
    __sync_lock_release(&zr_execution_backend_default_lock);
#endif
}

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
                descriptor->schemaVersion, 0u, 0u, 0u, 0u);
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
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

static EZrExecutionBackendStatus zr_execution_backend_validate_request(
        const SZrExecutionCompileRequest *request,
        SZrExecutionBackendDiagnostic *diagnostic) {
    zr_execution_backend_diag_clear(diagnostic);
    if (request == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = request->sourceId;
        diagnostic->instructionId = request->instructionId;
    }
    if (request->magic != ZR_EXECUTION_BACKEND_MAGIC) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, request->magic, 0u, 0u, 0u, 0u);
    }
    if (request->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA, diagnostic,
                ZR_EXECUTION_BACKEND_SCHEMA_VERSION, request->schemaVersion,
                0u, 0u, 0u, 0u);
    }
    if (!zr_execution_backend_request_shape_valid(request)) {
        if ((request->flags & ~ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK) != 0u) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS, diagnostic,
                    ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK,
                    request->flags, 0u, 0u, 0u, 0u);
        }
        if ((request->flags & ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT) == 0u) {
            return zr_execution_backend_fail(
                    ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED,
                    diagnostic, ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT,
                    request->flags, 0u, 0u, 0u, 0u);
        }
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH, diagnostic,
                1u, 0u, 0u, 0u, request->generationKey.moduleIdentity,
                request->contract.moduleHash);
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

static void zr_execution_backend_fill_fallback(
        const SZrExecutionCompileRequest *request,
        EZrExecutionBackendFallback fallback,
        EZrExecutionBackendStatus reason,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic) {
    if (ticket != ZR_NULL) {
        memset(ticket, 0, sizeof(*ticket));
        ticket->magic = ZR_EXECUTION_BACKEND_MAGIC;
        ticket->schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
        ticket->target = request != ZR_NULL ? request->requestedTarget
                                             : ZR_EXECUTION_BACKEND_TARGET_NONE;
        ticket->fallback = fallback;
        ticket->state = ZR_EXECUTION_BACKEND_JOB_FREE;
        if (request != ZR_NULL) ticket->generationKey = request->generationKey;
    }
    if (diagnostic != ZR_NULL) diagnostic->status = reason;
}

static EZrExecutionBackendFallback zr_execution_backend_choose_fallback(
        const SZrExecutionCompileRequest *request) {
    if (request == ZR_NULL) return ZR_EXECUTION_BACKEND_FALLBACK_NONE;
    if ((request->flags & ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE) != 0u) {
        return ZR_EXECUTION_BACKEND_FALLBACK_NONE;
    }
    /* AOT is preferred when explicitly allowed; ExecBC is the always-safe
     * interpreter projection and is the second deterministic choice. */
    if ((request->flags & ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK) != 0u) {
        return ZR_EXECUTION_BACKEND_FALLBACK_AOT;
    }
    if ((request->flags & ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK) != 0u) {
        return ZR_EXECUTION_BACKEND_FALLBACK_EXECBC;
    }
    return ZR_EXECUTION_BACKEND_FALLBACK_NONE;
}

static EZrExecutionBackendStatus zr_execution_backend_status_for_fallback(
        EZrExecutionBackendFallback fallback) {
    switch (fallback) {
        case ZR_EXECUTION_BACKEND_FALLBACK_EXECBC:
            return ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC;
        case ZR_EXECUTION_BACKEND_FALLBACK_AOT:
            return ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT;
        case ZR_EXECUTION_BACKEND_FALLBACK_NONE:
        default:
            return ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
    }
}

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

static TZrBool zr_execution_backend_code_references_registration(
        const SZrExecutionCodeRecord *code, TZrUInt32 registrationSlot) {
    if (code == ZR_NULL || code->state == ZR_EXECUTION_BACKEND_CODE_FREE) {
        return ZR_FALSE;
    }
    return (TZrBool)(code->registrationSlot == registrationSlot);
}

static TZrBool zr_execution_backend_find_candidate_locked(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileRequest *request,
        TZrUInt32 *registrationSlot,
        EZrExecutionBackendStatus *reason) {
    TZrUInt32 index;
    EZrExecutionBackendStatus localReason =
            ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
    TZrBool sawTarget = ZR_FALSE;
    if (service == ZR_NULL || request == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < service->registrationCapacity; ++index) {
        SZrExecutionBackendRegistrationRecord *record =
                &service->registrations[index];
        const SZrExecutionBackendDescriptor *descriptor;
        if (!record->active) continue;
        descriptor = &record->descriptor;
        if (descriptor->backendKind != request->requestedTarget) continue;
        sawTarget = ZR_TRUE;
        if (request->generationKey.backendRegistrationIdentity != 0u &&
            request->generationKey.backendRegistrationIdentity !=
                    record->registrationIdentity) {
            continue;
        }
        if ((descriptor->supportedOperations & request->requiredOperations) !=
                request->requiredOperations) {
            localReason = ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION;
            continue;
        }
        if (descriptor->target.abiVersion != request->contract.abiVersion ||
            descriptor->target.layoutHash != request->contract.layoutHash) {
            localReason = ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH;
            continue;
        }
        if (registrationSlot != ZR_NULL) *registrationSlot = index;
        if (reason != ZR_NULL) *reason = ZR_EXECUTION_BACKEND_STATUS_OK;
        return ZR_TRUE;
    }
    if (!sawTarget) localReason = ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
    if (reason != ZR_NULL) *reason = localReason;
    return ZR_FALSE;
}

static TZrUInt32 zr_execution_backend_find_job_slot_locked(
        SZrExecutionBackendService *service) {
    TZrUInt32 index;
    if (service == ZR_NULL) return UINT32_MAX;
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (service->jobs[index].ticket.state == ZR_EXECUTION_BACKEND_JOB_FREE) {
            return index;
        }
    }
    /* Failed/cancelled tickets retain their diagnostic state until the next
     * allocation, then become reusable. Published jobs remain pinned to their
     * code record and are released only by code reclamation. */
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (zr_execution_backend_job_state_terminal(
                    service->jobs[index].ticket.state)) {
            if (service->jobCount != 0u) --service->jobCount;
            zr_execution_backend_reset_job_locked(&service->jobs[index]);
            return index;
        }
    }
    return UINT32_MAX;
}

static TZrUInt32 zr_execution_backend_find_code_slot_locked(
        SZrExecutionBackendService *service) {
    TZrUInt32 index;
    if (service == ZR_NULL) return UINT32_MAX;
    for (index = 0u; index < service->codeCapacity; ++index) {
        if (service->codes[index].state == ZR_EXECUTION_BACKEND_CODE_FREE) {
            return index;
        }
    }
    return UINT32_MAX;
}

static TZrBool zr_execution_backend_code_identity_exists_locked(
        const SZrExecutionBackendService *service,
        TZrUInt64 codeIdentity) {
    TZrUInt32 index;
    if (service == ZR_NULL || codeIdentity == 0u) return ZR_FALSE;
    for (index = 0u; index < service->codeCapacity; ++index) {
        if (service->codes[index].state != ZR_EXECUTION_BACKEND_CODE_FREE &&
            service->codes[index].info.codeIdentity == codeIdentity) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
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

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CompileAsync(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic) {
    EZrExecutionBackendStatus status;
    EZrExecutionBackendStatus reason = ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
    EZrExecutionBackendFallback fallback;
    TZrUInt32 registrationSlot = UINT32_MAX;
    TZrUInt32 jobSlot = UINT32_MAX;
    TZrUInt64 ticketId;
    TZrUInt32 queryInFlight;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionCompileTicket candidateTicket;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    SZrExecutionGenerationKey candidateKey;
    if (ticket != ZR_NULL) memset(ticket, 0, sizeof(*ticket));
    if (ticket == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, 0u, 0u, 0u, 0u);
    }
    status = zr_execution_backend_validate_request(request, diagnostic);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) return status;
    if (!zr_execution_backend_service_shape_valid(service)) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                ZR_EXECUTION_BACKEND_MAGIC, 0u, 0u, 0u, 0u, 0u);
    }
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&candidateTicket, 0, sizeof(candidateTicket));
    memset(&candidateKey, 0, sizeof(candidateKey));
    zr_execution_backend_lock(service);
    if (service->shuttingDown || service->destroyed) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN, diagnostic,
                0u, 0u, 0u, 0u, 0u, 0u);
    }
    if (!zr_execution_backend_find_candidate_locked(
                service, request, &registrationSlot, &reason)) {
        zr_execution_backend_unlock(service);
        fallback = zr_execution_backend_choose_fallback(request);
        zr_execution_backend_fill_fallback(request, fallback, reason,
                                            ticket, diagnostic);
        return zr_execution_backend_status_for_fallback(fallback);
    }
    candidateKey = request->generationKey;
    candidateKey.backendRegistrationIdentity =
            service->registrations[registrationSlot].registrationIdentity;
    if (zr_execution_backend_generation_invalidated_locked(service, &candidateKey)) {
        zr_execution_backend_unlock(service);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION;
            diagnostic->expectedKey = candidateKey;
            diagnostic->actualKey = candidateKey;
            diagnostic->sourceId = request->sourceId;
            diagnostic->instructionId = request->instructionId;
        }
        return ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION;
    }
    jobSlot = zr_execution_backend_find_job_slot_locked(service);
    if (jobSlot == UINT32_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CAPACITY, diagnostic,
                service->jobCapacity, service->jobCount, 0u, 0u, 0u, 0u);
    }
    ticketId = service->nextTicketId;
    if (ticketId == 0u || ticketId == UINT64_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, (TZrUInt32)ticketId, 0u, 0u, 0u, 0u);
    }
    ++service->nextTicketId;
    descriptor = service->registrations[registrationSlot].descriptor;
    memset(&service->jobs[jobSlot], 0, sizeof(service->jobs[jobSlot]));
    service->jobs[jobSlot].request = *request;
    service->jobs[jobSlot].request.generationKey = candidateKey;
    candidateTicket.magic = ZR_EXECUTION_BACKEND_MAGIC;
    candidateTicket.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    candidateTicket.serviceIdentity = service->serviceIdentity;
    candidateTicket.ticketId = ticketId;
    candidateTicket.generationKey = service->jobs[jobSlot].request.generationKey;
    candidateTicket.target = request->requestedTarget;
    candidateTicket.fallback = ZR_EXECUTION_BACKEND_FALLBACK_NONE;
    candidateTicket.state = ZR_EXECUTION_BACKEND_JOB_QUEUED;
    service->jobs[jobSlot].ticket = candidateTicket;
    service->jobs[jobSlot].lastStatus = ZR_EXECUTION_BACKEND_STATUS_PENDING;
    service->jobs[jobSlot].registrationSlot = registrationSlot;
    service->jobs[jobSlot].codeSlot = UINT32_MAX;
    ++service->jobCount;
    if (service->queryTargetInFlight == UINT32_MAX ||
        service->backendCallbackInFlight == UINT32_MAX ||
        !zr_execution_backend_callback_enter_locked(service)) {
        queryInFlight = service->queryTargetInFlight;
        zr_execution_backend_reset_job_locked(&service->jobs[jobSlot]);
        if (service->jobCount != 0u) --service->jobCount;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, queryInFlight, ticketId, 0u, 0u, 0u);
    }
    ++service->queryTargetInFlight;
    if (ticket != ZR_NULL) *ticket = candidateTicket;
    zr_execution_backend_unlock(service);

    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    status = descriptor.vtable.queryTarget(
            &descriptor.target, request->requiredOperations, descriptor.userData,
            &callbackDiagnostic);
    zr_execution_backend_lock(service);
    if (service->queryTargetInFlight != 0u) --service->queryTargetInFlight;
    zr_execution_backend_callback_leave_locked(service);
    zr_execution_backend_unlock(service);
    if (status != ZR_EXECUTION_BACKEND_STATUS_OK) {
        zr_execution_backend_lock(service);
        {
            SZrExecutionCompileJob *job =
                    zr_execution_backend_find_job_locked(service, &candidateTicket, ZR_NULL);
            if (job != ZR_NULL && job->ticket.state == ZR_EXECUTION_BACKEND_JOB_QUEUED) {
                job->ticket.state = ZR_EXECUTION_BACKEND_JOB_FAILED;
                job->lastStatus = status;
            }
        }
        zr_execution_backend_unlock(service);
        fallback = zr_execution_backend_choose_fallback(request);
        zr_execution_backend_fill_fallback(request, fallback, status,
                                            ticket, diagnostic);
        return zr_execution_backend_status_for_fallback(fallback);
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_PENDING;
        diagnostic->ticketId = ticketId;
    }
    return ZR_EXECUTION_BACKEND_STATUS_PENDING;
}

static EZrExecutionBackendStatus zr_execution_backend_mark_callback_failure(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        EZrExecutionBackendStatus status,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    EZrExecutionBackendJobState actualState = ZR_EXECUTION_BACKEND_JOB_FREE;
    zr_execution_backend_lock(service);
    job = zr_execution_backend_find_job_locked(service, ticket, ZR_NULL);
    if (job != ZR_NULL && job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
        job->ticket.state = status == ZR_EXECUTION_BACKEND_STATUS_CANCELLED
                ? ZR_EXECUTION_BACKEND_JOB_CANCELLED
                : ZR_EXECUTION_BACKEND_JOB_FAILED;
        job->lastStatus = status;
    }
    if (job != ZR_NULL) actualState = job->ticket.state;
    zr_execution_backend_unlock(service);
    return zr_execution_backend_fail(status, diagnostic,
                                     ZR_EXECUTION_BACKEND_JOB_COMPILING,
                                     actualState,
                                     ticket != ZR_NULL ? ticket->ticketId : 0u,
                                     0u, 0u, 0u);
}

static EZrExecutionBackendStatus zr_execution_backend_code_mismatch_status(
        const SZrExecutionCompileRequest *request,
        const SZrExecutionBackendCodeInfo *code) {
    if (request == ZR_NULL || code == ZR_NULL) {
        return ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID;
    }
    if (!zr_execution_backend_generation_equal(&request->generationKey,
                                               &code->generationKey) ||
        code->contract.generation != request->contract.generation) {
        return ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION;
    }
    if (code->contract.schemaVersion != request->contract.schemaVersion ||
        code->contract.abiVersion != request->contract.abiVersion ||
        code->contract.logicalVersion != request->contract.logicalVersion ||
        code->contract.targetToken != request->contract.targetToken ||
        code->contract.signatureHash != request->contract.signatureHash ||
        code->contract.layoutHash != request->contract.layoutHash ||
        code->contract.moduleHash != request->contract.moduleHash ||
        code->contract.requiredCapabilities !=
                request->contract.requiredCapabilities ||
        code->contract.declaredEffects != request->contract.declaredEffects) {
        return ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH;
    }
    if (code->immutableIrHash != request->immutableIrHash ||
        code->compileInputHash != request->compileInputHash) {
        return ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID;
    }
    return ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ProcessNext(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 jobSlot = UINT32_MAX;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionBackendCompileInvocation invocation;
    SZrExecutionBackendCompiledCode code;
    SZrExecutionBackendDiagnostic callbackDiagnostic;
    EZrExecutionBackendStatus status;
    zr_execution_backend_diag_clear(diagnostic);
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&invocation, 0, sizeof(invocation));
    memset(&code, 0, sizeof(code));
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
    for (index = 0u; index < service->jobCapacity; ++index) {
        if (service->jobs[index].ticket.state == ZR_EXECUTION_BACKEND_JOB_QUEUED) {
            jobSlot = index;
            break;
        }
    }
    if (jobSlot == UINT32_MAX) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND, diagnostic,
                ZR_EXECUTION_BACKEND_JOB_QUEUED, ZR_EXECUTION_BACKEND_JOB_FREE,
                0u, 0u, 0u, 0u);
    }
    service->jobs[jobSlot].ticket.state = ZR_EXECUTION_BACKEND_JOB_COMPILING;
    service->jobs[jobSlot].lastStatus = ZR_EXECUTION_BACKEND_STATUS_PENDING;
    invocation.ticket = service->jobs[jobSlot].ticket;
    invocation.request = service->jobs[jobSlot].request;
    descriptor = service->registrations[service->jobs[jobSlot].registrationSlot].descriptor;
    if (!zr_execution_backend_callback_enter_locked(service)) {
        service->jobs[jobSlot].ticket.state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        service->jobs[jobSlot].lastStatus = ZR_EXECUTION_BACKEND_STATUS_OVERFLOW;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_OVERFLOW, diagnostic,
                UINT32_MAX, service->backendCallbackInFlight,
                invocation.ticket.ticketId, 0u, 0u, 0u);
    }
    zr_execution_backend_unlock(service);

    memset(&callbackDiagnostic, 0, sizeof(callbackDiagnostic));
    status = descriptor.vtable.compileAsync(
            &invocation, descriptor.userData, &code, &callbackDiagnostic);
    zr_execution_backend_lock(service);
    zr_execution_backend_callback_leave_locked(service);
    zr_execution_backend_unlock(service);
    if (status == ZR_EXECUTION_BACKEND_STATUS_PENDING) {
        zr_execution_backend_lock(service);
        {
            SZrExecutionCompileJob *job =
                    zr_execution_backend_find_job_locked(service, &invocation.ticket, ZR_NULL);
            if (job != ZR_NULL && job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
                job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_PENDING;
                if (job->cancelRequested) {
                    job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
                    job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
                }
            }
        }
        zr_execution_backend_unlock(service);
        if (diagnostic != ZR_NULL) {
            *diagnostic = callbackDiagnostic;
            diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_PENDING;
            diagnostic->ticketId = invocation.ticket.ticketId;
        }
        return ZR_EXECUTION_BACKEND_STATUS_PENDING;
    }
    if (status == ZR_EXECUTION_BACKEND_STATUS_OK) {
        return ZrCore_ExecutionBackendService_Complete(
                service, &invocation.ticket, &code, diagnostic);
    }
    if (code.codeIdentity != 0u) {
        SZrExecutionBackendDiagnostic disposeDiagnostic;
        memset(&disposeDiagnostic, 0, sizeof(disposeDiagnostic));
        (void)zr_execution_backend_dispose_unpublished(
                 service, &descriptor, &code, &disposeDiagnostic);
    }
    if (diagnostic != ZR_NULL) *diagnostic = callbackDiagnostic;
    if (diagnostic != ZR_NULL && diagnostic->status == ZR_EXECUTION_BACKEND_STATUS_OK) {
        diagnostic->status = status;
    }
    return zr_execution_backend_mark_callback_failure(
            service, &invocation.ticket, status, diagnostic);
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Complete(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        const SZrExecutionBackendCompiledCode *code,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    TZrUInt32 jobSlot = UINT32_MAX;
    TZrUInt32 codeSlot;
    SZrExecutionBackendDescriptor descriptor;
    SZrExecutionBackendCodeInfo disposeCode;
    TZrBool dispose = ZR_FALSE;
    EZrExecutionBackendStatus status;
    EZrExecutionBackendJobState actualState = ZR_EXECUTION_BACKEND_JOB_FREE;
    TZrUInt64 expectedIrHash = 0u;
    TZrUInt64 ticketId = 0u;
    TZrUInt32 codeCapacitySnapshot = 0u;
    TZrUInt32 codeCountSnapshot = 0u;
    SZrExecutionCompileRequest requestSnapshot;
    TZrUInt64 completedCodeIdentity = 0u;
    zr_execution_backend_diag_clear(diagnostic);
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&disposeCode, 0, sizeof(disposeCode));
    memset(&requestSnapshot, 0, sizeof(requestSnapshot));
    if (!zr_execution_backend_service_shape_valid(service) || ticket == ZR_NULL ||
        code == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, ticket != ZR_NULL ? ticket->ticketId : 0u,
                code != ZR_NULL ? code->codeIdentity : 0u, 0u, 0u);
    }
    zr_execution_backend_lock(service);
    job = zr_execution_backend_find_job_locked(service, ticket, &jobSlot);
    if (job == ZR_NULL) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET, diagnostic,
                1u, 0u, ticket->ticketId, code->codeIdentity, 0u, 0u);
    }
    requestSnapshot = job->request;
    ticketId = job->ticket.ticketId;
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = requestSnapshot.sourceId;
        diagnostic->instructionId = requestSnapshot.instructionId;
    }
    if (job->ticket.state != ZR_EXECUTION_BACKEND_JOB_COMPILING ||
        job->cancelRequested || service->shuttingDown) {
        descriptor = service->registrations[job->registrationSlot].descriptor;
        disposeCode = *code;
        dispose = ZR_TRUE;
        if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
            job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        }
        status = job->cancelRequested || service->shuttingDown
                ? ZR_EXECUTION_BACKEND_STATUS_CANCELLED
                : ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE;
        actualState = job->ticket.state;
        zr_execution_backend_unlock(service);
        if (dispose) {
            SZrExecutionBackendDiagnostic disposeDiagnostic;
            memset(&disposeDiagnostic, 0, sizeof(disposeDiagnostic));
            (void)zr_execution_backend_dispose_unpublished(
                    service, &descriptor, &disposeCode, &disposeDiagnostic);
        }
        if (ticket != ZR_NULL) ticket->state = actualState;
        return zr_execution_backend_fail(status, diagnostic,
                                         ZR_EXECUTION_BACKEND_JOB_COMPILING,
                                         actualState, ticket->ticketId,
                                         code->codeIdentity, 0u, 0u);
    }
    if (!zr_execution_backend_code_shape_valid(&requestSnapshot, code)) {
        status = zr_execution_backend_code_mismatch_status(&requestSnapshot, code);
        descriptor = service->registrations[job->registrationSlot].descriptor;
        disposeCode = *code;
        dispose = ZR_TRUE;
        expectedIrHash = requestSnapshot.immutableIrHash;
        job->ticket.state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        job->lastStatus = status;
        zr_execution_backend_unlock(service);
        if (dispose) {
            SZrExecutionBackendDiagnostic disposeDiagnostic;
            memset(&disposeDiagnostic, 0, sizeof(disposeDiagnostic));
            (void)zr_execution_backend_dispose_unpublished(
                    service, &descriptor, &disposeCode, &disposeDiagnostic);
        }
        if (ticket != ZR_NULL) ticket->state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        if (status == ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION) {
            return zr_execution_backend_fail(
                    status, diagnostic, 0u, 0u, ticketId, code->codeIdentity,
                    requestSnapshot.generationKey.generation,
                    code->generationKey.generation);
        }
        if (status == ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH) {
            return zr_execution_backend_fail(
                    status, diagnostic, 0u, 0u, ticketId, code->codeIdentity,
                    requestSnapshot.contract.layoutHash,
                    code->contract.layoutHash);
        }
        return zr_execution_backend_fail(
                status, diagnostic, 1u, 0u, ticketId, code->codeIdentity,
                expectedIrHash, code->immutableIrHash);
    }
    if (zr_execution_backend_code_identity_exists_locked(service,
                                                          code->codeIdentity)) {
        descriptor = service->registrations[job->registrationSlot].descriptor;
        disposeCode = *code;
        dispose = ZR_TRUE;
        job->ticket.state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID;
        zr_execution_backend_unlock(service);
        if (dispose) {
            SZrExecutionBackendDiagnostic disposeDiagnostic;
            memset(&disposeDiagnostic, 0, sizeof(disposeDiagnostic));
            (void)zr_execution_backend_dispose_unpublished(
                    service, &descriptor, &disposeCode, &disposeDiagnostic);
        }
        if (ticket != ZR_NULL) ticket->state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID, diagnostic,
                0u, 0u, ticket->ticketId, code->codeIdentity, 0u,
                code->codeIdentity);
    }
    codeSlot = zr_execution_backend_find_code_slot_locked(service);
    if (codeSlot == UINT32_MAX) {
        descriptor = service->registrations[job->registrationSlot].descriptor;
        disposeCode = *code;
        dispose = ZR_TRUE;
        codeCapacitySnapshot = service->codeCapacity;
        codeCountSnapshot = service->codeCount;
        job->ticket.state = ZR_EXECUTION_BACKEND_JOB_FAILED;
        job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CAPACITY;
        zr_execution_backend_unlock(service);
        if (dispose) {
            SZrExecutionBackendDiagnostic disposeDiagnostic;
            memset(&disposeDiagnostic, 0, sizeof(disposeDiagnostic));
            (void)zr_execution_backend_dispose_unpublished(
                    service, &descriptor, &disposeCode, &disposeDiagnostic);
        }
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_CAPACITY, diagnostic,
                codeCapacitySnapshot, codeCountSnapshot, ticket->ticketId,
                code->codeIdentity, 0u, 0u);
    }
    service->codes[codeSlot].info = *code;
    service->codes[codeSlot].state = ZR_EXECUTION_BACKEND_CODE_READY;
    service->codes[codeSlot].registrationSlot = job->registrationSlot;
    service->codes[codeSlot].jobSlot = jobSlot;
    service->codes[codeSlot].leaseCount = 0u;
    service->codes[codeSlot].dependencyLeaseCount = 0u;
    service->codes[codeSlot].mapsRegistered = ZR_TRUE;
    ++service->codeCount;
    job->result = *code;
    job->resultValid = ZR_TRUE;
    job->codeSlot = codeSlot;
    job->ticket.state = ZR_EXECUTION_BACKEND_JOB_READY;
    job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_OK;
    ticket->state = ZR_EXECUTION_BACKEND_JOB_READY;
    completedCodeIdentity = code->codeIdentity;
    zr_execution_backend_unlock(service);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->ticketId = ticket->ticketId;
        diagnostic->codeIdentity = completedCodeIdentity;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Publish(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    SZrExecutionCodeRecord *code;
    TZrUInt32 jobSlot;
    TZrUInt32 index;
    TZrUInt64 publishedCodeIdentity = 0u;
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
                ZR_EXECUTION_BACKEND_JOB_READY, ZR_EXECUTION_BACKEND_JOB_FREE,
                ticket->ticketId, 0u, 0u, 0u);
    }
    if (service->shuttingDown) {
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN, diagnostic,
                ZR_EXECUTION_BACKEND_JOB_READY, job->ticket.state,
                ticket->ticketId, 0u, 0u, 0u);
    }
    if (job->ticket.state != ZR_EXECUTION_BACKEND_JOB_READY ||
        job->codeSlot >= service->codeCapacity) {
        EZrExecutionBackendStatus status =
                job->ticket.state == ZR_EXECUTION_BACKEND_JOB_CANCELLED
                        ? ZR_EXECUTION_BACKEND_STATUS_CANCELLED
                        : ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED;
        TZrUInt32 actualState = job->ticket.state;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(status, diagnostic,
                                         ZR_EXECUTION_BACKEND_JOB_READY,
                                         actualState, ticket->ticketId, 0u, 0u, 0u);
    }
    code = &service->codes[job->codeSlot];
    if (code->state != ZR_EXECUTION_BACKEND_CODE_READY ||
        !code->mapsRegistered) {
        TZrUInt32 actualState = code->state;
        zr_execution_backend_unlock(service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
                diagnostic, ZR_EXECUTION_BACKEND_CODE_READY, actualState,
                ticket->ticketId, code->info.codeIdentity, 0u, 0u);
    }
    /* A function may have one published code per exact generation namespace.
     * Replacing it retires the old record, but active leases keep it alive. */
    for (index = 0u; index < service->codeCapacity; ++index) {
        SZrExecutionCodeRecord *old = &service->codes[index];
        if (old == code || old->state != ZR_EXECUTION_BACKEND_CODE_PUBLISHED) continue;
        if (old->info.contract.targetToken == code->info.contract.targetToken &&
            zr_execution_backend_generation_equal(&old->info.generationKey,
                                                   &code->info.generationKey)) {
            old->state = ZR_EXECUTION_BACKEND_CODE_RETIRED;
        }
    }
    code->state = ZR_EXECUTION_BACKEND_CODE_PUBLISHED;
    publishedCodeIdentity = code->info.codeIdentity;
    job->ticket.state = ZR_EXECUTION_BACKEND_JOB_PUBLISHED;
    job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_OK;
    ticket->state = ZR_EXECUTION_BACKEND_JOB_PUBLISHED;
    zr_execution_backend_unlock(service);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->ticketId = ticket->ticketId;
        diagnostic->codeIdentity = publishedCodeIdentity;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryTicket(
        const SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCompileTicketView *view,
        SZrExecutionBackendDiagnostic *diagnostic) {
    SZrExecutionCompileJob *job;
    if (view != ZR_NULL) memset(view, 0, sizeof(*view));
    zr_execution_backend_diag_clear(diagnostic);
    if (!zr_execution_backend_service_shape_valid(service) || ticket == ZR_NULL ||
        view == ZR_NULL) {
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT, diagnostic,
                1u, 0u, ticket != ZR_NULL ? ticket->ticketId : 0u, 0u, 0u, 0u);
    }
    zr_execution_backend_lock_const(service);
    job = zr_execution_backend_find_job_locked(
            (SZrExecutionBackendService *)service, ticket, ZR_NULL);
    if (job == ZR_NULL) {
        zr_execution_backend_unlock((SZrExecutionBackendService *)service);
        return zr_execution_backend_fail(
                ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET, diagnostic,
                1u, 0u, ticket->ticketId, 0u, 0u, 0u);
    }
    view->ticketId = job->ticket.ticketId;
    view->state = job->ticket.state;
    view->target = job->ticket.target;
    view->fallback = job->ticket.fallback;
    view->lastStatus = job->lastStatus;
    view->generationKey = job->ticket.generationKey;
    view->codeIdentity = job->resultValid ? job->result.codeIdentity : 0u;
    zr_execution_backend_unlock((SZrExecutionBackendService *)service);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = ZR_EXECUTION_BACKEND_STATUS_OK;
        diagnostic->ticketId = view->ticketId;
        diagnostic->codeIdentity = view->codeIdentity;
    }
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

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
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!zr_execution_backend_service_shape_valid(service) || request == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    zr_execution_backend_lock(service);
    resume = service->resumeInterpreter;
    userData = service->resumeUserData;
    zr_execution_backend_unlock(service);
    if (resume == ZR_NULL) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        return ZR_FALSE;
    }
    if (!resume(request, userData, diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecutionBackend_SetDefaultService(
        SZrExecutionBackendService *service) {
    if (service != ZR_NULL && !zr_execution_backend_service_shape_valid(service)) {
        return ZR_FALSE;
    }
    zr_execution_backend_default_lock_enter();
    zr_execution_backend_default_service = service;
    zr_execution_backend_default_lock_leave();
    return ZR_TRUE;
}

static SZrExecutionBackendService *zr_execution_backend_get_default(void) {
    SZrExecutionBackendService *service;
    zr_execution_backend_default_lock_enter();
    service = zr_execution_backend_default_service;
    zr_execution_backend_default_lock_leave();
    return service;
}

TZrBool ZrCore_ExecutionBackend_Register(
        const SZrExecutionBackendDescriptor *backend) {
    SZrExecutionBackendRegistration registration;
    SZrExecutionBackendDiagnostic diagnostic;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    status = ZrCore_ExecutionBackendService_Register(
            service, backend, &registration, &diagnostic);
    return (TZrBool)(status == ZR_EXECUTION_BACKEND_STATUS_OK);
}

TZrBool ZrCore_ExecutionBackend_CompileAsync(
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket) {
    SZrExecutionBackendDiagnostic diagnostic;
    EZrExecutionBackendStatus status;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    status = ZrCore_ExecutionBackendService_CompileAsync(
            service, request, ticket, &diagnostic);
    return (TZrBool)(status == ZR_EXECUTION_BACKEND_STATUS_PENDING ||
                     status == ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC ||
                     status == ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT);
}

TZrBool ZrCore_ExecutionBackend_InvalidateGeneration(
        const SZrExecutionGenerationKey *key) {
    SZrExecutionBackendDiagnostic diagnostic;
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    return (TZrBool)(ZrCore_ExecutionBackendService_InvalidateGeneration(
                             service, key, &diagnostic) ==
                     ZR_EXECUTION_BACKEND_STATUS_OK);
}

TZrBool ZrCore_ExecutionBackend_ResumeInterpreter(
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecutionBackendService *service = zr_execution_backend_get_default();
    return ZrCore_ExecutionBackendService_ResumeInterpreter(
            service, request, diagnostic);
}
