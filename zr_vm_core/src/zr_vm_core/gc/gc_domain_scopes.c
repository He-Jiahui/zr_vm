#include "gc/gc_domain_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

static SZrGcDomainMutatorRecord *gc_scopes_find_locked(
        SZrGcDomain *domain, const SZrState *state) {
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state)
            return &domain->mutators[index];
    }
    return ZR_NULL;
}

void ZrCore_GcDomain_CaptureScopes(
        SZrState *state, SZrGcDomainScopeSnapshot *snapshot) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    ZrCore_Memory_RawSet(snapshot, 0, sizeof(*snapshot));
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) return;
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_scopes_find_locked(domain, state);
    if (record != ZR_NULL) {
        snapshot->domain = domain;
        snapshot->identity = domain->identity;
        snapshot->mutatorId = record->mutatorId;
        snapshot->executionDepth = record->executionDepth;
        snapshot->nativeDepth = record->nativeDepth;
        snapshot->mutationDepth = record->mutationDepth;
        snapshot->nativeMode = record->nativeMode;
        snapshot->nativeEnteredFromInactive = record->nativeEnteredFromInactive;
    }
    ZrCore_GcDomain_Unlock(domain);
}

void ZrCore_GcDomain_RestoreScopes(
        SZrState *state, const SZrGcDomainScopeSnapshot *snapshot) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    if (state == ZR_NULL || snapshot == ZR_NULL || snapshot->domain == ZR_NULL ||
        state->gcDomain != snapshot->domain) return;
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_scopes_find_locked(domain, state);
    if (record == ZR_NULL || record->mutatorId != snapshot->mutatorId ||
        !ZrCore_GcDomain_IdentityEquals(domain->identity, snapshot->identity) ||
        record->mutationDepth < snapshot->mutationDepth) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }

    /* Release only callback-owned recursive levels. Keep coordination locked
     * until the roots and surviving caller scopes are safe to publish. */
    for (TZrUInt32 depth = record->mutationDepth;
         depth > snapshot->mutationDepth; --depth) {
        ZrCore_GcDomain_MutationUnlock(domain);
    }
    record->mutationDepth = snapshot->mutationDepth;
    record->executionDepth = snapshot->executionDepth;
    record->nativeDepth = snapshot->nativeDepth;
    record->nativeMode = snapshot->nativeMode;
    record->nativeEnteredFromInactive = snapshot->nativeEnteredFromInactive;
    if (record->nativeDepth > 0u &&
        record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED;
    } else if (record->nativeDepth > 0u &&
               record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL;
    } else {
        record->status = record->executionDepth > 0u || record->nativeDepth > 0u
                ? ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING
                : ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
    }
    record->observedEpoch = domain->safepointEpoch;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}
