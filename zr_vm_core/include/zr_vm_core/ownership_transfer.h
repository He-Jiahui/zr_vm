#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_H

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/type_layout.h"
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

typedef enum EZrDomainTransferStatus {
    ZR_DOMAIN_TRANSFER_STATUS_OK = 0,
    ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
    ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN,
    ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH,
    ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE,
    ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
    ZR_DOMAIN_TRANSFER_STATUS_OBJECT_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_BYTE_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_DEPTH_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
    ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT
} EZrDomainTransferStatus;

#define ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE ((TZrUInt32)1u << 0u)
#define ZR_DOMAIN_TRANSFER_FLAG_KNOWN_MASK \
    ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE

typedef struct SZrDomainTransferQuota {
    TZrUInt32 maxObjects;
    TZrUInt64 maxBytes;
    TZrUInt32 maxDepth;
} SZrDomainTransferQuota;

typedef struct SZrDomainTransferProviderToken {
    TZrUInt64 words[4];
} SZrDomainTransferProviderToken;

typedef EZrDomainTransferStatus (*FZrDomainTransferProviderPrepare)(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source,
        SZrDomainTransferProviderToken *outToken,
        TZrPtr userData);
typedef EZrDomainTransferStatus (*FZrDomainTransferProviderCommit)(
        struct SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData);
typedef void (*FZrDomainTransferProviderAbort)(
        SZrDomainTransferProviderToken *token,
        TZrPtr userData);

typedef struct SZrDomainTransferProvider {
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    FZrDomainTransferProviderPrepare prepare;
    FZrDomainTransferProviderCommit commit;
    FZrDomainTransferProviderAbort abort;
    TZrPtr userData;
} SZrDomainTransferProvider;

/*
 * PrepareCrossDomain snapshots this descriptor by value. The provider owns
 * userData and must keep it alive until the envelope's single terminal Free
 * call returns, including any abort cleanup performed after terminal state is
 * published. Callbacks may inspect the envelope, but must not recursively
 * commit or abort the same envelope or provider token.
 */

typedef struct SZrDomainTransferContract {
    EZrDomainTransferKind kind;
    TZrUInt32 schemaVersion;
    TZrUInt64 schemaHash;
    TZrUInt32 flags;
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    SZrDomainTransferQuota quota;
    const SZrDomainTransferProvider *provider;
} SZrDomainTransferContract;

typedef struct SZrDomainTransferDiagnostic {
    EZrDomainTransferStatus status;
    TZrUInt32 objectCount;
    TZrUInt64 byteCount;
    TZrUInt32 depth;
} SZrDomainTransferDiagnostic;

typedef struct SZrOwnershipTransferSnapshot {
    EZrOwnershipTransferState state;
    EZrDomainTransferKind kind;
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    TZrUInt32 serializedObjectCount;
    TZrUInt64 serializedByteCount;
    TZrBool hasPayload;
    TZrBool hasSourceGcEdge;
} SZrOwnershipTransferSnapshot;

ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareSameDomain(
        struct SZrState *state,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source);

ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareCrossDomain(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        SZrTypeValue *source,
        SZrDomainTransferDiagnostic *diagnostic);
ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        const SZrTypeLayout *layout,
        const void *sourceStorage,
        SZrDomainTransferDiagnostic *diagnostic);

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
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_CommitCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        const SZrTypeLayout *targetLayout,
        void *targetStorage,
        SZrDomainTransferDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Abort(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch);
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_AbortCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *state,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_OwnershipTransfer_GetSnapshot(
        SZrOwnershipTransferEnvelope *envelope,
        SZrOwnershipTransferSnapshot *outSnapshot);
/*
 * Free is the single terminal disposer. The caller must establish an external
 * quiescent point: no other thread may retain or concurrently access envelope.
 */
ZR_CORE_API void ZrCore_OwnershipTransfer_Free(
        struct SZrState *state,
        SZrOwnershipTransferEnvelope *envelope);

#endif
