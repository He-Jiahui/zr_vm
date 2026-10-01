#include "zr_vm_core/gc_domain_clone.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

/* wrapper 持有源分配器下的 envelope；提交或取消后缓存标量快照，允许先销毁源域再查询。 */
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

/* 各阶段只向可选诊断写状态与计数，不借出事务内部资源。 */
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

/* 在可能关闭 envelope 的阶段前更新快照，避免终结后查询依赖源域内存。 */
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

/* 只有状态机终结后才能归还源侧 envelope；wrapper 继续负责 GetSnapshot 的可观测性。 */
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
    /* 源分配器仍有效时关闭 envelope；终结后的调用方仅从缓存读取快照。 */
    ZrCore_OwnershipTransfer_Free(
            transaction->sourceState, transaction->envelope);
    transaction->envelope = ZR_NULL;
}

/* 结构化复制必须跨两个活动域；相等的当前身份属于同域共享而非图复制。 */
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

/* Prepare 在源侧编码；TODO: 通过预检的无原型 raw array 物化会改动源存储，核实 const source 契约。 */
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
    /* TODO: 下层图提交仅校验非零 schema 字段；若允许跨版本传输，需核实目标端何处验证格式指纹。 */
    contract.flags = ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    contract.quota = *quota;
    /* TODO: 无原型 raw array 物化若 OOM Throw，会越过事务清理；核实此源图在有效 TryRun 中是否可达。 */
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

/* 仅转移事务的可领取状态，wrapper/envelope 仍由 Prepare 调用方负责终结。 */
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

/* 领取前再查目标域代数，避免向已经关闭的目标域提交预编码图。 */
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

/* 目标域重建成功即关闭源侧信封；失败保留可重试或 Abort/Free 的已领取事务。 */
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

/* 源域在目标域销毁后仍有取消权，因而领取后的取消使用保存的 worker 身份。 */
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
        /* 目标域已关闭时只剩源域具备取消权限。 */
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

/* 未终结事务先尝试取消；若并发提交占有信封，保留 wrapper 给外部重试。 */
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
                /* TODO: 此处静默保留 wrapper；核实跨线程调用方能否观察失败并安排再次 Free。 */
                return;
            }
            if (transaction->envelope != ZR_NULL) {
                ZrCore_OwnershipTransfer_GetSnapshot(
                        transaction->envelope, &snapshot);
            } else if (transaction->hasCachedSnapshot) {
                snapshot = transaction->cachedSnapshot;
            }
        }
        /* envelope 属于源分配器，终结清理前 sourceState 必须仍有效。 */
        if (gc_domain_clone_state_is_terminal(snapshot.state) &&
            transaction->sourceState != ZR_NULL) {
            ZrCore_OwnershipTransfer_Free(
                    transaction->sourceState, transaction->envelope);
        }
        transaction->envelope = ZR_NULL;
    }
    free(transaction);
}

/* 对外暴露事务状态的拷贝；信封关闭后使用终结前缓存而不借用已释放内存。 */
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

typedef struct SZrGcDomainCloneExecuteCommit {
    SZrGcDomainCloneTransaction *transaction;
    SZrTypeValue *target;
    SZrDomainTransferDiagnostic *diagnostic;
    TZrBool result;
    TZrBool returned;
} SZrGcDomainCloneExecuteCommit;

static void gc_domain_clone_execute_commit(SZrState *state, TZrPtr arguments) {
    SZrGcDomainCloneExecuteCommit *context = (SZrGcDomainCloneExecuteCommit *)arguments;
    (void)state;
    context->result = ZrCore_GcDomainClone_Commit(
            context->transaction, context->target, context->diagnostic);
    context->returned = ZR_TRUE;
}

/* The hidden transaction must close before a target Commit Throw leaves Execute. */
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
    SZrGcDomainCloneExecuteCommit context = {0};
    EZrThreadStatus commitStatus;

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
    context.transaction = transaction;
    context.target = target;
    context.diagnostic = diagnostic;
    commitStatus = ZrCore_Exception_TryRun(targetState, gc_domain_clone_execute_commit, &context);
    if (!context.result) {
        (void)ZrCore_GcDomainClone_Abort(transaction, ZR_NULL);
    }
    ZrCore_GcDomainClone_Free(transaction);
    if (!context.returned) ZrCore_Exception_Throw(targetState, commitStatus);
    return context.result;
}
