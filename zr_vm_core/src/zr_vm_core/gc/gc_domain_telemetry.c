#include "gc_domain_internal.h"

#include "zr_vm_core/global.h"

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
    ZrCore_GcDomain_Lock(domain);
    if (!domain->active || domain->identity.id != identity.id ||
        domain->identity.generation != identity.generation) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
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
