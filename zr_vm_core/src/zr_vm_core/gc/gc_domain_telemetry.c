#include "gc_domain_internal.h"

#include "zr_vm_core/global.h"

/* 转移状态机在各提交点上报事件；只把当前有效域的事件并入统计。 */
void ZrCore_GcDomain_RecordTransferTelemetry(
        SZrGlobalState *global,
        SZrGcDomainIdentity identity,
        EZrGcDomainTransferTelemetryEvent event,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount) {
    SZrGcDomain *domain;

    if (global == ZR_NULL || global->gcDomain == ZR_NULL ||
        identity.id == 0u || identity.generation == 0u) {
        return;
    }
    domain = global->gcDomain;
    /* 与 GetStats 的域快照使用同一协调锁，防止并发读到部分计数。 */
    ZrCore_GcDomain_Lock(domain);
    if (!domain->active || domain->identity.id != identity.id ||
        domain->identity.generation != identity.generation) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    /* 准备记源端载荷，提交记目的端载荷；发布/领取/取消仅记事件次数。 */
    switch (event) {
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PREPARE:
            domain->outboundTransferPrepareCount++;
            domain->outboundTransferObjectCount += objectCount;
            domain->outboundTransferByteCount += byteCount;
            break;
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PUBLISH:
            domain->outboundTransferPublishCount++;
            break;
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_ABORT:
            domain->outboundTransferAbortCount++;
            break;
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_CLAIM:
            domain->inboundTransferClaimCount++;
            break;
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_COMMIT:
            domain->inboundTransferCommitCount++;
            domain->inboundTransferObjectCount += objectCount;
            domain->inboundTransferByteCount += byteCount;
            break;
        case ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_ABORT:
            domain->inboundTransferAbortCount++;
            break;
        default:
            break;
    }
    ZrCore_GcDomain_Unlock(domain);
}
