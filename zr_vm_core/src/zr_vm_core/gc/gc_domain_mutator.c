#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "gc/gc_domain_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

#if !defined(ZR_PLATFORM_WIN)
#include <time.h>
#endif

#define ZR_GC_DOMAIN_MUTATOR_INITIAL_CAPACITY ((TZrSize)4u)
#define ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS ((TZrUInt32)10u)

static SZrGcDomainMutatorRecord *gc_domain_find_mutator_locked(
        SZrGcDomain *domain,
        const SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        if (domain->mutators[index].state == state) {
            return &domain->mutators[index];
        }
    }
    return ZR_NULL;
}

static TZrBool gc_domain_grow_mutators_locked(SZrGcDomain *domain) {
    TZrSize newCapacity;
    TZrSize newBytes;
    SZrGcDomainMutatorRecord *newMutators;

    if (domain == ZR_NULL || domain->global == ZR_NULL) {
        return ZR_FALSE;
    }
    newCapacity = domain->mutatorCapacity == 0u
                          ? ZR_GC_DOMAIN_MUTATOR_INITIAL_CAPACITY
                          : domain->mutatorCapacity * 2u;
    newBytes = newCapacity * sizeof(SZrGcDomainMutatorRecord);
    newMutators = (SZrGcDomainMutatorRecord *)ZrCore_Memory_RawMallocWithType(
            domain->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newMutators == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newMutators, 0, newBytes);
    if (domain->mutators != ZR_NULL && domain->mutatorLength > 0u) {
        ZrCore_Memory_RawCopy(
                newMutators,
                domain->mutators,
                domain->mutatorLength * sizeof(SZrGcDomainMutatorRecord));
    }
    if (domain->mutators != ZR_NULL && domain->mutatorCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(
                domain->global,
                domain->mutators,
                domain->mutatorCapacity * sizeof(SZrGcDomainMutatorRecord),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    domain->mutators = newMutators;
    domain->mutatorCapacity = newCapacity;
    return ZR_TRUE;
}

static TZrUInt64 gc_domain_now_milliseconds(void) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)GetTickCount64();
#else
    struct timespec current;
    if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) {
        return 0u;
    }
    return (TZrUInt64)current.tv_sec * 1000u +
           (TZrUInt64)current.tv_nsec / 1000000u;
#endif
}

static void gc_domain_wait_locked(
        SZrGcDomain *domain,
        TZrUInt32 milliseconds) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    (void)SleepConditionVariableCS(
            &domain->coordinationCondition,
            &domain->coordinationLock,
            milliseconds);
#else
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        return;
    }
    deadline.tv_sec += (time_t)(milliseconds / 1000u);
    deadline.tv_nsec += (long)(milliseconds % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    (void)pthread_cond_timedwait(
            &domain->coordinationCondition,
            &domain->coordinationLock,
            &deadline);
#endif
}

static SZrGcDomainMutatorRecord *gc_domain_wait_for_entry_boundary_locked(
        SZrGcDomain *domain,
        SZrState *state,
        SZrGcDomainMutatorRecord *record) {
    while (record != ZR_NULL && domain->pauseRequested &&
           domain->collectorState != state) {
        if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING &&
            record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
            record->observedEpoch = domain->safepointEpoch;
            record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED;
            ZrCore_GcDomain_Broadcast(domain);
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
        record = gc_domain_find_mutator_locked(domain, state);
    }
    if (record != ZR_NULL &&
        record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        ZrCore_GcDomain_Broadcast(domain);
    }
    return record;
}

static SZrGcDomainMutatorRecord *gc_domain_first_blocker_locked(
        SZrGcDomain *domain) {
    if (domain == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        SZrGcDomainMutatorRecord *record = &domain->mutators[index];

        if (record->state == domain->collectorState ||
            record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE ||
            record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED) {
            continue;
        }
        if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED &&
            record->observedEpoch == domain->safepointEpoch) {
            continue;
        }
        return record;
    }
    return ZR_NULL;
}

TZrBool ZrCore_GcDomain_CoordinationInit(SZrGcDomain *domain) {
    if (domain == ZR_NULL) {
        return ZR_FALSE;
    }
#if defined(ZR_PLATFORM_WIN)
    InitializeCriticalSection(&domain->coordinationLock);
    InitializeConditionVariable(&domain->coordinationCondition);
#else
    if (pthread_mutex_init(&domain->coordinationLock, ZR_NULL) != 0) {
        return ZR_FALSE;
    }
    if (pthread_cond_init(&domain->coordinationCondition, ZR_NULL) != 0) {
        (void)pthread_mutex_destroy(&domain->coordinationLock);
        return ZR_FALSE;
    }
#endif
    domain->coordinationInitialized = ZR_TRUE;
    domain->nextMutatorId = 1u;
    return ZR_TRUE;
}

void ZrCore_GcDomain_CoordinationFree(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    DeleteCriticalSection(&domain->coordinationLock);
#else
    (void)pthread_cond_destroy(&domain->coordinationCondition);
    (void)pthread_mutex_destroy(&domain->coordinationLock);
#endif
    domain->coordinationInitialized = ZR_FALSE;
}

void ZrCore_GcDomain_Lock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    EnterCriticalSection(&domain->coordinationLock);
#else
    (void)pthread_mutex_lock(&domain->coordinationLock);
#endif
}

void ZrCore_GcDomain_Unlock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    LeaveCriticalSection(&domain->coordinationLock);
#else
    (void)pthread_mutex_unlock(&domain->coordinationLock);
#endif
}

void ZrCore_GcDomain_Broadcast(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    WakeAllConditionVariable(&domain->coordinationCondition);
#else
    (void)pthread_cond_broadcast(&domain->coordinationCondition);
#endif
}

TZrBool ZrCore_GcDomain_RegisterMutator(
        SZrGcDomain *domain,
        SZrState *state) {
    SZrGcDomainMutatorRecord *record;

    if (domain == ZR_NULL || state == ZR_NULL || !domain->active) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    if (domain->mutatorLength == domain->mutatorCapacity &&
        !gc_domain_grow_mutators_locked(domain)) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    record = &domain->mutators[domain->mutatorLength++];
    ZrCore_Memory_RawSet(record, 0, sizeof(*record));
    record->state = state;
    record->mutatorId = domain->nextMutatorId++;
    if (record->mutatorId == 0u) {
        record->mutatorId = domain->nextMutatorId++;
    }
    record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

void ZrCore_GcDomain_UnregisterMutator(
        SZrGcDomain *domain,
        SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL) {
        return;
    }
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        if (domain->mutators[index].state == state) {
            TZrSize tailLength = domain->mutatorLength - index - 1u;
            for (TZrSize offset = 0u; offset < tailLength; offset++) {
                domain->mutators[index + offset] =
                        domain->mutators[index + offset + 1u];
            }
            domain->mutatorLength--;
            ZrCore_Memory_RawSet(
                    &domain->mutators[domain->mutatorLength],
                    0,
                    sizeof(SZrGcDomainMutatorRecord));
            break;
        }
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}

TZrBool ZrCore_GcDomain_MutatorAttach(
        SZrState *ownerState,
        SZrState *mutatorState) {
    if (ownerState == ZR_NULL || ownerState->gcDomain == ZR_NULL ||
        mutatorState == ZR_NULL || mutatorState->gcDomain != ZR_NULL ||
        mutatorState->global != ownerState->global) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_AttachState(ownerState->gcDomain, mutatorState);
    return mutatorState->gcDomain == ownerState->gcDomain;
}

void ZrCore_GcDomain_MutatorDetach(SZrState *mutatorState) {
    SZrGcDomain *domain;

    if (mutatorState == ZR_NULL || mutatorState->gcDomain == ZR_NULL) {
        return;
    }
    domain = mutatorState->gcDomain;
    ZrCore_GcDomain_DetachState(domain, mutatorState);
}

TZrBool ZrCore_GcDomain_MutatorEnter(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    record = gc_domain_wait_for_entry_boundary_locked(domain, state, record);
    if (record == ZR_NULL) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE) {
        record->executionDepth = 1u;
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
        record->observedEpoch = domain->safepointEpoch;
    } else if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING &&
               record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE &&
               record->executionDepth != ~(TZrUInt32)0u) {
        record->executionDepth++;
    } else {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

void ZrCore_GcDomain_MutatorLeave(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL && record->executionDepth > 0u) {
        record->executionDepth--;
        if (record->executionDepth == 0u && record->nativeDepth == 0u) {
            record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
            record->nativeEnteredFromInactive = ZR_FALSE;
            record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
            record->observedEpoch = domain->safepointEpoch;
            ZrCore_GcDomain_Broadcast(domain);
        }
    }
    ZrCore_GcDomain_Unlock(domain);
}

void ZrCore_GcDomain_MutatorUnwindScopes(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        record->executionDepth = 0u;
        record->nativeDepth = 0u;
        record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
        record->nativeEnteredFromInactive = ZR_FALSE;
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
        record->observedEpoch = domain->safepointEpoch;
        ZrCore_GcDomain_Broadcast(domain);
    }
    ZrCore_GcDomain_Unlock(domain);
}

TZrBool ZrCore_GcDomain_MutatorPoll(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record == ZR_NULL || !domain->pauseRequested ||
        domain->collectorState == state ||
        record->status != ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING ||
        record->nativeMode != ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    record->observedEpoch = domain->safepointEpoch;
    record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED;
    ZrCore_GcDomain_Broadcast(domain);
    while (domain->pauseRequested && domain->collectorState != state) {
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL &&
        record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
    }
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

TZrUInt64 ZrCore_GcDomain_GetMutatorId(const SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrUInt64 result = 0u;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return 0u;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        result = record->mutatorId;
    }
    ZrCore_GcDomain_Unlock(domain);
    return result;
}

TZrBool ZrCore_GcDomain_NativeEnter(
        SZrState *state,
        EZrGcNativeSafepointMode mode) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL ||
        mode < ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE ||
        mode > ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    record = gc_domain_wait_for_entry_boundary_locked(domain, state, record);
    if (record == ZR_NULL) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->nativeDepth > 0u) {
        if (record->nativeMode != mode || record->nativeDepth == ~(TZrUInt32)0u) {
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        record->nativeDepth++;
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        record->nativeEnteredFromInactive = ZR_TRUE;
    } else {
        record->nativeEnteredFromInactive = ZR_FALSE;
    }
    if (record->status != ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    record->nativeDepth = 1u;
    record->nativeMode = mode;
    if (mode == ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED;
    } else if (mode == ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL;
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

void ZrCore_GcDomain_NativeLeave(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrBool pollAfterLeave = ZR_FALSE;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL && record->nativeDepth > 0u) {
        record->nativeDepth--;
        if (record->nativeDepth == 0u) {
            record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
            if (record->executionDepth == 0u) {
                record->status =
                        ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
                record->nativeEnteredFromInactive = ZR_FALSE;
            } else {
                record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
                pollAfterLeave = domain->pauseRequested &&
                                 domain->collectorState != state;
            }
            ZrCore_GcDomain_Broadcast(domain);
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    if (pollAfterLeave) {
        ZrCore_GcDomain_MutatorPoll(state);
    }
}

TZrBool ZrCore_GcDomain_StopTheWorldBegin(
        SZrState *state,
        TZrUInt32 timeoutMilliseconds,
        SZrGcDomainPauseDiagnostic *outDiagnostic) {
    SZrGcDomain *domain;
    TZrUInt64 started;

    if (outDiagnostic != ZR_NULL) {
        ZrCore_Memory_RawSet(outDiagnostic, 0, sizeof(*outDiagnostic));
        outDiagnostic->blockingNativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    }
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    started = gc_domain_now_milliseconds();
    ZrCore_GcDomain_Lock(domain);
    {
        SZrGcDomainMutatorRecord *caller =
                gc_domain_find_mutator_locked(domain, state);
        if (caller != ZR_NULL &&
            (caller->status ==
                     ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED ||
             caller->status ==
                     ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL)) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
                outDiagnostic->blockingMutatorId = caller->mutatorId;
                outDiagnostic->blockingNativeMode = caller->nativeMode;
                outDiagnostic->blockingState = caller->state;
                outDiagnostic->blockingNativeFrame =
                        caller->state != ZR_NULL
                                ? caller->state->callInfoList
                                : ZR_NULL;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
    }
    if (domain->pauseRequested && domain->collectorState == state) {
        domain->pauseDepth++;
        if (outDiagnostic != ZR_NULL) {
            outDiagnostic->safepointEpoch = domain->safepointEpoch;
        }
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    while (domain->pauseRequested) {
        TZrUInt64 elapsed = gc_domain_now_milliseconds() - started;
        if (elapsed >= timeoutMilliseconds) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
    domain->safepointEpoch++;
    if (domain->safepointEpoch == 0u) {
        domain->safepointEpoch = 1u;
    }
    domain->pauseRequested = ZR_TRUE;
    domain->collectorState = state;
    domain->pauseDepth = 1u;
    ZrCore_GcDomain_Broadcast(domain);

    for (;;) {
        SZrGcDomainMutatorRecord *blocker = gc_domain_first_blocker_locked(domain);
        TZrUInt64 elapsed;

        if (blocker == ZR_NULL) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_TRUE;
        }
        elapsed = gc_domain_now_milliseconds() - started;
        if (elapsed >= timeoutMilliseconds) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
                outDiagnostic->blockingMutatorId = blocker->mutatorId;
                outDiagnostic->blockingNativeMode = blocker->nativeMode;
                outDiagnostic->blockingState = blocker->state;
                outDiagnostic->blockingNativeFrame =
                        blocker->state != ZR_NULL
                                ? blocker->state->callInfoList
                                : ZR_NULL;
            }
            domain->pauseRequested = ZR_FALSE;
            domain->collectorState = ZR_NULL;
            domain->pauseDepth = 0u;
            ZrCore_GcDomain_Broadcast(domain);
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
}

void ZrCore_GcDomain_StopTheWorldEnd(SZrState *state) {
    SZrGcDomain *domain;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    if (!domain->pauseRequested || domain->collectorState != state) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    if (domain->pauseDepth > 1u) {
        domain->pauseDepth--;
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    domain->pauseRequested = ZR_FALSE;
    domain->collectorState = ZR_NULL;
    domain->pauseDepth = 0u;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}

void ZrCore_GcDomain_GetMutatorSnapshot(
        const SZrState *state,
        SZrGcDomainMutatorSnapshot *outSnapshot) {
    SZrGcDomain *domain;

    if (outSnapshot == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(outSnapshot, 0, sizeof(*outSnapshot));
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    outSnapshot->safepointEpoch = domain->safepointEpoch;
    outSnapshot->registeredMutatorCount = (TZrUInt32)domain->mutatorLength;
    outSnapshot->pauseRequested = domain->pauseRequested;
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        switch (domain->mutators[index].status) {
            case ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING:
                outSnapshot->runningMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED:
                outSnapshot->parkedMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED:
                outSnapshot->blockingDetachedMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL:
                outSnapshot->noSafepointCriticalMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE:
            default:
                break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
}

void ZrCore_GcDomain_WakeMutators(SZrState *state) {
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    ZrCore_GcDomain_Lock(state->gcDomain);
    ZrCore_GcDomain_Broadcast(state->gcDomain);
    ZrCore_GcDomain_Unlock(state->gcDomain);
}
