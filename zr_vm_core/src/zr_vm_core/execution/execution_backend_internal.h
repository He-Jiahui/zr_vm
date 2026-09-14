#ifndef ZR_VM_CORE_EXECUTION_BACKEND_INTERNAL_H
#define ZR_VM_CORE_EXECUTION_BACKEND_INTERNAL_H

#include "zr_vm_core/execution_backend.h"

#include <limits.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ZR_EXECUTION_BACKEND_INTERNAL_UNUSED __attribute__((unused))
#else
#define ZR_EXECUTION_BACKEND_INTERNAL_UNUSED
#endif

/* Internal helpers are shared by the service and code-lifetime translation
 * units.  They intentionally operate on caller-owned fixed-capacity arrays. */

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_diag_clear(
        SZrExecutionBackendDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED EZrExecutionBackendStatus zr_execution_backend_fail(
        EZrExecutionBackendStatus status,
        SZrExecutionBackendDiagnostic *diagnostic,
        TZrUInt32 expectedState,
        TZrUInt32 actualState,
        TZrUInt64 ticketId,
        TZrUInt64 codeIdentity,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->expectedState = expectedState;
        diagnostic->actualState = actualState;
        diagnostic->ticketId = ticketId;
        diagnostic->codeIdentity = codeIdentity;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_lock(SZrExecutionBackendService *service) {
#if defined(_MSC_VER)
    while (_InterlockedExchange((volatile long *)&service->lock, 1L) != 0L) {
    }
#else
    while (__sync_lock_test_and_set(&service->lock, 1u) != 0u) {
    }
#endif
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_unlock(SZrExecutionBackendService *service) {
#if defined(_MSC_VER)
    _InterlockedExchange((volatile long *)&service->lock, 0L);
#else
    __sync_lock_release(&service->lock);
#endif
}

/* Callback entry/leave is separate from the service lock.  Callers enter
 * while holding the lock, release it before invoking user code, and leave
 * after the callback returns.  The counter is deliberately bounded so a
 * corrupt/overflowed service fails closed instead of wrapping to zero. */
static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool
zr_execution_backend_callback_enter_locked(SZrExecutionBackendService *service) {
    if (service == ZR_NULL || service->backendCallbackInFlight == UINT32_MAX) {
        return ZR_FALSE;
    }
    ++service->backendCallbackInFlight;
    return ZR_TRUE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void
zr_execution_backend_callback_leave_locked(SZrExecutionBackendService *service) {
    if (service != ZR_NULL && service->backendCallbackInFlight != 0u) {
        --service->backendCallbackInFlight;
    }
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void
zr_execution_backend_callback_leave(SZrExecutionBackendService *service) {
    if (service == ZR_NULL) return;
    zr_execution_backend_lock(service);
    zr_execution_backend_callback_leave_locked(service);
    zr_execution_backend_unlock(service);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_lock_const(
        const SZrExecutionBackendService *service) {
    zr_execution_backend_lock((SZrExecutionBackendService *)service);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_service_shape_valid(
        const SZrExecutionBackendService *service) {
    return (TZrBool)(service != ZR_NULL &&
                     service->magic == ZR_EXECUTION_BACKEND_MAGIC &&
                     service->schemaVersion == ZR_EXECUTION_BACKEND_SCHEMA_VERSION &&
                     service->serviceIdentity != 0u &&
                     service->registrations != ZR_NULL &&
                     service->registrationCapacity != 0u &&
                     service->jobs != ZR_NULL && service->jobCapacity != 0u &&
                     service->codes != ZR_NULL && service->codeCapacity != 0u);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_generation_equal(
        const SZrExecutionGenerationKey *left,
        const SZrExecutionGenerationKey *right) {
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL &&
                     left->domainIdentity == right->domainIdentity &&
                     left->moduleIdentity == right->moduleIdentity &&
                     left->generation == right->generation &&
                     left->backendRegistrationIdentity ==
                             right->backendRegistrationIdentity);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool
zr_execution_backend_generation_invalidated_locked(
        const SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key) {
    TZrUInt32 index;
    if (service == ZR_NULL || key == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < service->invalidatedKeyCount &&
                       index < ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY; ++index) {
        if (zr_execution_backend_generation_equal(
                    &service->invalidatedKeys[index], key)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_key_valid(
        const SZrExecutionGenerationKey *key,
        TZrBool requireBackendIdentity) {
    return (TZrBool)(key != ZR_NULL && key->domainIdentity != 0u &&
                     key->moduleIdentity != 0u && key->generation != 0u &&
                     (!requireBackendIdentity ||
                      key->backendRegistrationIdentity != 0u));
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_target_kind_valid(
        EZrExecutionBackendTargetKind kind) {
    return (TZrBool)(kind > ZR_EXECUTION_BACKEND_TARGET_NONE &&
                     kind < ZR_EXECUTION_BACKEND_TARGET_COUNT);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_map_kind_valid(EZrExecutionBackendMapKind kind) {
    return (TZrBool)(kind >= ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS &&
                     kind < ZR_EXECUTION_BACKEND_MAP_KIND_COUNT);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_map_flag_for_kind(
        EZrExecutionBackendMapKind kind, TZrUInt32 *flag) {
    if (flag == ZR_NULL || !zr_execution_backend_map_kind_valid(kind)) {
        return ZR_FALSE;
    }
    switch (kind) {
        case ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS: *flag = ZR_EXECUTION_BACKEND_MAP_ROOTS; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_EH: *flag = ZR_EXECUTION_BACKEND_MAP_EH; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEBUG: *flag = ZR_EXECUTION_BACKEND_MAP_DEBUG; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT: *flag = ZR_EXECUTION_BACKEND_MAP_DEOPT; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_IMPORTS: *flag = ZR_EXECUTION_BACKEND_MAP_IMPORTS; break;
        default: return ZR_FALSE;
    }
    return ZR_TRUE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_descriptor_shape_valid(
        const SZrExecutionBackendDescriptor *descriptor) {
    if (descriptor == ZR_NULL ||
        descriptor->magic != ZR_EXECUTION_BACKEND_MAGIC ||
        descriptor->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        !zr_execution_backend_target_kind_valid(descriptor->backendKind) ||
        descriptor->backendKind != descriptor->target.kind ||
        (descriptor->flags & ~ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK) != 0u ||
        descriptor->target.schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        descriptor->target.abiVersion == 0u ||
        descriptor->target.targetTripleHash == 0u ||
        descriptor->target.layoutHash == 0u ||
        descriptor->target.capabilityHash == 0u ||
        descriptor->supportedOperations == 0u ||
        (descriptor->supportedOperations &
         ~ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK) != 0u ||
        descriptor->vtable.queryTarget == ZR_NULL ||
        descriptor->vtable.compileAsync == ZR_NULL ||
        descriptor->vtable.unregisterMaps == ZR_NULL ||
        descriptor->vtable.retire == ZR_NULL ||
        descriptor->vtable.destroy == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_request_shape_valid(
        const SZrExecutionCompileRequest *request) {
    if (request == ZR_NULL || request->magic != ZR_EXECUTION_BACKEND_MAGIC ||
        request->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        (request->flags & ~ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK) != 0u ||
        (request->flags & ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT) == 0u ||
        !zr_execution_backend_target_kind_valid(request->requestedTarget) ||
        !zr_execution_backend_key_valid(&request->generationKey, ZR_FALSE) ||
        request->contract.schemaVersion != ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ||
        request->contract.abiVersion == 0u ||
        request->contract.logicalVersion == 0u ||
        request->contract.generation != request->generationKey.generation ||
        request->contract.targetToken == 0u ||
        request->contract.signatureHash == 0u ||
        request->contract.layoutHash == 0u ||
        request->contract.moduleHash != request->generationKey.moduleIdentity ||
        (request->contract.requiredCapabilities &
         ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u ||
        (request->contract.declaredEffects &
         ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u ||
        request->immutableIrHash == 0u || request->compileInputHash == 0u ||
        request->sourceId == 0u || request->instructionId == 0u ||
        request->requiredOperations == 0u ||
        (request->requiredOperations & ~ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK) != 0u ||
        (request->requiredMapFlags & ~ZR_EXECUTION_BACKEND_MAP_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrUInt64 zr_execution_backend_code_map_hash(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind kind) {
    if (code == ZR_NULL) return 0u;
    switch (kind) {
        case ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS: return code->rootMapHash;
        case ZR_EXECUTION_BACKEND_MAP_KIND_EH: return code->ehMapHash;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEBUG: return code->debugMapHash;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT: return code->deoptMapHash;
        case ZR_EXECUTION_BACKEND_MAP_KIND_IMPORTS: return code->runtimeImportsHash;
        default: return 0u;
    }
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_code_shape_valid(
        const SZrExecutionCompileRequest *request,
        const SZrExecutionBackendCodeInfo *code) {
    TZrUInt32 required;
    EZrExecutionBackendMapKind kind;
    if (request == ZR_NULL || code == ZR_NULL ||
        code->magic != ZR_EXECUTION_BACKEND_MAGIC ||
        code->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        !zr_execution_backend_generation_equal(&request->generationKey,
                                                &code->generationKey) ||
        code->contract.schemaVersion != request->contract.schemaVersion ||
        code->contract.abiVersion != request->contract.abiVersion ||
        code->contract.logicalVersion != request->contract.logicalVersion ||
        code->contract.generation != request->contract.generation ||
        code->contract.targetToken != request->contract.targetToken ||
        code->contract.signatureHash != request->contract.signatureHash ||
        code->contract.layoutHash != request->contract.layoutHash ||
        code->contract.moduleHash != request->contract.moduleHash ||
        code->contract.requiredCapabilities != request->contract.requiredCapabilities ||
        code->contract.declaredEffects != request->contract.declaredEffects ||
        code->immutableIrHash != request->immutableIrHash ||
        code->compileInputHash != request->compileInputHash ||
        code->codeIdentity == 0u || code->codeSize == 0u ||
        (code->mapRegistrationFlags & ~ZR_EXECUTION_BACKEND_MAP_KNOWN_MASK) != 0u ||
        (code->mapRegistrationFlags & request->requiredMapFlags) !=
                request->requiredMapFlags) {
        return ZR_FALSE;
    }
    for (kind = ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS;
         kind < ZR_EXECUTION_BACKEND_MAP_KIND_COUNT; ++kind) {
        if (!zr_execution_backend_map_flag_for_kind(kind, &required)) {
            return ZR_FALSE;
        }
        if ((code->mapRegistrationFlags & required) != 0u &&
            zr_execution_backend_code_map_hash(code, kind) == 0u) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED SZrExecutionBackendRegistrationRecord *
zr_execution_backend_find_registration_locked(
        SZrExecutionBackendService *service,
        TZrUInt64 registrationIdentity,
        TZrUInt32 *slotIndex) {
    TZrUInt32 index;
    if (service == ZR_NULL || registrationIdentity == 0u) return ZR_NULL;
    for (index = 0u; index < service->registrationCapacity; ++index) {
        SZrExecutionBackendRegistrationRecord *record =
                &service->registrations[index];
        if (record->active && record->registrationIdentity == registrationIdentity) {
            if (slotIndex != ZR_NULL) *slotIndex = index;
            return record;
        }
    }
    return ZR_NULL;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED SZrExecutionCompileJob *zr_execution_backend_find_job_locked(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        TZrUInt32 *slotIndex) {
    TZrUInt32 index;
    if (service == ZR_NULL || ticket == ZR_NULL ||
        ticket->magic != ZR_EXECUTION_BACKEND_MAGIC ||
        ticket->schemaVersion != ZR_EXECUTION_BACKEND_SCHEMA_VERSION ||
        ticket->serviceIdentity != service->serviceIdentity ||
        ticket->ticketId == 0u) return ZR_NULL;
    for (index = 0u; index < service->jobCapacity; ++index) {
        SZrExecutionCompileJob *job = &service->jobs[index];
        if (job->ticket.ticketId == ticket->ticketId &&
            job->ticket.magic == ZR_EXECUTION_BACKEND_MAGIC &&
            job->ticket.serviceIdentity == service->serviceIdentity &&
            zr_execution_backend_generation_equal(&job->ticket.generationKey,
                                                   &ticket->generationKey)) {
            if (slotIndex != ZR_NULL) *slotIndex = index;
            return job;
        }
    }
    return ZR_NULL;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED TZrBool zr_execution_backend_job_state_terminal(
        EZrExecutionBackendJobState state) {
    return (TZrBool)(state == ZR_EXECUTION_BACKEND_JOB_FAILED ||
                     state == ZR_EXECUTION_BACKEND_JOB_CANCELLED);
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_reset_job_locked(
        SZrExecutionCompileJob *job) {
    if (job != ZR_NULL) memset(job, 0, sizeof(*job));
}

/* Mark matching code as retired. The caller must hold service->lock. */
static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_mark_codes_retired_locked(
        SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key,
        TZrUInt32 *outMarked) {
    TZrUInt32 index;
    TZrUInt32 marked = 0u;
    if (service == ZR_NULL || key == ZR_NULL) {
        if (outMarked != ZR_NULL) *outMarked = 0u;
        return;
    }
    for (index = 0u; index < service->codeCapacity; ++index) {
        SZrExecutionCodeRecord *record = &service->codes[index];
        if ((record->state == ZR_EXECUTION_BACKEND_CODE_READY ||
             record->state == ZR_EXECUTION_BACKEND_CODE_PUBLISHED) &&
            zr_execution_backend_generation_equal(&record->info.generationKey,
                                                   key)) {
            record->state = ZR_EXECUTION_BACKEND_CODE_RETIRED;
            ++marked;
        }
    }
    if (outMarked != ZR_NULL) *outMarked = marked;
}

static ZR_EXECUTION_BACKEND_INTERNAL_UNUSED void zr_execution_backend_mark_jobs_cancelled_locked(
        SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key,
        TZrUInt32 *outCompiling) {
    TZrUInt32 index;
    TZrUInt32 compiling = 0u;
    if (service == ZR_NULL || key == ZR_NULL) {
        if (outCompiling != ZR_NULL) *outCompiling = 0u;
        return;
    }
    for (index = 0u; index < service->jobCapacity; ++index) {
        SZrExecutionCompileJob *job = &service->jobs[index];
        if (!zr_execution_backend_generation_equal(&job->request.generationKey,
                                                    key)) continue;
        if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_QUEUED) {
            job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
            job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        } else if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_COMPILING) {
            job->cancelRequested = ZR_TRUE;
            ++compiling;
        } else if (job->ticket.state == ZR_EXECUTION_BACKEND_JOB_READY) {
            job->ticket.state = ZR_EXECUTION_BACKEND_JOB_CANCELLED;
            job->lastStatus = ZR_EXECUTION_BACKEND_STATUS_CANCELLED;
        }
    }
    if (outCompiling != ZR_NULL) *outCompiling = compiling;
}

/* Implemented by execution_code_handle.c; callbacks are always invoked after
 * the service lock has been released. */
EZrExecutionBackendStatus zr_execution_backend_dispose_unpublished(
        SZrExecutionBackendService *service,
        const SZrExecutionBackendDescriptor *descriptor,
        const SZrExecutionBackendCodeInfo *code,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus zr_execution_backend_collect_code_slot(
        SZrExecutionBackendService *service,
        TZrUInt32 slotIndex,
        SZrExecutionBackendDiagnostic *diagnostic);

#endif /* ZR_VM_CORE_EXECUTION_BACKEND_INTERNAL_H */
