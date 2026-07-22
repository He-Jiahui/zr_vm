#include "zr_vm_core/ownership_transfer.h"

#include "ownership_resource_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <sched.h>
#endif

struct SZrOwnershipTransferEnvelope {
    SZrGlobalState *ownerGlobal;
    SZrGcDomainIdentity targetDomain;
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    SZrTypeValue payload;
    volatile TZrInt32 state;
    volatile TZrInt32 transitionLock;
    TZrBool hasPayload;
};

static volatile TZrInt64 g_next_transfer_id = 0;

static TZrInt32 ownership_transfer_atomic_load_acquire(
        const volatile TZrInt32 *value) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrInt32)InterlockedCompareExchange(
            (volatile LONG *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void ownership_transfer_atomic_store_release(
        volatile TZrInt32 *value,
        TZrInt32 next) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

static TZrBool ownership_transfer_atomic_compare_exchange(
        volatile TZrInt32 *value,
        TZrInt32 expected,
        TZrInt32 next) {
#if defined(ZR_PLATFORM_WIN)
    return InterlockedCompareExchange(
                   (volatile LONG *)value,
                   (LONG)next,
                   (LONG)expected) == (LONG)expected;
#else
    return __atomic_compare_exchange_n(
            value,
            &expected,
            next,
            ZR_FALSE,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE);
#endif
}

static TZrUInt64 ownership_transfer_next_id(void) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedIncrement64((volatile LONG64 *)&g_next_transfer_id);
#else
    return (TZrUInt64)__atomic_add_fetch(
            &g_next_transfer_id, 1, __ATOMIC_RELAXED);
#endif
}

static void ownership_transfer_lock(SZrOwnershipTransferEnvelope *envelope) {
    while (!ownership_transfer_atomic_compare_exchange(
            &envelope->transitionLock, 0, 1)) {
#if defined(ZR_PLATFORM_WIN)
        (void)SwitchToThread();
#else
        (void)sched_yield();
#endif
    }
}

static void ownership_transfer_unlock(SZrOwnershipTransferEnvelope *envelope) {
    ownership_transfer_atomic_store_release(&envelope->transitionLock, 0);
}

static TZrBool ownership_transfer_state_matches_domain(
        const SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope) {
    return state != ZR_NULL && envelope != ZR_NULL &&
           ZrCore_GcDomain_IdentityIsCurrent(state, envelope->targetDomain);
}

SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_PrepareSameDomain(
        SZrState *state,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source) {
    SZrOwnershipTransferEnvelope *envelope;

    if (state == ZR_NULL || state->global == ZR_NULL || source == ZR_NULL ||
        !ZrCore_GcDomain_IdentityIsCurrent(state, targetDomain) ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_NULL;
    }
    envelope = (SZrOwnershipTransferEnvelope *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrOwnershipTransferEnvelope),
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
    if (envelope == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Memory_RawSet(envelope, 0, sizeof(*envelope));
    envelope->ownerGlobal = state->global;
    envelope->targetDomain = targetDomain;
    envelope->transferId = ownership_transfer_next_id();
    envelope->generation = targetDomain.generation;
    ZrCore_Value_ResetAsNull(&envelope->payload);
    if (!ZrCore_Ownership_UniqueValue(state, &envelope->payload, source)) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                envelope,
                sizeof(*envelope),
                ZR_MEMORY_NATIVE_TYPE_MANAGER);
        return ZR_NULL;
    }
    envelope->hasPayload = ZR_TRUE;
    ownership_transfer_atomic_store_release(
            &envelope->state,
            (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_PREPARED);
    return envelope;
}

TZrBool ZrCore_OwnershipTransfer_Publish(
        SZrOwnershipTransferEnvelope *envelope) {
    TZrBool result;

    if (envelope == ZR_NULL) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    result = envelope->hasPayload &&
             ownership_transfer_atomic_compare_exchange(
                     &envelope->state,
                     (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_PREPARED,
                     (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED);
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Claim(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch) {
    TZrBool result = ZR_FALSE;

    if (!ownership_transfer_state_matches_domain(targetState, envelope) ||
        workerId == 0u || claimEpoch == 0u) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    if (envelope->hasPayload &&
        ownership_transfer_atomic_load_acquire(&envelope->state) ==
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED) {
        envelope->claimantWorkerId = workerId;
        envelope->claimEpoch = claimEpoch;
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_QUEUED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED);
        if (!result) {
            envelope->claimantWorkerId = 0u;
            envelope->claimEpoch = 0u;
        }
    }
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Commit(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target) {
    TZrBool result = ZR_FALSE;

    if (!ownership_transfer_state_matches_domain(targetState, envelope) ||
        target == ZR_NULL) {
        return ZR_FALSE;
    }
    ownership_transfer_lock(envelope);
    if (envelope->hasPayload &&
        envelope->claimantWorkerId == workerId &&
        envelope->claimEpoch == claimEpoch &&
        ownership_transfer_atomic_load_acquire(&envelope->state) ==
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED &&
        ZrCore_Ownership_UniqueValue(targetState, target, &envelope->payload)) {
        envelope->hasPayload = ZR_FALSE;
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED);
    }
    ownership_transfer_unlock(envelope);
    return result;
}

TZrBool ZrCore_OwnershipTransfer_Abort(
        SZrOwnershipTransferEnvelope *envelope,
        SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch) {
    EZrOwnershipTransferState state;
    SZrTypeValue payload;
    TZrBool releasePayload = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (!ownership_transfer_state_matches_domain(targetState, envelope)) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(&payload);
    ownership_transfer_lock(envelope);
    state = (EZrOwnershipTransferState)ownership_transfer_atomic_load_acquire(
            &envelope->state);
    if ((state == ZR_OWNERSHIP_TRANSFER_STATE_PREPARED ||
         state == ZR_OWNERSHIP_TRANSFER_STATE_QUEUED) &&
        workerId == 0u && claimEpoch == 0u) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    } else if (state == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED &&
               envelope->claimantWorkerId == workerId &&
               envelope->claimEpoch == claimEpoch) {
        result = ownership_transfer_atomic_compare_exchange(
                &envelope->state,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
                (TZrInt32)ZR_OWNERSHIP_TRANSFER_STATE_ABORTED);
    }
    if (result && envelope->hasPayload) {
        payload = envelope->payload;
        ZrCore_Value_ResetAsNull(&envelope->payload);
        envelope->hasPayload = ZR_FALSE;
        releasePayload = ZR_TRUE;
    }
    ownership_transfer_unlock(envelope);
    if (releasePayload) {
        ZrCore_Ownership_ReleaseValue(targetState, &payload);
    }
    return result;
}

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
    ownership_transfer_lock(envelope);
    outSnapshot->state = (EZrOwnershipTransferState)
            ownership_transfer_atomic_load_acquire(&envelope->state);
    outSnapshot->targetDomain = envelope->targetDomain;
    outSnapshot->transferId = envelope->transferId;
    outSnapshot->generation = envelope->generation;
    outSnapshot->claimantWorkerId = envelope->claimantWorkerId;
    outSnapshot->claimEpoch = envelope->claimEpoch;
    outSnapshot->hasPayload = envelope->hasPayload;
    ownership_transfer_unlock(envelope);
}

void ZrCore_OwnershipTransfer_Free(
        SZrState *state,
        SZrOwnershipTransferEnvelope *envelope) {
    EZrOwnershipTransferState transferState;
    TZrUInt64 workerId = 0u;
    TZrUInt64 claimEpoch = 0u;

    if (!ownership_transfer_state_matches_domain(state, envelope)) {
        return;
    }
    ownership_transfer_lock(envelope);
    transferState = (EZrOwnershipTransferState)
            ownership_transfer_atomic_load_acquire(&envelope->state);
    if (transferState == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        workerId = envelope->claimantWorkerId;
        claimEpoch = envelope->claimEpoch;
    }
    ownership_transfer_unlock(envelope);
    if (transferState == ZR_OWNERSHIP_TRANSFER_STATE_PREPARED ||
        transferState == ZR_OWNERSHIP_TRANSFER_STATE_QUEUED ||
        transferState == ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED) {
        if (!ZrCore_OwnershipTransfer_Abort(
                    envelope, state, workerId, claimEpoch)) {
            return;
        }
    }
    ownership_transfer_lock(envelope);
    transferState = (EZrOwnershipTransferState)
            ownership_transfer_atomic_load_acquire(&envelope->state);
    ownership_transfer_unlock(envelope);
    if (transferState != ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED &&
        transferState != ZR_OWNERSHIP_TRANSFER_STATE_ABORTED) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(
            envelope->ownerGlobal,
            envelope,
            sizeof(*envelope),
            ZR_MEMORY_NATIVE_TYPE_MANAGER);
}
