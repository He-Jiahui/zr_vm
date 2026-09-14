#include "zr_vm_core/gc_domain_clone.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

struct SZrGcDomainCloneTransaction {
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *sourceState;
    SZrState *targetState;
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;
    TZrUInt64 workerId;
    TZrUInt64 claimEpoch;
    SZrOwnershipTransferSnapshot cachedSnapshot;
    TZrBool hasCachedSnapshot;
};

static void gc_domain_clone_diagnostic_set(
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

static void gc_domain_clone_cache_snapshot(
        SZrGcDomainCloneTransaction *transaction) {
    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL) {
        return;
    }
    ZrCore_OwnershipTransfer_GetSnapshot(
            transaction->envelope, &transaction->cachedSnapshot);
    transaction->hasCachedSnapshot = ZR_TRUE;
}

static TZrBool gc_domain_clone_state_is_terminal(
        EZrOwnershipTransferState state) {
    return state == ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED ||
           state == ZR_OWNERSHIP_TRANSFER_STATE_ABORTED;
}

static void gc_domain_clone_dispose_terminal(
        SZrGcDomainCloneTransaction *transaction) {
    SZrOwnershipTransferSnapshot snapshot;

    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL ||
        transaction->sourceState == ZR_NULL) {
        return;
    }
    ZrCore_OwnershipTransfer_GetSnapshot(
            transaction->envelope, &snapshot);
    if (!gc_domain_clone_state_is_terminal(snapshot.state)) {
        return;
    }
    transaction->cachedSnapshot = snapshot;
    transaction->hasCachedSnapshot = ZR_TRUE;
    /* The wrapper owns the envelope and closes it while the source allocator
     * is still valid.  The cached scalar snapshot keeps post-terminal
     * inspection independent of that allocator. */
    ZrCore_OwnershipTransfer_Free(
            transaction->sourceState, transaction->envelope);
    transaction->envelope = ZR_NULL;
}

static EZrDomainTransferStatus gc_domain_clone_validate_domain_pair(
        SZrState *sourceState,
        SZrState *targetState,
        SZrGcDomainIdentity *outSourceDomain,
        SZrGcDomainIdentity *outTargetDomain) {
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;

    if (sourceState == ZR_NULL || targetState == ZR_NULL ||
        sourceState->global == ZR_NULL || targetState->global == ZR_NULL) {
        return ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT;
    }
    sourceDomain = ZrCore_GcDomain_GetIdentity(sourceState);
    targetDomain = ZrCore_GcDomain_GetIdentity(targetState);
    if (sourceDomain.id == 0u || sourceDomain.generation == 0u ||
        targetDomain.id == 0u || targetDomain.generation == 0u) {
        return ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION;
    }
    if (ZrCore_GcDomain_IdentityEquals(sourceDomain, targetDomain)) {
        return ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH;
    }
    if (outSourceDomain != ZR_NULL) {
        *outSourceDomain = sourceDomain;
    }
    if (outTargetDomain != ZR_NULL) {
        *outTargetDomain = targetDomain;
    }
    return ZR_DOMAIN_TRANSFER_STATUS_OK;
}

SZrGcDomainCloneTransaction *ZrCore_GcDomainClone_Prepare(
        SZrState *sourceState,
        SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrGcDomainCloneTransaction *transaction;
    SZrDomainTransferContract contract;

    gc_domain_clone_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
            0u,
            0u,
            0u);
    if (source == ZR_NULL || quota == ZR_NULL ||
        quota->maxObjects == 0u || quota->maxBytes == 0u ||
        quota->maxDepth == 0u) {
        return ZR_NULL;
    }
    {
        EZrDomainTransferStatus domainStatus =
                gc_domain_clone_validate_domain_pair(
                        sourceState,
                        targetState,
                        ZR_NULL,
                        ZR_NULL);
        if (domainStatus != ZR_DOMAIN_TRANSFER_STATUS_OK) {
            gc_domain_clone_diagnostic_set(
                    diagnostic,
                    domainStatus,
                    0u,
                    0u,
                    0u);
            return ZR_NULL;
        }
    }

    transaction = (SZrGcDomainCloneTransaction *)calloc(1u, sizeof(*transaction));
    if (transaction == ZR_NULL) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }
    transaction->sourceState = sourceState;
    transaction->targetState = targetState;
    transaction->sourceDomain = ZrCore_GcDomain_GetIdentity(sourceState);
    transaction->targetDomain = ZrCore_GcDomain_GetIdentity(targetState);
    memset(&contract, 0, sizeof(contract));
    contract.kind = ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE;
    contract.schemaVersion = ZR_GC_DOMAIN_CLONE_SCHEMA_VERSION;
    contract.schemaHash = ZR_GC_DOMAIN_CLONE_SCHEMA_HASH;
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    contract.quota = *quota;
    transaction->envelope = ZrCore_OwnershipTransfer_PrepareCrossDomain(
            sourceState,
            transaction->targetDomain,
            &contract,
            (SZrTypeValue *)source,
            diagnostic);
    if (transaction->envelope == ZR_NULL) {
        free(transaction);
        return ZR_NULL;
    }
    gc_domain_clone_cache_snapshot(transaction);
    return transaction;
}

TZrBool ZrCore_GcDomainClone_Publish(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic) {
    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    if (!ZrCore_OwnershipTransfer_Publish(transaction->envelope)) {
        gc_domain_clone_cache_snapshot(transaction);
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                transaction->cachedSnapshot.serializedObjectCount,
                transaction->cachedSnapshot.serializedByteCount,
                0u);
        return ZR_FALSE;
    }
    gc_domain_clone_cache_snapshot(transaction);
    gc_domain_clone_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            transaction->cachedSnapshot.serializedObjectCount,
            transaction->cachedSnapshot.serializedByteCount,
            0u);
    return ZR_TRUE;
}

TZrBool ZrCore_GcDomainClone_Claim(
        SZrGcDomainCloneTransaction *transaction,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic) {
    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL ||
        workerId == 0u || claimEpoch == 0u) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_IdentityIsCurrent(
                transaction->targetState, transaction->targetDomain)) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    if (!ZrCore_OwnershipTransfer_Claim(
                transaction->envelope,
                transaction->targetState,
                workerId,
                claimEpoch)) {
        gc_domain_clone_cache_snapshot(transaction);
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                transaction->cachedSnapshot.serializedObjectCount,
                transaction->cachedSnapshot.serializedByteCount,
                0u);
        return ZR_FALSE;
    }
    transaction->workerId = workerId;
    transaction->claimEpoch = claimEpoch;
    gc_domain_clone_cache_snapshot(transaction);
    gc_domain_clone_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            transaction->cachedSnapshot.serializedObjectCount,
            transaction->cachedSnapshot.serializedByteCount,
            0u);
    return ZR_TRUE;
}

TZrBool ZrCore_GcDomainClone_Commit(
        SZrGcDomainCloneTransaction *transaction,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    TZrBool result;

    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL ||
        target == ZR_NULL || !ZR_VALUE_IS_TYPE_NULL(target->type)) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_IdentityIsCurrent(
                transaction->targetState, transaction->targetDomain)) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    result = ZrCore_OwnershipTransfer_CommitCrossDomain(
            transaction->envelope,
            transaction->targetState,
            transaction->workerId,
            transaction->claimEpoch,
            target,
            diagnostic);
    gc_domain_clone_cache_snapshot(transaction);
    if (result) {
        gc_domain_clone_dispose_terminal(transaction);
    }
    return result;
}

TZrBool ZrCore_GcDomainClone_Abort(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic) {
    EZrOwnershipTransferState state;
    TZrBool result;
    SZrState *authorityState;
    TZrUInt64 workerId;
    TZrUInt64 claimEpoch;

    if (transaction == ZR_NULL || transaction->envelope == ZR_NULL) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomainClone_GetSnapshot(
                transaction, &transaction->cachedSnapshot)) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    transaction->hasCachedSnapshot = ZR_TRUE;
    state = transaction->cachedSnapshot.state;
    if (gc_domain_clone_state_is_terminal(state)) {
        gc_domain_clone_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                transaction->cachedSnapshot.serializedObjectCount,
                transaction->cachedSnapshot.serializedByteCount,
                0u);
        return ZR_FALSE;
    }
    authorityState = transaction->sourceState;
    workerId = 0u;
    claimEpoch = 0u;
    if (state == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        /* The source domain is an explicit cancellation authority.  This is
         * also the safe path after the target domain has been destroyed. */
        workerId = transaction->workerId;
        claimEpoch = transaction->claimEpoch;
    }
    result = ZrCore_OwnershipTransfer_AbortCrossDomain(
            transaction->envelope,
            authorityState,
            workerId,
            claimEpoch,
            diagnostic);
    gc_domain_clone_cache_snapshot(transaction);
    if (result) {
        gc_domain_clone_dispose_terminal(transaction);
    }
    return result;
}

void ZrCore_GcDomainClone_Free(
        SZrGcDomainCloneTransaction *transaction) {
    SZrOwnershipTransferSnapshot snapshot;

    if (transaction == ZR_NULL) {
        return;
    }
    if (transaction->envelope != ZR_NULL) {
        ZrCore_OwnershipTransfer_GetSnapshot(
                transaction->envelope, &snapshot);
        if (!gc_domain_clone_state_is_terminal(snapshot.state)) {
            if (!ZrCore_GcDomainClone_Abort(transaction, ZR_NULL)) {
                /* A concurrent commit still owns the envelope.  Do not drop
                 * the wrapper and strand that in-flight transaction. */
                return;
            }
            if (transaction->envelope != ZR_NULL) {
                ZrCore_OwnershipTransfer_GetSnapshot(
                        transaction->envelope, &snapshot);
            } else if (transaction->hasCachedSnapshot) {
                snapshot = transaction->cachedSnapshot;
            }
        }
        /* The participating source state must remain alive through this
         * terminal disposer because the envelope's allocator belongs to it. */
        if (gc_domain_clone_state_is_terminal(snapshot.state) &&
            transaction->sourceState != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(
                    transaction->sourceState, transaction->envelope);
        }
        transaction->envelope = ZR_NULL;
    }
    free(transaction);
}

TZrBool ZrCore_GcDomainClone_GetSnapshot(
        const SZrGcDomainCloneTransaction *transaction,
        SZrOwnershipTransferSnapshot *outSnapshot) {
    if (outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    if (transaction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (transaction->envelope != ZR_NULL) {
        ZrCore_OwnershipTransfer_GetSnapshot(
                transaction->envelope,
                outSnapshot);
        return ZR_TRUE;
    }
    if (transaction->hasCachedSnapshot) {
        *outSnapshot = transaction->cachedSnapshot;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_GcDomainClone_Execute(
        SZrState *sourceState,
        SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrGcDomainCloneTransaction *transaction;
    TZrBool result;

    transaction = ZrCore_GcDomainClone_Prepare(
            sourceState,
            targetState,
            source,
            quota,
            diagnostic);
    if (transaction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomainClone_Publish(transaction, diagnostic) ||
        !ZrCore_GcDomainClone_Claim(
                transaction, workerId, claimEpoch, diagnostic)) {
        (void)ZrCore_GcDomainClone_Abort(transaction, ZR_NULL);
        ZrCore_GcDomainClone_Free(transaction);
        return ZR_FALSE;
    }
    result = ZrCore_GcDomainClone_Commit(transaction, target, diagnostic);
    if (!result) {
        (void)ZrCore_GcDomainClone_Abort(transaction, ZR_NULL);
    }
    ZrCore_GcDomainClone_Free(transaction);
    return result;
}
