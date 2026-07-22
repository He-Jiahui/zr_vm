#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_INTERNAL_H

#include "ownership_transfer_cross_domain_internal.h"
#include "zr_vm_core/global.h"

struct SZrOwnershipTransferEnvelope {
    SZrGlobalState *ownerGlobal;
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;
    EZrDomainTransferKind kind;
    TZrUInt32 schemaVersion;
    TZrUInt64 schemaHash;
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    SZrTypeValue payload;
    SZrDomainTransferProviderToken providerPayload;
    SZrDomainTransferProvider provider;
    SZrDomainTransferGraph *graph;
    TZrByte *valueBytes;
    TZrUInt32 valueByteCount;
    TZrUInt32 serializedObjectCount;
    TZrUInt64 serializedByteCount;
    volatile TZrInt32 state;
    volatile TZrInt32 transitionLock;
    TZrBool hasPayload;
    TZrBool hasProvider;
    TZrBool commitInProgress;
    TZrBool isCrossDomain;
    TZrBool hasSourceGcEdge;
};

SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_InternalNew(
        struct SZrState *state,
        SZrGcDomainIdentity sourceDomain,
        SZrGcDomainIdentity targetDomain);
void ZrCore_OwnershipTransfer_InternalDelete(
        SZrOwnershipTransferEnvelope *envelope);
TZrBool ZrCore_OwnershipTransfer_InternalContractIsValid(
        const SZrDomainTransferContract *contract);
TZrBool ZrCore_OwnershipTransfer_InternalTargetMatches(
        const struct SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope);
void ZrCore_OwnershipTransfer_InternalDiagnosticSet(
        SZrDomainTransferDiagnostic *diagnostic,
        EZrDomainTransferStatus status,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount,
        TZrUInt32 depth);
void ZrCore_OwnershipTransfer_InternalLock(
        SZrOwnershipTransferEnvelope *envelope);
void ZrCore_OwnershipTransfer_InternalUnlock(
        SZrOwnershipTransferEnvelope *envelope);
TZrInt32 ZrCore_OwnershipTransfer_InternalStateLoad(
        const SZrOwnershipTransferEnvelope *envelope);
void ZrCore_OwnershipTransfer_InternalStateStore(
        SZrOwnershipTransferEnvelope *envelope,
        EZrOwnershipTransferState state);
TZrBool ZrCore_OwnershipTransfer_InternalCommitProvider(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);
TZrBool ZrCore_OwnershipTransfer_InternalCommitGraph(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

#endif
