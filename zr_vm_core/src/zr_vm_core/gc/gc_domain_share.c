#include "zr_vm_core/gc_domain.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"

static void gc_domain_share_diagnostic_init(
        SZrGcDomainShareDiagnostic *diagnostic,
        const SZrGcDomainShareRequest *request) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_NONE;
    if (request != ZR_NULL) {
        diagnostic->target = request->target;
        diagnostic->producerDomain = ZrCore_GcDomain_GetIdentity(request->producerState);
        diagnostic->consumerDomain = ZrCore_GcDomain_GetIdentity(request->consumerState);
    }
}

TZrBool ZrCore_GcDomain_ShareValue(
        const SZrGcDomainShareRequest *request,
        SZrGcDomainShareDiagnostic *diagnostic,
        SZrGcRootHandle *outHandle) {
    SZrGcDomainIdentity domain;

    gc_domain_share_diagnostic_init(diagnostic, request);
    if (outHandle != ZR_NULL) {
        outHandle->domain.id = 0u;
        outHandle->domain.generation = 0u;
        outHandle->slotIndex = ~(TZrUInt32)0u;
        outHandle->slotGeneration = 0u;
    }
    if (request == ZR_NULL || request->producerState == ZR_NULL ||
        request->consumerState == ZR_NULL || request->target == ZR_NULL ||
        request->consumerState->gcDomain == ZR_NULL || outHandle == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (request->producerState->gcDomain != request->consumerState->gcDomain ||
        !ZrCore_GcDomain_ObjectBelongsToState(
                request->producerState, request->target)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_FOREIGN_DOMAIN;
        }
        return ZR_FALSE;
    }
    if (request->sharedReference) {
        if (!request->syncProof) {
            if (diagnostic != ZR_NULL) {
                diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_SYNC_PROOF_REQUIRED;
            }
            return ZR_FALSE;
        }
    } else if (!request->sendProof) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_SEND_PROOF_REQUIRED;
        }
        return ZR_FALSE;
    }
    if (request->target->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_DROPPING ||
        request->target->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_DROPPED) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_LIFETIME;
        }
        return ZR_FALSE;
    }
    domain = ZrCore_GcDomain_GetIdentity(request->producerState);
    if (!ZrCore_GcRootHandle_Create(request->consumerState, request->target, outHandle)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->failure = ZR_GC_DOMAIN_SHARE_FAILURE_ROOT_REGISTRATION;
        }
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL) {
        diagnostic->producerDomain = domain;
    }
    return ZR_TRUE;
}
