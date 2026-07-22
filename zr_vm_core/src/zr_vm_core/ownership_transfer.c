#include "zr_vm_core/ownership_transfer.h"

#include "ownership_resource_internal.h"
#include "ownership_transfer_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <sched.h>
#endif

static volatile TZrInt64 g_next_transfer_id = 0;

static TZrInt32 ownership_transfer_atomic_load_acquire(
        const volatile TZrInt32 *value) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrInt32)InterlockedCompareExchange(
            (volatile LONG *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void ownership_transfer_atomic_store_release(
        volatile TZrInt32 *value,
        TZrInt32 next) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

static TZrBool ownership_transfer_atomic_compare_exchange(
        volatile TZrInt32 *value,
        TZrInt32 expected,
        TZrInt32 next) {
#if defined(ZR_PLATFORM_WIN)
    return InterlockedCompareExchange(
                   (volatile LONG *)value,
                   (LONG)next,
                   (LONG)expected) == (LONG)expected;
#else
    return __atomic_compare_exchange_n(
            value,
            &expected,
            next,
            ZR_FALSE,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE);
#endif
}

static TZrUInt64 ownership_transfer_next_id(void) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedIncrement64((volatile LONG64 *)&g_next_transfer_id);
#else
    return (TZrUInt64)__atomic_add_fetch(
            &g_next_transfer_id, 1, __ATOMIC_RELAXED);
#endif
}

static void ownership_transfer_lock(SZrOwnershipTransferEnvelope *envelope) {
    while (!ownership_transfer_atomic_compare_exchange(
            &envelope->transitionLock, 0, 1)) {
#if defined(ZR_PLATFORM_WIN)
        (void)SwitchToThread();
#else
        (void)sched_yield();
#endif
    }
}

static void ownership_transfer_unlock(SZrOwnershipTransferEnvelope *envelope) {
    ownership_transfer_atomic_store_release(&envelope->transitionLock, 0);
}

static TZrBool ownership_transfer_state_matches_domain(
        const SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope) {
    return state != ZR_NULL && envelope != ZR_NULL &&
           ZrCore_GcDomain_IdentityIsCurrent(state, envelope->targetDomain);
}

static TZrBool ownership_transfer_state_matches_source_domain(
        const SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope) {
    return state != ZR_NULL && envelope != ZR_NULL &&
           ZrCore_GcDomain_IdentityIsCurrent(state, envelope->sourceDomain);
}

static void ownership_transfer_diagnostic_set(
        SZrDomainTransferDiagnostic *diagnostic,
        EZrDomainTransferStatus status,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount,
        TZrUInt32 depth) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->status = status;
    diagnostic->objectCount = objectCount;
    diagnostic->byteCount = byteCount;
    diagnostic->depth = depth;
}

static SZrOwnershipTransferEnvelope *ownership_transfer_envelope_new(
        SZrState *state,
        SZrGcDomainIdentity sourceDomain,
        SZrGcDomainIdentity targetDomain) {
    SZrOwnershipTransferEnvelope *envelope;

    envelope = (SZrOwnershipTransferEnvelope *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrOwnershipTransferEnvelope),
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
    if (envelope == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Memory_RawSet(envelope, 0, sizeof(*envelope));
    envelope->ownerGlobal = state->global;
    envelope->sourceDomain = sourceDomain;
    envelope->targetDomain = targetDomain;
    envelope->transferId = ownership_transfer_next_id();
    envelope->generation = targetDomain.generation;
    ZrCore_Value_ResetAsNull(&envelope->payload);
    return envelope;
}

static void ownership_transfer_envelope_delete(
        SZrOwnershipTransferEnvelope *envelope) {
    if (envelope == ZR_NULL) {
        return;
    }
    if (envelope->valueBytes != ZR_NULL && envelope->valueByteCount != 0u) {
        ZrCore_Memory_RawFreeWithType(
                envelope->ownerGlobal,
                envelope->valueBytes,
                envelope->valueByteCount,
                ZR_MEMORY_NATIVE_TYPE_MANAGER);
        envelope->valueBytes = ZR_NULL;
        envelope->valueByteCount = 0u;
    }
    ZrCore_Memory_RawFreeWithType(
            envelope->ownerGlobal,
            envelope,
            sizeof(*envelope),
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

static TZrBool ownership_transfer_is_value_copy_payload(
        const SZrTypeValue *source) {
    if (source == ZR_NULL || source->isGarbageCollectable ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        source->ownershipControl != ZR_NULL ||
        source->ownershipWeakRef != ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_VALUE_IS_TYPE_NULL(source->type) ||
           source->type == ZR_VALUE_TYPE_BOOL ||
           ZR_VALUE_IS_TYPE_INT(source->type) ||
           ZR_VALUE_IS_TYPE_FLOAT(source->type);
}

static TZrBool ownership_transfer_provider_matches_contract(
        const SZrDomainTransferContract *contract) {
    return contract != ZR_NULL && contract->provider != ZR_NULL &&
           contract->providerToken != 0u &&
           contract->providerContractHash != 0u &&
           contract->provider->providerToken == contract->providerToken &&
           contract->provider->providerContractHash ==
                   contract->providerContractHash &&
           contract->provider->prepare != ZR_NULL &&
           contract->provider->commit != ZR_NULL &&
           contract->provider->abort != ZR_NULL;
}

static EZrDomainTransferStatus ownership_transfer_provider_failure_status(
        EZrDomainTransferStatus status,
        EZrDomainTransferStatus fallback) {
    switch (status) {
        case ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED:
            return status;
        default:
            return fallback;
    }
}

static TZrBool ownership_transfer_contract_is_valid(
        const SZrDomainTransferContract *contract) {
    if (contract == ZR_NULL || contract->schemaVersion == 0u ||
        contract->schemaHash == 0u ||
        (contract->flags & ~ZR_DOMAIN_TRANSFER_FLAG_KNOWN_MASK) != 0u ||
        (contract->flags & ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE) == 0u) {
        return ZR_FALSE;
    }
    switch (contract->kind) {
        case ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY:
            return contract->provider == ZR_NULL &&
                   contract->providerToken == 0u &&
                   contract->providerContractHash == 0u;
        case ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE:
            return contract->provider == ZR_NULL &&
                   contract->providerToken == 0u &&
                   contract->providerContractHash == 0u &&
                   contract->quota.maxObjects != 0u &&
                   contract->quota.maxBytes != 0u &&
                   contract->quota.maxDepth != 0u;
        case ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE:
        case ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE:
            return ownership_transfer_provider_matches_contract(contract);
        case ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN:
        default:
            return ZR_FALSE;
    }
}

SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_InternalNew(
        SZrState *state,
        SZrGcDomainIdentity sourceDomain,
        SZrGcDomainIdentity targetDomain) {
    return ownership_transfer_envelope_new(state, sourceDomain, targetDomain);
}

void ZrCore_OwnershipTransfer_InternalDelete(
        SZrOwnershipTransferEnvelope *envelope) {
    ownership_transfer_envelope_delete(envelope);
}

TZrBool ZrCore_OwnershipTransfer_InternalContractIsValid(
        const SZrDomainTransferContract *contract) {
    return ownership_transfer_contract_is_valid(contract);
}

TZrBool ZrCore_OwnershipTransfer_InternalTargetMatches(
        const SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope) {
    return ownership_transfer_state_matches_domain(state, envelope);
}

void ZrCore_OwnershipTransfer_InternalDiagnosticSet(
        SZrDomainTransferDiagnostic *diagnostic,
        EZrDomainTransferStatus status,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount,
        TZrUInt32 depth) {
    ownership_transfer_diagnostic_set(
            diagnostic, status, objectCount, byteCount, depth);
}

void ZrCore_OwnershipTransfer_InternalLock(
        SZrOwnershipTransferEnvelope *envelope) {
    ownership_transfer_lock(envelope);
}

void ZrCore_OwnershipTransfer_InternalUnlock(
        SZrOwnershipTransferEnvelope *envelope) {
    ownership_transfer_unlock(envelope);
}

TZrInt32 ZrCore_OwnershipTransfer_InternalStateLoad(
        const SZrOwnershipTransferEnvelope *envelope) {
    return ownership_transfer_atomic_load_acquire(&envelope->state);
}

void ZrCore_OwnershipTransfer_InternalStateStore(
        SZrOwnershipTransferEnvelope *envelope,
        EZrOwnershipTransferState state) {
    ownership_transfer_atomic_store_release(
            &envelope->state, (TZrInt32)state);
}

SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_PrepareSameDomain(
        SZrState *state,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source) {
    SZrOwnershipTransferEnvelope *envelope;
    SZrGcDomainIdentity sourceDomain;

    if (state == ZR_NULL || state->global == ZR_NULL || source == ZR_NULL ||
        !ZrCore_GcDomain_IdentityIsCurrent(state, targetDomain) ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_NULL;
    }
    sourceDomain = ZrCore_GcDomain_GetIdentity(state);
    envelope = ownership_transfer_envelope_new(
            state, sourceDomain, targetDomain);
    if (envelope == ZR_NULL) {
        return ZR_NULL;
    }
    if (!ZrCore_Ownership_UniqueValue(state, &envelope->payload, source)) {
        ownership_transfer_envelope_delete(envelope);
        return ZR_NULL;
    }
    envelope->hasPayload = ZR_TRUE;
    ownership_transfer_atomic_store_release(
            &envelope->state,
            (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_PREPARED);
    return envelope;
}

SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_PrepareCrossDomain(
        SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        SZrTypeValue *source,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrOwnershipTransferEnvelope *envelope;
    SZrGcDomainIdentity sourceDomain;

    ownership_transfer_diagnostic_set(
            diagnostic, ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    if (sourceState == ZR_NULL || sourceState->global == ZR_NULL ||
        source == ZR_NULL || targetDomain.id == 0u ||
        targetDomain.generation == 0u) {
        return ZR_NULL;
    }
    sourceDomain = ZrCore_GcDomain_GetIdentity(sourceState);
    if (sourceDomain.id == 0u || sourceDomain.generation == 0u ||
        ZrCore_GcDomain_IdentityEquals(sourceDomain, targetDomain)) {
        ownership_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }
    if (contract != ZR_NULL &&
        contract->kind == ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN) {
        ownership_transfer_diagnostic_set(
                diagnostic, ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN, 0u, 0u, 0u);
        return ZR_NULL;
    }
    if (!ownership_transfer_contract_is_valid(contract)) {
        return ZR_NULL;
    }
    if (contract->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE &&
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        ownership_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }

    envelope = ownership_transfer_envelope_new(
            sourceState, sourceDomain, targetDomain);
    if (envelope == ZR_NULL) {
        if (contract->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE) {
            ZrCore_Ownership_ReleaseValue(sourceState, source);
        }
        return ZR_NULL;
    }
    envelope->isCrossDomain = ZR_TRUE;
    envelope->kind = contract->kind;
    envelope->schemaVersion = contract->schemaVersion;
    envelope->schemaHash = contract->schemaHash;
    envelope->providerToken = contract->providerToken;
    envelope->providerContractHash = contract->providerContractHash;
    if (contract->provider != ZR_NULL) {
        envelope->provider = *contract->provider;
        envelope->hasProvider = ZR_TRUE;
    }

    switch (contract->kind) {
        case ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY:
            if (!ownership_transfer_is_value_copy_payload(source)) {
                ownership_transfer_diagnostic_set(
                        diagnostic,
                        source->isGarbageCollectable
                                ? ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE
                                : ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
                        0u,
                        0u,
                        0u);
                ownership_transfer_envelope_delete(envelope);
                return ZR_NULL;
            }
            envelope->payload = *source;
            envelope->hasPayload = ZR_TRUE;
            break;
        case ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE:
            envelope->graph = ZrCore_DomainTransferGraph_Prepare(
                    sourceState,
                    source,
                    &contract->quota,
                    diagnostic,
                    &envelope->serializedObjectCount,
                    &envelope->serializedByteCount);
            if (envelope->graph == ZR_NULL) {
                ownership_transfer_envelope_delete(envelope);
                return ZR_NULL;
            }
            envelope->hasPayload = ZR_TRUE;
            break;
        case ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE:
        case ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE: {
            EZrDomainTransferStatus providerStatus =
                    envelope->provider.prepare(
                            sourceState,
                            targetDomain,
                            source,
                            &envelope->providerPayload,
                            envelope->provider.userData);
            if (providerStatus != ZR_DOMAIN_TRANSFER_STATUS_OK) {
                envelope->provider.abort(
                        &envelope->providerPayload,
                        envelope->provider.userData);
                ZrCore_Memory_RawSet(
                        &envelope->providerPayload,
                        0,
                        sizeof(envelope->providerPayload));
                if (contract->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE &&
                    ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
                    ZrCore_Ownership_ReleaseValue(sourceState, source);
                }
                ownership_transfer_diagnostic_set(
                        diagnostic,
                        ownership_transfer_provider_failure_status(
                                providerStatus,
                                ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED),
                        0u,
                        0u,
                        0u);
                ownership_transfer_envelope_delete(envelope);
                return ZR_NULL;
            }
            if (contract->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE &&
                !ZR_VALUE_IS_TYPE_NULL(source->type)) {
                envelope->provider.abort(
                        &envelope->providerPayload,
                        envelope->provider.userData);
                ZrCore_Ownership_ReleaseValue(sourceState, source);
                ownership_transfer_diagnostic_set(
                        diagnostic,
                        ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED,
                        0u,
                        0u,
                        0u);
                ownership_transfer_envelope_delete(envelope);
                return ZR_NULL;
            }
            envelope->hasPayload = ZR_TRUE;
            break;
        }
        case ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN:
        default:
            ownership_transfer_envelope_delete(envelope);
            ownership_transfer_diagnostic_set(
                    diagnostic,
                    ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN,
                    0u,
                    0u,
                    0u);
            return ZR_NULL;
    }
    ownership_transfer_atomic_store_release(
            &envelope->state,
            (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_PREPARED);
    ownership_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            envelope->serializedObjectCount,
            envelope->serializedByteCount,
            0u);
    return envelope;
}

TZrBool ZrCore_OwnershipTransfer_Publish(
        SZrOwnershipTransferEnvelope *envelope) {
    TZrBool result;

    if (envelope == ZR_NULL) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    result = envelope->hasPayload &&
             ownership_transfer_atomic_compare_exchange(
                     &envelope->state,
                     (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_PREPARED,
                     (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED);
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Claim(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch) {
    TZrBool result = ZR_FALSE;

    if (!ownership_transfer_state_matches_domain(targetState, envelope) ||
        workerId == 0u || claimEpoch == 0u) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    if (envelope->hasPayload &&
        ownership_transfer_atomic_load_acquire(&envelope->state) ==
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED) {
        envelope->claimantWorkerId = workerId;
        envelope->claimEpoch = claimEpoch;
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED);
        if (!result) {
            envelope->claimantWorkerId = 0u;
            envelope->claimEpoch = 0u;
        }
    }
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Commit(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target) {
    TZrBool result = ZR_FALSE;

    if (!ownership_transfer_state_matches_domain(targetState, envelope) ||
        target == ZR_NULL || envelope->isCrossDomain) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    if (envelope->hasPayload &&
        envelope->claimantWorkerId == workerId &&
        envelope->claimEpoch == claimEpoch &&
        ownership_transfer_atomic_load_acquire(&envelope->state) ==
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED &&
        ZrCore_Ownership_UniqueValue(targetState, target, &envelope->payload)) {
        envelope->hasPayload = ZR_FALSE;
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
    }
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_CommitCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    TZrBool result = ZR_FALSE;

    ownership_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
            0u,
            0u,
            0u);
    if (envelope == ZR_NULL || !envelope->isCrossDomain ||
        !ownership_transfer_state_matches_domain(targetState, envelope) ||
        target == ZR_NULL || !ZR_VALUE_IS_TYPE_NULL(target->type)) {
        if (envelope != ZR_NULL && targetState != ZR_NULL &&
            envelope->isCrossDomain &&
            !ownership_transfer_state_matches_domain(targetState, envelope)) {
            ownership_transfer_diagnostic_set(
                    diagnostic,
                    ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
                    0u,
                    0u,
                    0u);
        }
        return ZR_FALSE;
    }
    if (envelope->kind == ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE ||
        envelope->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE) {
        return ZrCore_OwnershipTransfer_InternalCommitProvider(
                envelope,
                targetState,
                workerId,
                claimEpoch,
                target,
                diagnostic);
    }
    if (envelope->kind == ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE) {
        return ZrCore_OwnershipTransfer_InternalCommitGraph(
                envelope,
                targetState,
                workerId,
                claimEpoch,
                target,
                diagnostic);
    }

    ownership_transfer_lock(envelope);
    if (!envelope->hasPayload || envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ownership_transfer_atomic_load_acquire(&envelope->state) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        ownership_transfer_unlock(envelope);
        ownership_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                envelope->serializedObjectCount,
                envelope->serializedByteCount,
                0u);
        return ZR_FALSE;
    }

    switch (envelope->kind) {
        case ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY:
            if (envelope->valueBytes == ZR_NULL) {
                *target = envelope->payload;
                ZrCore_Value_ResetAsNull(&envelope->payload);
                result = ZR_TRUE;
            } else {
                ownership_transfer_diagnostic_set(
                        diagnostic,
                        ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
                        0u,
                        envelope->serializedByteCount,
                        0u);
            }
            break;
        case ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE:
            break;
        case ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE:
        case ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE:
            break;
        case ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN:
        default:
            ownership_transfer_diagnostic_set(
                    diagnostic,
                    ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN,
                    0u,
                    0u,
                    0u);
            break;
    }
    if (result) {
        envelope->hasPayload = ZR_FALSE;
        ownership_transfer_atomic_store_release(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
        ownership_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_OK,
                envelope->serializedObjectCount,
                envelope->serializedByteCount,
                0u);
    }
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Abort(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch) {
    EZrOwnershipTransferState state;
    SZrTypeValue payload;
    TZrBool releasePayload = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (envelope != ZR_NULL && envelope->isCrossDomain) {
        return ZrCore_OwnershipTransfer_AbortCrossDomain(
                envelope,
                targetState,
                workerId,
                claimEpoch,
                ZR_NULL);
    }
    if (!ownership_transfer_state_matches_domain(targetState, envelope)) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(&payload);
    ownership_transfer_lock(envelope);
    state = (EZrOwnershipTransferState)ownership_transfer_atomic_load_acquire(
            &envelope->state);
    if ((state == ZR_OWNERSHIP_TRANSFER_STATE_PREPARED ||
         state == ZR_OWNERSHIP_TRANSFER_STATE_QUEUED) &&
        workerId == 0u && claimEpoch == 0u) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    } else if (state == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED &&
               envelope->claimantWorkerId == workerId &&
               envelope->claimEpoch == claimEpoch) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    }
    if (result && envelope->hasPayload) {
        payload = envelope->payload;
        ZrCore_Value_ResetAsNull(&envelope->payload);
        envelope->hasPayload = ZR_FALSE;
        releasePayload = ZR_TRUE;
    }
    ownership_transfer_unlock(envelope);
    if (releasePayload) {
        ZrCore_Ownership_ReleaseValue(targetState, &payload);
    }
    return result;
}

TZrBool ZrCore_OwnershipTransfer_AbortCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *state,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic) {
    EZrOwnershipTransferState transferState;
    SZrDomainTransferGraph *graphToFree = ZR_NULL;
    TZrByte *valueBytesToFree = ZR_NULL;
    TZrUInt32 valueByteCount = 0u;
    SZrDomainTransferProviderToken providerPayload;
    SZrDomainTransferProvider provider;
    SZrGlobalState *ownerGlobal = ZR_NULL;
    TZrUInt32 objectCount = 0u;
    TZrUInt64 byteCount = 0u;
    TZrBool abortProvider = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    ZrCore_Memory_RawSet(&provider, 0, sizeof(provider));
    ZrCore_Memory_RawSet(&providerPayload, 0, sizeof(providerPayload));
    ownership_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
            0u,
            0u,
            0u);
    if (envelope == ZR_NULL || !envelope->isCrossDomain || state == ZR_NULL) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    objectCount = envelope->serializedObjectCount;
    byteCount = envelope->serializedByteCount;
    transferState = (EZrOwnershipTransferState)
            ownership_transfer_atomic_load_acquire(&envelope->state);
    if (!envelope->commitInProgress &&
        (transferState == ZR_OWNERSHIP_TRANSFER_STATE_PREPARED ||
         transferState == ZR_OWNERSHIP_TRANSFER_STATE_QUEUED) &&
        workerId == 0u && claimEpoch == 0u &&
        ownership_transfer_state_matches_source_domain(state, envelope)) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)transferState,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    } else if (!envelope->commitInProgress &&
               transferState == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED &&
               envelope->claimantWorkerId == workerId &&
               envelope->claimEpoch == claimEpoch &&
               ownership_transfer_state_matches_domain(state, envelope)) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    }
    if (result && envelope->hasPayload) {
        ownerGlobal = envelope->ownerGlobal;
        if (envelope->kind == ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE) {
            graphToFree = envelope->graph;
            envelope->graph = ZR_NULL;
        } else if (envelope->valueBytes != ZR_NULL) {
            valueBytesToFree = envelope->valueBytes;
            valueByteCount = envelope->valueByteCount;
            envelope->valueBytes = ZR_NULL;
            envelope->valueByteCount = 0u;
        } else if (envelope->kind ==
                           ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE ||
                   envelope->kind == ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE) {
            provider = envelope->provider;
            providerPayload = envelope->providerPayload;
            ZrCore_Memory_RawSet(
                    &envelope->providerPayload,
                    0,
                    sizeof(envelope->providerPayload));
            abortProvider = envelope->hasProvider;
        } else {
            ZrCore_Value_ResetAsNull(&envelope->payload);
        }
        envelope->hasPayload = ZR_FALSE;
    }
    ownership_transfer_unlock(envelope);

    if (!result) {
        ownership_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                objectCount,
                byteCount,
                0u);
        return ZR_FALSE;
    }
    ZrCore_DomainTransferGraph_Free(graphToFree);
    if (valueBytesToFree != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                ownerGlobal,
                valueBytesToFree,
                valueByteCount,
                ZR_MEMORY_NATIVE_TYPE_MANAGER);
    }
    if (abortProvider) {
        provider.abort(&providerPayload, provider.userData);
    }
    ownership_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            objectCount,
            byteCount,
            0u);
    return ZR_TRUE;
}
