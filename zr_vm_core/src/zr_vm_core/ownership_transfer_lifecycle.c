#include "ownership_transfer_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
/* 供域克隆包装器缓存终结前的标量状态；持锁保证复制的字段来自同一次 envelope 状态。 */
void ZrCore_OwnershipTransfer_GetSnapshot(
        SZrOwnershipTransferEnvelope *envelope,
        SZrOwnershipTransferSnapshot *outSnapshot) {
    if (outSnapshot == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(outSnapshot, 0, sizeof(*outSnapshot));
    if (envelope == ZR_NULL) {
        return;
    }
    ZrCore_OwnershipTransfer_InternalLock(envelope);
    outSnapshot->state = (EZrOwnershipTransferState)
            ZrCore_OwnershipTransfer_InternalStateLoad(envelope);
    outSnapshot->kind = envelope->kind;
    outSnapshot->sourceDomain = envelope->sourceDomain;
    outSnapshot->targetDomain = envelope->targetDomain;
    outSnapshot->transferId = envelope->transferId;
    outSnapshot->generation = envelope->generation;
    outSnapshot->claimantWorkerId = envelope->claimantWorkerId;
    outSnapshot->claimEpoch = envelope->claimEpoch;
    outSnapshot->serializedObjectCount = envelope->serializedObjectCount;
    outSnapshot->serializedByteCount = envelope->serializedByteCount;
    outSnapshot->hasPayload = envelope->hasPayload;
    outSnapshot->hasSourceGcEdge = envelope->hasSourceGcEdge;
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);
}
/* 仅由有源/目标域权限且已确保外部静止的调用方终结 envelope。 */
void ZrCore_OwnershipTransfer_Free(
        SZrState *state,
        SZrOwnershipTransferEnvelope *envelope) {
    EZrOwnershipTransferState transferState;
    TZrUInt64 workerId = 0u;
    TZrUInt64 claimEpoch = 0u;
    TZrBool sourceMatches;
    TZrBool targetMatches;

    if (state == ZR_NULL || envelope == ZR_NULL) {
        return;
    }
    /* 跨域源方也有取消权限；同域信封必须由目标域状态完成清理。 */
    sourceMatches = ZrCore_GcDomain_IdentityIsCurrent(
            state, envelope->sourceDomain);
    targetMatches = ZrCore_OwnershipTransfer_InternalTargetMatches(
            state, envelope);
    if ((!envelope->isCrossDomain && !targetMatches) ||
        (envelope->isCrossDomain && !sourceMatches && !targetMatches)) {
        return;
    }
    /* CLAIMED 状态撤销需精确 worker/epoch，以免错撤另一轮认领。 */
    ZrCore_OwnershipTransfer_InternalLock(envelope);
    transferState = (EZrOwnershipTransferState)
            ZrCore_OwnershipTransfer_InternalStateLoad(envelope);
    if (transferState == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        workerId = envelope->claimantWorkerId;
        claimEpoch = envelope->claimEpoch;
    }
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);
    /* 流转中的信封先经过状态机撤销；撤销失败不允许直接释放。 */
    if (transferState == ZR_OWNERSHIP_TRANSFER_STATE_PREPARED ||
        transferState == ZR_OWNERSHIP_TRANSFER_STATE_QUEUED ||
        transferState == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        TZrBool aborted = envelope->isCrossDomain
                                   ? ZrCore_OwnershipTransfer_AbortCrossDomain(
                                             envelope,
                                             state,
                                             workerId,
                                             claimEpoch,
                                             ZR_NULL)
                                   : ZrCore_OwnershipTransfer_Abort(
                                             envelope,
                                             state,
                                             workerId,
                                             claimEpoch);
        if (!aborted) {
            return;
        }
    }
    /* 仅在 COMMITTED/ABORTED 终态归还源侧分配与关联资源。 */
    ZrCore_OwnershipTransfer_InternalLock(envelope);
    transferState = (EZrOwnershipTransferState)
            ZrCore_OwnershipTransfer_InternalStateLoad(envelope);
    ZrCore_OwnershipTransfer_InternalUnlock(envelope);
    if (transferState != ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED &&
        transferState != ZR_OWNERSHIP_TRANSFER_STATE_ABORTED) {
        return;
    }
    ZrCore_OwnershipTransfer_InternalDelete(envelope);
}
