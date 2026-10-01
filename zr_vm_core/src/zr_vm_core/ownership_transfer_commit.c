#include "ownership_transfer_internal.h"
/* provider 交接与结构化克隆共用 CLAIMED 状态门禁，但目标重建分别委托回调和图提交。 */
#include "ownership_resource_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/exception.h"
/* 保留 provider 可诊断的失败码，其余不明状态归并为提交失败。 */
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
/* 资源移动只接受目标域的直接 unique 值，避免把无所有权的半成品当作已提交。 */
static TZrBool ownership_transfer_commit_provider_target_is_valid(
        EZrDomainTransferKind kind,
        const SZrTypeValue *target) {
    if (target == ZR_NULL || ZR_VALUE_IS_TYPE_NULL(target->type)) {
        return ZR_FALSE;
    }
    return kind != ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE ||
           ZrCore_OwnershipResource_IsDirectUniqueValue(target);
}
/* 用 worker/epoch 独占提交窗口；回调在锁外执行，完成后再线性化 envelope 终态。 */
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
    /* TODO: provider 回调契约未说明能否非局部抛出；若可 Throw，此处会遗留 commitInProgress 和 payload，需核验注册方。 */
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
    /* 回调返回后重验同一认领，防止失败或重入时把目标写入已改变的信封。 */
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
    /* 回报诊断使用进入提交窗口时的计数，调用方据结果决定是否重试或终结。 */
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
/* 图提交同样独占窗口；目标对象分配发生在锁外并由图提交器管理临时根。 */
TZrBool ZrCore_OwnershipTransfer_InternalCommitGraph(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrDomainTransferGraph *graph;
    SZrDomainTransferGraphCommitFailure failure = {0};
    TZrUInt32 objectCount;
    TZrUInt64 byteCount;
    TZrBool result;
    /* 已占用窗口的认领不可再次提交；失败时不改变信封 payload。 */
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
            targetState, graph, target, diagnostic, &failure);
    /* 正常返回后再次核对认领与原图身份，再决定转移 payload 所有权。 */
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
        if (failure.thrown) ZrCore_Exception_Throw(targetState, failure.status);
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
    /* 成功时 payload 已交给目标，原图容器可释放；失败时保留以供撤销。 */
    if (result) {
        ZrCore_DomainTransferGraph_Free(graph);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_OK,
                objectCount,
                byteCount,
                0u);
    }
    /* The graph decoder retired all temporary roots. Release the commit
     * window before propagating its exact non-local status to the caller. */
    if (failure.thrown) ZrCore_Exception_Throw(targetState, failure.status);
    return result;
}
