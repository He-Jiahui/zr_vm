#include "ownership_transfer_internal.h"
#include "gc/gc_domain_internal.h"

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
/** @brief 校验 ValueCopy 布局和 schema。BUG: 父布局可掩盖嵌套 MOVE_ONLY，随后被原样 memcpy。 */
static TZrBool ownership_transfer_value_copy_layout_matches(
        const SZrDomainTransferContract *contract,
        const SZrTypeLayout *layout) {
    return contract != ZR_NULL && layout != ZR_NULL &&
           contract->kind == ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY &&
           layout->domainTransferKind ==
                   (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY &&
           ZrCore_TypeLayout_Validate(layout) &&
           ZrCore_TypeLayout_CanRawCopy(layout) &&
           layout->domainTransferSchemaVersion == contract->schemaVersion &&
           layout->domainTransferSchemaHash == contract->schemaHash;
}
/** @brief 快照跨域 VALUE_COPY 内联布局。@pre sourceStorage 调用期间稳定可读并覆盖 byteSize 字节。@return 成功返回 PREPARED envelope，否则返回空。 */
SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
        SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        const SZrTypeLayout *layout,
        const void *sourceStorage,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrGcDomainIdentity sourceDomain;
    SZrOwnershipTransferEnvelope *envelope;

    ZrCore_OwnershipTransfer_InternalDiagnosticSet(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
            0u,
            0u,
            0u);
    if (sourceState == ZR_NULL || sourceState->global == ZR_NULL ||
        sourceStorage == ZR_NULL || targetDomain.id == 0u ||
        targetDomain.generation == 0u ||
        !ZrCore_OwnershipTransfer_InternalContractIsValid(contract) ||
        !ownership_transfer_value_copy_layout_matches(contract, layout)) {
        return ZR_NULL;
    }
    sourceDomain = ZrCore_GcDomain_GetIdentity(sourceState);
    if (sourceDomain.id == 0u || sourceDomain.generation == 0u ||
        ZrCore_GcDomain_IdentityEquals(sourceDomain, targetDomain)) {
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }
    /* targetDomain 只是待验证的身份快照，目标 state 在 Commit 时再次核对。 */
    envelope = ZrCore_OwnershipTransfer_InternalNew(
            sourceState, sourceDomain, targetDomain); /* TODO: 公共契约需明确源 global 保活至 terminal Free；envelope/payload 均由其分配。 */
    if (envelope == ZR_NULL) {
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                0u,
                layout->byteSize,
                0u);
        return ZR_NULL;
    }
    envelope->valueBytes = (TZrByte *)ZrCore_Memory_RawMallocWithType(
            sourceState->global,
            layout->byteSize,
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
    if (envelope->valueBytes == ZR_NULL) {
        ZrCore_OwnershipTransfer_InternalDelete(envelope);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                0u,
                layout->byteSize,
                0u);
        return ZR_NULL;
    }
    memcpy(envelope->valueBytes, sourceStorage, layout->byteSize);
    envelope->valueByteCount = layout->byteSize;
    envelope->serializedByteCount = layout->byteSize;
    envelope->isCrossDomain = ZR_TRUE;
    envelope->kind = ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY;
    envelope->schemaVersion = contract->schemaVersion;
    envelope->schemaHash = contract->schemaHash;
    envelope->hasPayload = ZR_TRUE;
    ZrCore_OwnershipTransfer_InternalStateStore(
            envelope, ZR_OWNERSHIP_TRANSFER_STATE_PREPARED);
    ZrCore_GcDomain_RecordTransferTelemetry(
            sourceState->global,
            sourceDomain,
            ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PREPARE,
            0u,
            layout->byteSize);
    ZrCore_OwnershipTransfer_InternalDiagnosticSet(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            0u,
            layout->byteSize,
            0u);
    return envelope;
}
/** @brief 校验目标域、schema 与 claim 后写入 targetStorage 并消费快照。@pre 目标存储覆盖 byteSize 且可写。@return 仅成功提交时返回 TRUE。 */
TZrBool ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        const SZrTypeLayout *targetLayout,
        void *targetStorage,
        SZrDomainTransferDiagnostic *diagnostic) {
    TZrByte *bytesToFree;
    TZrUInt32 byteCount;
    SZrGlobalState *ownerGlobal;

    ZrCore_OwnershipTransfer_InternalDiagnosticSet(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
            0u,
            0u,
            0u); /* TODO: 明确 NULL 输入与尺寸错配的 INVALID_ARGUMENT/DECODE_FAILED/STATE_CONFLICT 归类。 */
    if (envelope != ZR_NULL && envelope->isCrossDomain &&
        targetState != ZR_NULL &&
        !ZrCore_OwnershipTransfer_InternalTargetMatches(
                targetState, envelope)) {
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
                0u,
                envelope->serializedByteCount,
                0u);
        return ZR_FALSE;
    }
    if (envelope == ZR_NULL || !envelope->isCrossDomain ||
        envelope->kind != ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY ||
        !ZrCore_OwnershipTransfer_InternalTargetMatches(
                targetState, envelope) ||
        targetStorage == ZR_NULL || targetLayout == ZR_NULL ||
        !ZrCore_TypeLayout_Validate(targetLayout) ||
        !ZrCore_TypeLayout_CanRawCopy(targetLayout) ||
        targetLayout->domainTransferKind !=
                (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY ||
        targetLayout->domainTransferSchemaVersion != envelope->schemaVersion ||
        targetLayout->domainTransferSchemaHash != envelope->schemaHash) {
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
                0u,
                envelope != ZR_NULL ? envelope->serializedByteCount : 0u,
                0u);
        return ZR_FALSE;
    }
    /* 预检在锁外完成，claim 身份和状态在锁内再次确认，避免重复消费。 */
    ZrCore_OwnershipTransfer_InternalLock(envelope);
    if (!envelope->hasPayload || envelope->valueBytes == ZR_NULL ||
        envelope->valueByteCount != targetLayout->byteSize ||
        envelope->claimantWorkerId != workerId ||
        envelope->claimEpoch != claimEpoch ||
        ZrCore_OwnershipTransfer_InternalStateLoad(envelope) !=
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        ZrCore_OwnershipTransfer_InternalUnlock(envelope);
        ZrCore_OwnershipTransfer_InternalDiagnosticSet(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT,
                0u,
                envelope->serializedByteCount,
                0u);
        return ZR_FALSE;
    }
    memcpy(targetStorage, envelope->valueBytes, envelope->valueByteCount);
    bytesToFree = envelope->valueBytes;
    byteCount = envelope->valueByteCount;
    ownerGlobal = envelope->ownerGlobal;
    envelope->valueBytes = ZR_NULL;
    envelope->valueByteCount = 0u;
    envelope->hasPayload = ZR_FALSE;
    ZrCore_OwnershipTransfer_InternalStateStore(
            envelope, ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
    ZrCore_OwnershipTransfer_InternalUnlock(envelope); /* terminal 状态发布后再释放源域字节；Free 仍须遵守 envelope 外部静默前提。 */
    ZrCore_Memory_RawFreeWithType(
            ownerGlobal,
            bytesToFree,
            byteCount,
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
    ZrCore_OwnershipTransfer_InternalDiagnosticSet(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            0u,
            byteCount,
            0u);
    ZrCore_GcDomain_RecordTransferTelemetry(
            targetState->global,
            envelope->targetDomain,
            ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_COMMIT,
            0u,
            byteCount);
    return ZR_TRUE;
}
