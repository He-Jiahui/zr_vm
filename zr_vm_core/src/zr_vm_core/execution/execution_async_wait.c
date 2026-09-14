#include "zr_vm_core/async_frame_budget.h"

#include <limits.h>
#include <string.h>

#if defined(_MSC_VER)
#include <windows.h>
#endif

enum {
    ASYNC_STAGE_VALIDATE = 1u,
    ASYNC_STAGE_BEGIN = 2u,
    ASYNC_STAGE_POLL = 3u,
    ASYNC_STAGE_PIN = 4u,
    ASYNC_STAGE_WAIT = 5u,
    ASYNC_STAGE_COMPILE = 6u
};

static TZrUInt32 async_atomic_load(const volatile TZrUInt32 *value) {
#if defined(_MSC_VER)
    return (TZrUInt32)InterlockedCompareExchange(
            (volatile LONG *)value, 0L, 0L);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void async_atomic_store(volatile TZrUInt32 *value, TZrUInt32 next) {
#if defined(_MSC_VER)
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

static TZrBool async_atomic_cas(volatile TZrUInt32 *value,
                                TZrUInt32 *expected,
                                TZrUInt32 desired) {
#if defined(_MSC_VER)
    LONG old = InterlockedCompareExchange((volatile LONG *)value,
                                           (LONG)desired,
                                           (LONG)*expected);
    if ((TZrUInt32)old == *expected) {
        return ZR_TRUE;
    }
    *expected = (TZrUInt32)old;
    return ZR_FALSE;
#else
    return (TZrBool)__atomic_compare_exchange_n(
            value, expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

static void async_lock(volatile TZrUInt32 *lock) {
    TZrUInt32 expected;

    if (lock == ZR_NULL) {
        return;
    }
    for (;;) {
        expected = 0u;
        if (async_atomic_cas(lock, &expected, 1u)) {
            return;
        }
    }
}

static void async_unlock(volatile TZrUInt32 *lock) {
    if (lock != ZR_NULL) {
        async_atomic_store(lock, 0u);
    }
}

static void async_diag_clear(SZrAsyncFrameDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool async_fail(SZrAsyncFrameDiagnostic *diagnostic,
                          EZrAsyncFrameDiagnosticCode code,
                          TZrUInt32 stage,
                          TZrUInt32 state,
                          TZrUInt64 expected,
                          TZrUInt64 actual,
                          TZrUInt64 generation) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->stage = stage;
        diagnostic->state = state;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->generation = generation;
    }
    return ZR_FALSE;
}

void ZrCore_AsyncFrameBudget_DiagnosticClear(
        SZrAsyncFrameDiagnostic *diagnostic) {
    async_diag_clear(diagnostic);
}

const TZrChar *ZrCore_AsyncFrameBudget_DiagnosticName(
        EZrAsyncFrameDiagnosticCode code) {
    switch (code) {
        case ZR_ASYNC_FRAME_DIAGNOSTIC_NONE: return "none";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA: return "invalid-schema";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS: return "invalid-flags";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE: return "invalid-state";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND: return "borrow-across-suspend";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND: return "stack-alias-across-suspend";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND: return "lock-guard-across-suspend";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL: return "native-critical";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED: return "state-map-required";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_BUDGET_EXHAUSTED: return "budget-exhausted";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN: return "active-pin";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_PIN_UNDERFLOW: return "pin-underflow";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_CAPACITY: return "wait-capacity";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND: return "wait-not-found";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED: return "wait-already-resumed";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY: return "wait-not-ready";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CAPACITY: return "compile-capacity";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT: return "invalid-snapshot";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED: return "compile-cancelled";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION: return "stale-generation";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH: return "contract-mismatch";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_OUT_OF_MEMORY: return "out-of-memory";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW: return "overflow";
        case ZR_ASYNC_FRAME_DIAGNOSTIC_COUNT: break;
    }
    return "unknown";
}

const TZrChar *ZrCore_AsyncFrameBudget_StatusName(EZrAsyncFrameStatus status) {
    switch (status) {
        case ZR_ASYNC_FRAME_STATUS_IDLE: return "idle";
        case ZR_ASYNC_FRAME_STATUS_RUNNING: return "running";
        case ZR_ASYNC_FRAME_STATUS_SUSPENDED: return "suspended";
        case ZR_ASYNC_FRAME_STATUS_RESUMING: return "resuming";
        case ZR_ASYNC_FRAME_STATUS_COMPLETED: return "completed";
        case ZR_ASYNC_FRAME_STATUS_CANCELLED: return "cancelled";
        case ZR_ASYNC_FRAME_STATUS_FAULTED: return "faulted";
        case ZR_ASYNC_FRAME_STATUS_TORN_DOWN: return "torn-down";
        case ZR_ASYNC_FRAME_STATUS_COUNT: break;
    }
    return "unknown";
}

void ZrCore_AsyncFrameBudget_Init(SZrAsyncFrameBudget *frame) {
    if (frame == ZR_NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    frame->magic = ZR_ASYNC_FRAME_BUDGET_MAGIC;
    frame->schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    frame->status = ZR_ASYNC_FRAME_STATUS_IDLE;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
}

TZrBool ZrCore_AsyncFrameBudget_Validate(
        const SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    async_diag_clear(diagnostic);
    if (frame == ZR_NULL) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_VALIDATE, 0u, 1u, 0u, 0u);
    }
    if (frame->magic != ZR_ASYNC_FRAME_BUDGET_MAGIC) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
                          ASYNC_STAGE_VALIDATE, frame->status,
                          ZR_ASYNC_FRAME_BUDGET_MAGIC, frame->magic,
                          frame->generation);
    }
    if (frame->schemaVersion != ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
                          ASYNC_STAGE_VALIDATE, frame->status,
                          ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION,
                          frame->schemaVersion, frame->generation);
    }
    if ((frame->flags & ~ZR_ASYNC_FRAME_FLAG_KNOWN_MASK) != 0u ||
        frame->reserved != 0u ||
        frame->status >= ZR_ASYNC_FRAME_STATUS_COUNT ||
        frame->suspendReason >= ZR_ASYNC_FRAME_SUSPEND_COUNT ||
        (async_atomic_load(&frame->cancellationRequested) != 0u &&
         async_atomic_load(&frame->cancellationRequested) != 1u)) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS,
                          ASYNC_STAGE_VALIDATE, frame->status,
                          ZR_ASYNC_FRAME_FLAG_KNOWN_MASK, frame->flags,
                          frame->generation);
    }
    if (frame->frameId == 0u || frame->generation == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_VALIDATE, frame->status, 1u,
                          frame->frameId == 0u ? frame->frameId : frame->generation,
                          frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND) != 0u &&
        (frame->flags & ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT) == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS,
                          ASYNC_STAGE_VALIDATE, frame->status,
                          ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT, frame->flags,
                          frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID) != 0u &&
        (frame->stateMapId == 0u || frame->stateMapGeneration != frame->generation)) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED,
                          ASYNC_STAGE_VALIDATE, frame->status,
                          frame->generation, frame->stateMapGeneration,
                          frame->generation);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Begin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrBool wasCancelled;

    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_IDLE) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_BEGIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_IDLE, frame->status,
                          frame->generation);
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_RUNNING;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
    wasCancelled = (TZrBool)(async_atomic_load(&frame->cancellationRequested) != 0u);
    frame->pendingFlags = wasCancelled ? ZR_ASYNC_FRAME_PENDING_CANCEL : 0u;
    frame->consumedWorkUnits = 0u;
    frame->pollCount = 0u;
    frame->suspensionCount = 0u;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_CanSuspend(
        const SZrAsyncFrameBudget *frame,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_RUNNING &&
        frame->status != ZR_ASYNC_FRAME_STATUS_SUSPENDED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_POLL, frame->status,
                          ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                          frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT) == 0u ||
        (frame->flags & ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND) == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS,
                          ASYNC_STAGE_POLL, frame->status,
                          ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND, frame->flags,
                          frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_HAS_BORROW) != 0u) {
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND,
                          ASYNC_STAGE_POLL, frame->status, 0u,
                          frame->flags, frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_HAS_STACK_ALIAS) != 0u) {
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND,
                          ASYNC_STAGE_POLL, frame->status, 0u,
                          frame->flags, frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_HAS_LOCK_GUARD) != 0u) {
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND,
                          ASYNC_STAGE_POLL, frame->status, 0u,
                          frame->flags, frame->generation);
    }
    if (inNativeCritical ||
        (frame->flags & ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL) != 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL,
                          ASYNC_STAGE_POLL, frame->status, 0u, 1u,
                          frame->generation);
    }
    if (!atStateMapBoundary) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_POLL, frame->status, 1u, 0u,
                          frame->generation);
    }
    if ((frame->flags & ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID) == 0u ||
        frame->stateMapId == 0u ||
        frame->stateMapGeneration != frame->generation) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED,
                          ASYNC_STAGE_POLL, frame->status,
                          frame->generation, frame->stateMapGeneration,
                          frame->generation);
    }
    return ZR_TRUE;
}

EZrAsyncFramePollOutcome ZrCore_AsyncFrameBudget_Poll(
        SZrAsyncFrameBudget *frame,
        TZrUInt64 workUnits,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrBool budgetHit;
    TZrBool cancelled;

    async_diag_clear(diagnostic);
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_ASYNC_FRAME_POLL_FAULTED;
    }
    if (frame->status == ZR_ASYNC_FRAME_STATUS_SUSPENDED) {
        return ZR_ASYNC_FRAME_POLL_SUSPENDED;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_RUNNING) {
        async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                   ASYNC_STAGE_POLL, frame->status,
                   ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                   frame->generation);
        return ZR_ASYNC_FRAME_POLL_FAULTED;
    }
    if (frame->pollCount != UINT64_MAX) {
        frame->pollCount++;
    }
    if (workUnits > UINT64_MAX - frame->consumedWorkUnits) {
        frame->consumedWorkUnits = UINT64_MAX;
    } else {
        frame->consumedWorkUnits += workUnits;
    }
    budgetHit = frame->maxWorkUnits != 0u &&
                frame->consumedWorkUnits >= frame->maxWorkUnits;
    cancelled = async_atomic_load(&frame->cancellationRequested) != 0u;
    if (budgetHit) {
        frame->pendingFlags |= ZR_ASYNC_FRAME_PENDING_BUDGET;
    }
    if (cancelled) {
        frame->pendingFlags |= ZR_ASYNC_FRAME_PENDING_CANCEL;
    }

    /* A native critical section may finish, but is never interrupted here. */
    if (inNativeCritical ||
        (frame->flags & ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL) != 0u) {
        return ZR_ASYNC_FRAME_POLL_RUNNING;
    }
    if (cancelled && atStateMapBoundary) {
        frame->status = ZR_ASYNC_FRAME_STATUS_CANCELLED;
        frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
        return ZR_ASYNC_FRAME_POLL_CANCELLED;
    }
    if (!budgetHit || !atStateMapBoundary) {
        return ZR_ASYNC_FRAME_POLL_RUNNING;
    }
    if (!ZrCore_AsyncFrameBudget_CanSuspend(frame, ZR_TRUE, ZR_FALSE,
                                             diagnostic)) {
        /* Resource/contract violations are faults; a missing boundary remains
         * a running frame and is handled on the next poll. */
        if (diagnostic != ZR_NULL &&
            diagnostic->code == ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE &&
            diagnostic->actual == 0u) {
            return ZR_ASYNC_FRAME_POLL_RUNNING;
        }
        frame->status = ZR_ASYNC_FRAME_STATUS_FAULTED;
        return ZR_ASYNC_FRAME_POLL_FAULTED;
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_SUSPENDED;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_BUDGET;
    frame->pendingFlags &= ~ZR_ASYNC_FRAME_PENDING_BUDGET;
    if (frame->suspensionCount != UINT64_MAX) {
        frame->suspensionCount++;
    }
    return ZR_ASYNC_FRAME_POLL_SUSPENDED;
}

TZrBool ZrCore_AsyncFrameBudget_Suspend(
        SZrAsyncFrameBudget *frame,
        EZrAsyncFrameSuspendReason reason,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (reason <= ZR_ASYNC_FRAME_SUSPEND_NONE ||
        reason >= ZR_ASYNC_FRAME_SUSPEND_COUNT) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_POLL, frame->status,
                          ZR_ASYNC_FRAME_SUSPEND_WAIT, reason,
                          frame->generation);
    }
    if (!ZrCore_AsyncFrameBudget_CanSuspend(frame, atStateMapBoundary,
                                             inNativeCritical, diagnostic)) {
        return ZR_FALSE;
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_SUSPENDED;
    frame->suspendReason = reason;
    frame->pendingFlags &= ~ZR_ASYNC_FRAME_PENDING_BUDGET;
    if (frame->suspensionCount != UINT64_MAX) {
        frame->suspensionCount++;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Complete(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_RUNNING &&
        frame->status != ZR_ASYNC_FRAME_STATUS_SUSPENDED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_BEGIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                          frame->generation);
    }
    if (frame->pinCount != 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN,
                          ASYNC_STAGE_PIN, frame->status, 0u,
                          frame->pinCount, frame->generation);
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_COMPLETED;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
    frame->pendingFlags = 0u;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Fault(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status == ZR_ASYNC_FRAME_STATUS_TORN_DOWN ||
        frame->status == ZR_ASYNC_FRAME_STATUS_COMPLETED ||
        frame->status == ZR_ASYNC_FRAME_STATUS_CANCELLED ||
        frame->status == ZR_ASYNC_FRAME_STATUS_FAULTED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_BEGIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                          frame->generation);
    }
    if (frame->pinCount != 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN,
                          ASYNC_STAGE_PIN, frame->status, 0u,
                          frame->pinCount, frame->generation);
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_FAULTED;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Pin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_RUNNING &&
        frame->status != ZR_ASYNC_FRAME_STATUS_SUSPENDED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_PIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                          frame->generation);
    }
    if (frame->pinCount == UINT32_MAX) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
                          ASYNC_STAGE_PIN, frame->status, UINT32_MAX,
                          frame->pinCount, frame->generation);
    }
    frame->pinCount++;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Unpin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->pinCount == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_PIN_UNDERFLOW,
                          ASYNC_STAGE_PIN, frame->status, 1u, 0u,
                          frame->generation);
    }
    frame->pinCount--;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_RequestCancel(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status == ZR_ASYNC_FRAME_STATUS_TORN_DOWN ||
        frame->status == ZR_ASYNC_FRAME_STATUS_COMPLETED ||
        frame->status == ZR_ASYNC_FRAME_STATUS_CANCELLED ||
        frame->status == ZR_ASYNC_FRAME_STATUS_FAULTED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_POLL, frame->status,
                          ZR_ASYNC_FRAME_STATUS_RUNNING, frame->status,
                          frame->generation);
    }
    async_atomic_store(&frame->cancellationRequested, 1u);
    frame->pendingFlags |= ZR_ASYNC_FRAME_PENDING_CANCEL;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Resume(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status != ZR_ASYNC_FRAME_STATUS_SUSPENDED) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_BEGIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_SUSPENDED, frame->status,
                          frame->generation);
    }
    if (async_atomic_load(&frame->cancellationRequested) != 0u) {
        frame->status = ZR_ASYNC_FRAME_STATUS_CANCELLED;
        return ZR_TRUE;
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_RESUMING;
    frame->suspendReason = ZR_ASYNC_FRAME_SUSPEND_NONE;
    frame->pendingFlags &= ~ZR_ASYNC_FRAME_PENDING_BUDGET;
    frame->status = ZR_ASYNC_FRAME_STATUS_RUNNING;
    return ZR_TRUE;
}

TZrBool ZrCore_AsyncFrameBudget_Teardown(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (!ZrCore_AsyncFrameBudget_Validate(frame, diagnostic)) {
        return ZR_FALSE;
    }
    if (frame->status == ZR_ASYNC_FRAME_STATUS_TORN_DOWN) {
        return ZR_TRUE;
    }
    if (frame->status == ZR_ASYNC_FRAME_STATUS_RUNNING ||
        frame->status == ZR_ASYNC_FRAME_STATUS_RESUMING) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                          ASYNC_STAGE_BEGIN, frame->status,
                          ZR_ASYNC_FRAME_STATUS_SUSPENDED, frame->status,
                          frame->generation);
    }
    if (frame->pinCount != 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN,
                          ASYNC_STAGE_PIN, frame->status, 0u,
                          frame->pinCount, frame->generation);
    }
    frame->status = ZR_ASYNC_FRAME_STATUS_TORN_DOWN;
    frame->pendingFlags = 0u;
    async_atomic_store(&frame->cancellationRequested, 0u);
    return ZR_TRUE;
}

static SZrAsyncWaitSlot *async_wait_slot(
        const SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrAsyncWaitSlot *slot;

    if (handle == ZR_NULL || handle->registry == ZR_NULL ||
        handle->registry->slots == ZR_NULL ||
        handle->slotIndex >= handle->registry->capacity) {
        async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
                   ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_FREE, 1u, 0u, 0u);
        return ZR_NULL;
    }
    slot = &handle->registry->slots[handle->slotIndex];
    if (slot->token != handle->token || slot->generation != handle->generation ||
        async_atomic_load(&slot->state) == ZR_ASYNC_WAIT_STATE_FREE) {
        async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
                   ASYNC_STAGE_WAIT, async_atomic_load(&slot->state),
                   handle->token, slot->token, handle->generation);
        return ZR_NULL;
    }
    return slot;
}

TZrBool ZrCore_AsyncWaitRegistry_Init(
        SZrAsyncWaitRegistry *registry,
        SZrAsyncWaitSlot *slots,
        TZrUInt32 capacity,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrUInt32 index;

    async_diag_clear(diagnostic);
    if (registry == ZR_NULL || slots == ZR_NULL || capacity == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_WAIT, 0u, 1u, 0u, 0u);
    }
    memset(registry, 0, sizeof(*registry));
    registry->slots = slots;
    registry->capacity = capacity;
    registry->nextToken = 1u;
    for (index = 0u; index < capacity; index++) {
        memset(&slots[index], 0, sizeof(slots[index]));
        async_atomic_store(&slots[index].state, ZR_ASYNC_WAIT_STATE_FREE);
    }
    return ZR_TRUE;
}

void ZrCore_AsyncWaitRegistry_Deinit(SZrAsyncWaitRegistry *registry) {
    TZrUInt32 index;

    if (registry == ZR_NULL) {
        return;
    }
    async_lock(&registry->lock);
    if (registry->slots != ZR_NULL) {
        for (index = 0u; index < registry->capacity; index++) {
            registry->slots[index].token = 0u;
            registry->slots[index].frameId = 0u;
            registry->slots[index].generation = 0u;
            registry->slots[index].deadlineMicros = 0u;
            async_atomic_store(&registry->slots[index].resumeCount, 0u);
            async_atomic_store(&registry->slots[index].state,
                               ZR_ASYNC_WAIT_STATE_FREE);
        }
    }
    registry->slots = ZR_NULL;
    registry->capacity = 0u;
    registry->nextToken = 0u;
    async_unlock(&registry->lock);
}

static TZrBool async_wait_begin_internal(
        SZrAsyncWaitRegistry *registry,
        const SZrAsyncWaitRequest *request,
        SZrAsyncWaitHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 token;
    SZrAsyncWaitSlot *slot = ZR_NULL;
    TZrUInt32 expected;

    async_diag_clear(diagnostic);
    if (registry == ZR_NULL || request == ZR_NULL || outHandle == ZR_NULL ||
        registry->slots == ZR_NULL || registry->capacity == 0u) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_WAIT, 0u, 1u, 0u, 0u);
    }
    if (request->schemaVersion != ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
                          ASYNC_STAGE_WAIT, 0u,
                          ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION,
                          request->schemaVersion, request->generation);
    }
    if (request->frameId == 0u || request->generation == 0u ||
        !request->allowSuspend) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_WAIT, 0u, 1u,
                          request->frameId == 0u ? request->frameId :
                              (request->allowSuspend ? request->generation : 0u),
                          request->generation);
    }
    if (request->holdsBorrow) {
        if (diagnostic != ZR_NULL) {
            diagnostic->sourceId = request->sourceId;
            diagnostic->instructionId = request->instructionId;
        }
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND,
                          ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_REGISTERING,
                          0u, 1u, request->generation);
    }
    if (request->holdsStackAlias) {
        if (diagnostic != ZR_NULL) {
            diagnostic->sourceId = request->sourceId;
            diagnostic->instructionId = request->instructionId;
        }
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND,
                          ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_REGISTERING,
                          0u, 1u, request->generation);
    }
    if (request->holdsLockGuard) {
        if (diagnostic != ZR_NULL) {
            diagnostic->sourceId = request->sourceId;
            diagnostic->instructionId = request->instructionId;
        }
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND,
                          ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_REGISTERING,
                          0u, 1u, request->generation);
    }
    if (request->inNativeCritical) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL,
                          ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_REGISTERING,
                          0u, 1u, request->generation);
    }

    async_lock(&registry->lock);
    for (index = 0u; index < registry->capacity; index++) {
        if (async_atomic_load(&registry->slots[index].state) ==
            ZR_ASYNC_WAIT_STATE_FREE) {
            slot = &registry->slots[index];
            break;
        }
    }
    if (slot == ZR_NULL) {
        async_unlock(&registry->lock);
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_CAPACITY,
                          ASYNC_STAGE_WAIT, 0u, registry->capacity,
                          registry->capacity, request->generation);
    }
    token = registry->nextToken;
    if (token == 0u) {
        async_unlock(&registry->lock);
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
                          ASYNC_STAGE_WAIT, ZR_ASYNC_WAIT_STATE_FREE,
                          UINT64_MAX, registry->nextToken,
                          request->generation);
    }
    registry->nextToken = token == UINT64_MAX ? 0u : token + 1u;
    slot->token = token;
    slot->frameId = request->frameId;
    slot->generation = request->generation;
    slot->deadlineMicros = request->deadlineMicros;
    async_atomic_store(&slot->resumeCount, 0u);
    async_atomic_store(&slot->state, ZR_ASYNC_WAIT_STATE_REGISTERING);
    outHandle->registry = registry;
    outHandle->slotIndex = index;
    outHandle->token = token;
    outHandle->generation = request->generation;
    async_unlock(&registry->lock);

    /* Publish WAITING only if no wake/cancel/timeout won the registration
     * race.  A wake can therefore happen between registration and recheck. */
    expected = ZR_ASYNC_WAIT_STATE_REGISTERING;
    (void)async_atomic_cas(&slot->state, &expected,
                           request->conditionReady ? ZR_ASYNC_WAIT_STATE_READY
                                                    : ZR_ASYNC_WAIT_STATE_WAITING);
    if (request->conditionReady &&
        async_atomic_load(&slot->state) == ZR_ASYNC_WAIT_STATE_WAITING) {
        expected = ZR_ASYNC_WAIT_STATE_WAITING;
        (void)async_atomic_cas(&slot->state, &expected,
                               ZR_ASYNC_WAIT_STATE_READY);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Execution_BeginAsyncWait(
        const SZrAsyncWaitRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (request == ZR_NULL) {
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                          ASYNC_STAGE_WAIT, 0u, 1u, 0u, 0u);
    }
    return async_wait_begin_internal(request->registry, request,
                                     request->outHandle, diagnostic);
}

TZrBool ZrCore_AsyncWait_Begin(
        SZrAsyncWaitRegistry *registry,
        const SZrAsyncWaitRequest *request,
        SZrAsyncWaitHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    return async_wait_begin_internal(registry, request, outHandle, diagnostic);
}

static TZrBool async_wait_win(SZrAsyncWaitHandle *handle,
                              EZrAsyncWaitState terminal,
                              SZrAsyncFrameDiagnostic *diagnostic) {
    SZrAsyncWaitSlot *slot;
    TZrUInt32 expected;

    async_diag_clear(diagnostic);
    slot = async_wait_slot(handle, diagnostic);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    expected = ZR_ASYNC_WAIT_STATE_REGISTERING;
    if (async_atomic_cas(&slot->state, &expected, terminal)) {
        return ZR_TRUE;
    }
    expected = ZR_ASYNC_WAIT_STATE_WAITING;
    if (async_atomic_cas(&slot->state, &expected, terminal)) {
        return ZR_TRUE;
    }
    if (expected == ZR_ASYNC_WAIT_STATE_RESUMED) {
        return async_fail(diagnostic,
                          ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED,
                          ASYNC_STAGE_WAIT, expected, 0u, expected,
                          handle->generation);
    }
    return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
                      ASYNC_STAGE_WAIT, expected, ZR_ASYNC_WAIT_STATE_WAITING,
                      expected, handle->generation);
}

TZrBool ZrCore_AsyncWait_Recheck(
        SZrAsyncWaitHandle *handle,
        TZrBool conditionReady,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrAsyncWaitSlot *slot;
    TZrUInt32 expected;

    async_diag_clear(diagnostic);
    slot = async_wait_slot(handle, diagnostic);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!conditionReady) {
        return ZR_TRUE;
    }
    expected = ZR_ASYNC_WAIT_STATE_REGISTERING;
    if (async_atomic_cas(&slot->state, &expected, ZR_ASYNC_WAIT_STATE_READY)) {
        return ZR_TRUE;
    }
    expected = ZR_ASYNC_WAIT_STATE_WAITING;
    if (async_atomic_cas(&slot->state, &expected, ZR_ASYNC_WAIT_STATE_READY)) {
        return ZR_TRUE;
    }
    /* A terminal winner is already the desired outcome; this is the key to
     * the register/recheck no-lost-wakeup rule. */
    if (expected == ZR_ASYNC_WAIT_STATE_READY ||
        expected == ZR_ASYNC_WAIT_STATE_CANCELLED ||
        expected == ZR_ASYNC_WAIT_STATE_TIMED_OUT ||
        expected == ZR_ASYNC_WAIT_STATE_RESUMED) {
        return ZR_TRUE;
    }
    return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
                      ASYNC_STAGE_WAIT, expected, ZR_ASYNC_WAIT_STATE_WAITING,
                      expected, handle->generation);
}

TZrBool ZrCore_AsyncWait_Wake(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    return async_wait_win(handle, ZR_ASYNC_WAIT_STATE_READY, diagnostic);
}

TZrBool ZrCore_AsyncWait_Cancel(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    return async_wait_win(handle, ZR_ASYNC_WAIT_STATE_CANCELLED, diagnostic);
}

TZrBool ZrCore_AsyncWait_Timeout(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    return async_wait_win(handle, ZR_ASYNC_WAIT_STATE_TIMED_OUT, diagnostic);
}

TZrBool ZrCore_AsyncWait_Resume(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrAsyncWaitSlot *slot;
    TZrUInt32 expected;

    async_diag_clear(diagnostic);
    slot = async_wait_slot(handle, diagnostic);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    for (;;) {
        expected = async_atomic_load(&slot->state);
        if (expected == ZR_ASYNC_WAIT_STATE_RESUMED) {
            return async_fail(diagnostic,
                              ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED,
                              ASYNC_STAGE_WAIT, expected, 0u, expected,
                              handle->generation);
        }
        if (expected != ZR_ASYNC_WAIT_STATE_READY &&
            expected != ZR_ASYNC_WAIT_STATE_CANCELLED &&
            expected != ZR_ASYNC_WAIT_STATE_TIMED_OUT) {
            return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
                              ASYNC_STAGE_WAIT, expected,
                              ZR_ASYNC_WAIT_STATE_READY, expected,
                              handle->generation);
        }
        if (async_atomic_cas(&slot->state, &expected,
                             ZR_ASYNC_WAIT_STATE_RESUMED)) {
            TZrUInt32 count = async_atomic_load(&slot->resumeCount);
            if (count != UINT32_MAX) {
                async_atomic_store(&slot->resumeCount, count + 1u);
            }
            return ZR_TRUE;
        }
    }
}

TZrBool ZrCore_AsyncWait_Release(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrAsyncWaitSlot *slot;
    TZrUInt32 expected;

    async_diag_clear(diagnostic);
    slot = async_wait_slot(handle, diagnostic);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    /* Keep the slot reserved until its identity fields are cleared.  Marking
     * FREE before taking the registry lock would let a concurrent Begin reuse
     * the slot while this release still clears it. */
    async_lock(&handle->registry->lock);
    if (slot->token != handle->token ||
        slot->generation != handle->generation) {
        async_unlock(&handle->registry->lock);
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
                          ASYNC_STAGE_WAIT, async_atomic_load(&slot->state),
                          handle->token, slot->token, handle->generation);
    }
    expected = ZR_ASYNC_WAIT_STATE_RESUMED;
    if (!async_atomic_cas(&slot->state, &expected, ZR_ASYNC_WAIT_STATE_FREE)) {
        async_unlock(&handle->registry->lock);
        return async_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
                          ASYNC_STAGE_WAIT, expected,
                          ZR_ASYNC_WAIT_STATE_RESUMED, expected,
                          handle->generation);
    }
    slot->token = 0u;
    slot->frameId = 0u;
    slot->generation = 0u;
    slot->deadlineMicros = 0u;
    async_atomic_store(&slot->resumeCount, 0u);
    async_unlock(&handle->registry->lock);
    memset(handle, 0, sizeof(*handle));
    return ZR_TRUE;
}

EZrAsyncWaitState ZrCore_AsyncWait_State(
        const SZrAsyncWaitHandle *handle) {
    SZrAsyncWaitSlot *slot = async_wait_slot(handle, ZR_NULL);
    return slot == ZR_NULL
               ? ZR_ASYNC_WAIT_STATE_FREE
               : (EZrAsyncWaitState)async_atomic_load(&slot->state);
}

TZrUInt32 ZrCore_AsyncWait_ResumeCount(
        const SZrAsyncWaitHandle *handle) {
    SZrAsyncWaitSlot *slot = async_wait_slot(handle, ZR_NULL);
    return slot == ZR_NULL ? 0u : async_atomic_load(&slot->resumeCount);
}
