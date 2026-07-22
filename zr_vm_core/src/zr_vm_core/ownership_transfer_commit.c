#include "ownership_transfer_internal.h"

#include "ownership_resource_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/value.h"

static EZrDomainTransferStatus ownership_transfer_commit_provider_failure_status(
        EZrDomainTransferStatus status) {
    switch (status) {
        case ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED:
        case ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED:
            return status;
        default:
            return ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
    }
}

static TZrBool ownership_transfer_commit_provider_target_is_valid(
        EZrDomainTransferKind kind,
        const SZrTypeValue *target) {
    if (target == ZR_NULL || ZR_VALUE_IS_TYPE_NULL(target->type)) {
        return ZR_FALSE;
    }
    return kind != ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE ||
           ZrCore_OwnershipResource_IsDirectUniqueValue(target);
}

TZrBool ZrCore_OwnershipTransfer_InternalCommitProvider(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrDomainTransferProvider provider;
    SZrDomainTransferProviderToken providerPayload;
    EZrDomainTransferKind kind;
    EZrDomainTransferStatus providerStatus;
    TZrUInt32 objectCount;
    TZrUInt64 byteCount;
    TZrBool result;

    ZrCore_Memory_RawSet(&provider, 0, sizeof(provider));
    ZrCore_Memory_RawSet(&providerPayload, 0, sizeof(providerPayload));
    ZrCore_OwnershipTransfer_InternalLock(envelope);
    objectCount = envelope->serializedObjectCount;
    byteCount = envelope->serializedByteCount;
    if (!envelope->hasPayload || !envelope->hasProvider ||
        envelope->commitInProgress ||
        envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ZrCore_OwnershipTransfer_InternalStateLoad(envelope) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        ZrCore_OwnershipTransfer_InternalUnlock(envelope);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                objectCount,
                byteCount,
                0u);
        return ZR_FALSE;
    }
    envelope->commitInProgress = ZR_TRUE;
    provider = envelope->provider;
    providerPayload = envelope->providerPayload;
    kind = envelope->kind;
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);

    providerStatus = provider.commit(
            targetState, &providerPayload, target, provider.userData);
    if (providerStatus == ZR_DOMAIN_TRANSFER_STATUS_OK &&
        !ownership_transfer_commit_provider_target_is_valid(kind, target)) {
        providerStatus = ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED;
    }
    result = providerStatus == ZR_DOMAIN_TRANSFER_STATUS_OK;
    if (!result && !ZR_VALUE_IS_TYPE_NULL(target->type)) {
        ZrCore_Ownership_ReleaseValue(targetState, target);
    }

    ZrCore_OwnershipTransfer_InternalLock(envelope);
    if (!envelope->commitInProgress || !envelope->hasPayload ||
        envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ZrCore_OwnershipTransfer_InternalStateLoad(envelope) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        envelope->commitInProgress = ZR_FALSE;
        ZrCore_OwnershipTransfer_InternalUnlock(envelope);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                objectCount,
                byteCount,
                0u);
        return ZR_FALSE;
    }
    envelope->providerPayload = providerPayload;
    envelope->commitInProgress = ZR_FALSE;
    if (result) {
        ZrCore_Memory_RawSet(
                &envelope->providerPayload,
                0,
                sizeof(envelope->providerPayload));
        envelope->hasPayload = ZR_FALSE;
        ZrCore_OwnershipTransfer_InternalStateStore(
                envelope, ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
    }
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);

    ZrCore_OwnershipTransfer_InternalDiagnosticSet(
            diagnostic,
            result ? ZR_DOMAIN_TRANSFER_STATUS_OK
                   : ownership_transfer_commit_provider_failure_status(
                             providerStatus),
            objectCount,
            byteCount,
            0u);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_InternalCommitGraph(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrDomainTransferGraph *graph;
    TZrUInt32 objectCount;
    TZrUInt64 byteCount;
    TZrBool result;

    ZrCore_OwnershipTransfer_InternalLock(envelope);
    objectCount = envelope->serializedObjectCount;
    byteCount = envelope->serializedByteCount;
    if (!envelope->hasPayload || envelope->graph == ZR_NULL ||
        envelope->commitInProgress ||
        envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ZrCore_OwnershipTransfer_InternalStateLoad(envelope) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        ZrCore_OwnershipTransfer_InternalUnlock(envelope);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                objectCount,
                byteCount,
                0u);
        return ZR_FALSE;
    }
    envelope->commitInProgress = ZR_TRUE;
    graph = envelope->graph;
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);

    result = ZrCore_DomainTransferGraph_Commit(
            targetState, graph, target, diagnostic);

    ZrCore_OwnershipTransfer_InternalLock(envelope);
    if (!envelope->commitInProgress || !envelope->hasPayload ||
        envelope->graph != graph ||
        envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ZrCore_OwnershipTransfer_InternalStateLoad(envelope) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        envelope->commitInProgress = ZR_FALSE;
        ZrCore_OwnershipTransfer_InternalUnlock(envelope);
        if (!ZR_VALUE_IS_TYPE_NULL(target->type)) {
            ZrCore_Value_ResetAsNull(target);
        }
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                objectCount,
                byteCount,
                0u);
        return ZR_FALSE;
    }
    envelope->commitInProgress = ZR_FALSE;
    if (result) {
        envelope->graph = ZR_NULL;
        envelope->hasPayload = ZR_FALSE;
        ZrCore_OwnershipTransfer_InternalStateStore(
                envelope, ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
    }
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);

    if (result) {
        ZrCore_DomainTransferGraph_Free(graph);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_OK,
                objectCount,
                byteCount,
                0u);
    }
    return result;
}
