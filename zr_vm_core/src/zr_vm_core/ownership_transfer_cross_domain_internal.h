#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_CROSS_DOMAIN_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_CROSS_DOMAIN_INTERNAL_H

#include "zr_vm_core/ownership_transfer.h"

typedef struct SZrDomainTransferGraph SZrDomainTransferGraph;

SZrDomainTransferGraph *ZrCore_DomainTransferGraph_Prepare(
        struct SZrState *sourceState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        SZrDomainTransferDiagnostic *diagnostic,
        TZrUInt32 *outObjectCount,
        TZrUInt64 *outByteCount);

TZrBool ZrCore_DomainTransferGraph_Commit(
        struct SZrState *targetState,
        const SZrDomainTransferGraph *graph,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

void ZrCore_DomainTransferGraph_Free(SZrDomainTransferGraph *graph);

#endif
