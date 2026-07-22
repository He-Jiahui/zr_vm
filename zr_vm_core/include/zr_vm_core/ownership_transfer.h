#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_H

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/value.h"

struct SZrState;

typedef struct SZrOwnershipTransferEnvelope SZrOwnershipTransferEnvelope;

typedef enum EZrOwnershipTransferState {
    ZR_OWNERSHIP_TRANSFER_STATE_PREPARED = 0,
    ZR_OWNERSHIP_TRANSFER_STATE_QUEUED,
    ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
    ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED,
    ZR_OWNERSHIP_TRANSFER_STATE_ABORTED
} EZrOwnershipTransferState;

typedef struct SZrOwnershipTransferSnapshot {
    EZrOwnershipTransferState state;
    SZrGcDomainIdentity targetDomain;
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    TZrBool hasPayload;
} SZrOwnershipTransferSnapshot;

ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareSameDomain(
        struct SZrState *state,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Publish(
        SZrOwnershipTransferEnvelope *envelope);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Claim(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Commit(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Abort(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch);
ZR_CORE_API void ZrCore_OwnershipTransfer_GetSnapshot(
        SZrOwnershipTransferEnvelope *envelope,
        SZrOwnershipTransferSnapshot *outSnapshot);
ZR_CORE_API void ZrCore_OwnershipTransfer_Free(
        struct SZrState *state,
        SZrOwnershipTransferEnvelope *envelope);

#endif
