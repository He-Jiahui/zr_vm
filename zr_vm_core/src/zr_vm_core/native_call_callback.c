#include "zr_vm_core/native_call_contract.h"

#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
#include <windows.h>
#endif

/* A malformed or abandoned slot must not make every caller spin forever.
 * Public operations fail closed after this bounded hand-off window; normal
 * callback transitions hold the lock for only a few atomic stores. */
#define ZR_NATIVE_CALLBACK_LOCK_SPIN_LIMIT ((TZrUInt32)1000000u)

static void native_callback_set_diagnostic(
        SZrNativeCallDiagnostic *diagnostic,
        EZrNativeCallStatus status,
        EZrNativeCallPhase phase,
        TZrUInt32 parameterIndex,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->status = status;
    diagnostic->phase = phase;
    diagnostic->parameterIndex = parameterIndex;
    diagnostic->operationIndex = parameterIndex;
    diagnostic->sourceId = 0u;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrUInt32 native_callback_load(const volatile TZrUInt32 *value) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    return (TZrUInt32)InterlockedCompareExchange(
            (volatile LONG *)value, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#else
    return *value;
#endif
}

static void native_callback_store(volatile TZrUInt32 *value, TZrUInt32 next) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#else
    *value = next;
#endif
}

static TZrUInt64 native_callback_load64(const volatile TZrUInt64 *value) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    return (TZrUInt64)InterlockedCompareExchange64(
            (volatile LONGLONG *)value, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#else
    return *value;
#endif
}

static void native_callback_store64(volatile TZrUInt64 *value, TZrUInt64 next) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    (void)InterlockedExchange64((volatile LONGLONG *)value, (LONGLONG)next);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#else
    *value = next;
#endif
}

static TZrBool native_callback_compare_exchange(
        volatile TZrUInt32 *value,
        TZrUInt32 *expected,
        TZrUInt32 desired) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    LONG previous = InterlockedCompareExchange(
            (volatile LONG *)value, (LONG)desired, (LONG)*expected);
    if ((TZrUInt32)previous == *expected) {
        return ZR_TRUE;
    }
    *expected = (TZrUInt32)previous;
    return ZR_FALSE;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(value, expected, desired, 0,
                                       __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#else
    if (*value == *expected) {
        *value = desired;
        return ZR_TRUE;
    }
    *expected = *value;
    return ZR_FALSE;
#endif
}

static TZrUInt32 native_callback_increment(volatile TZrUInt32 *value) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    return (TZrUInt32)InterlockedIncrement((volatile LONG *)value);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
#else
    return ++(*value);
#endif
}

static TZrUInt32 native_callback_decrement(volatile TZrUInt32 *value) {
#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    return (TZrUInt32)InterlockedDecrement((volatile LONG *)value);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_sub_fetch(value, 1u, __ATOMIC_ACQ_REL);
#else
    return --(*value);
#endif
}

/* Callback slots are tiny process-local records, so a short spin lock keeps
 * the state/id/refcount transition atomic without storing a pointer to a
 * platform mutex in the cacheable ABI descriptor. */
static TZrBool native_callback_lock(SZrNativeCallbackSlot *slot) {
    TZrUInt32 expected;
    TZrUInt32 spin;

    for (spin = 0u; spin < ZR_NATIVE_CALLBACK_LOCK_SPIN_LIMIT; spin++) {
        expected = 0u;
        if (native_callback_compare_exchange(&slot->lock, &expected, 1u)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void native_callback_unlock(SZrNativeCallbackSlot *slot) {
    native_callback_store(&slot->lock, 0u);
}

TZrBool ZrCore_NativeCallback_Register(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        TZrUInt64 generation,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 state;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (slot == ZR_NULL || callbackId == 0u ||
        (slot != ZR_NULL && slot->reserved0 != 0u)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, callbackId);
        return ZR_FALSE;
    }
    if (!native_callback_lock(slot)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                native_callback_load(&slot->lock));
        return ZR_FALSE;
    }
    state = native_callback_load(&slot->state);
    if (state == ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED &&
        (native_callback_load(&slot->inFlight) != 0u ||
         native_callback_load64(&slot->callbackId) != 0u ||
         native_callback_load64(&slot->generation) != 0u)) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 1u);
        return ZR_FALSE;
    }
    if (state != ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic,
                state == ZR_NATIVE_CALLBACK_SLOT_CLOSING
                        ? ZR_NATIVE_CALL_STATUS_CALLBACK_ALREADY_CLOSING
                        : ZR_NATIVE_CALL_STATUS_ALREADY_ACTIVE,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX,
                ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED, state);
        return ZR_FALSE;
    }
    native_callback_store(&slot->inFlight, 0u);
    native_callback_store64(&slot->callbackId, callbackId);
    native_callback_store64(&slot->generation, generation);
    native_callback_store(&slot->state, ZR_NATIVE_CALLBACK_SLOT_ACTIVE);
    native_callback_unlock(slot);
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCallback_Enter(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt64 activeId;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (slot == ZR_NULL || callbackId == 0u ||
        (slot != ZR_NULL && slot->reserved0 != 0u)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_INVOKE, UINT32_MAX, callbackId, 0u);
        return ZR_FALSE;
    }
    if (!native_callback_lock(slot)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
                ZR_NATIVE_CALL_PHASE_INVOKE, UINT32_MAX, 0u,
                native_callback_load(&slot->lock));
        return ZR_FALSE;
    }
    activeId = native_callback_load64(&slot->callbackId);
    if (native_callback_load(&slot->state) != ZR_NATIVE_CALLBACK_SLOT_ACTIVE ||
        activeId != callbackId) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_REJECTED,
                ZR_NATIVE_CALL_PHASE_INVOKE, UINT32_MAX, callbackId, activeId);
        return ZR_FALSE;
    }
    if (native_callback_load(&slot->inFlight) == ~(TZrUInt32)0u) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_REJECTED,
                ZR_NATIVE_CALL_PHASE_INVOKE, UINT32_MAX,
                ~(TZrUInt32)0u, ~(TZrUInt32)0u);
        return ZR_FALSE;
    }
    (void)native_callback_increment(&slot->inFlight);
    native_callback_unlock(slot);
    return ZR_TRUE;
}

void ZrCore_NativeCallback_Leave(SZrNativeCallbackSlot *slot) {
    if (slot == ZR_NULL || slot->reserved0 != 0u) {
        return;
    }
    if (!native_callback_lock(slot)) {
        return;
    }
    if (native_callback_load(&slot->inFlight) != 0u) {
        (void)native_callback_decrement(&slot->inFlight);
    }
    native_callback_unlock(slot);
}

TZrBool ZrCore_NativeCallback_BeginUnregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 state;
    TZrUInt64 activeId;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (slot == ZR_NULL || callbackId == 0u ||
        (slot != ZR_NULL && slot->reserved0 != 0u)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, callbackId, 0u);
        return ZR_FALSE;
    }
    if (!native_callback_lock(slot)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, 0u,
                native_callback_load(&slot->lock));
        return ZR_FALSE;
    }
    state = native_callback_load(&slot->state);
    activeId = native_callback_load64(&slot->callbackId);
    if (state != ZR_NATIVE_CALLBACK_SLOT_ACTIVE || activeId != callbackId) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic,
                state == ZR_NATIVE_CALLBACK_SLOT_CLOSING
                        ? ZR_NATIVE_CALL_STATUS_CALLBACK_ALREADY_CLOSING
                        : ZR_NATIVE_CALL_STATUS_CALLBACK_NOT_REGISTERED,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, callbackId, activeId);
        return ZR_FALSE;
    }
    native_callback_store(&slot->state, ZR_NATIVE_CALLBACK_SLOT_CLOSING);
    native_callback_unlock(slot);
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCallback_FinishUnregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 expected = ZR_NATIVE_CALLBACK_SLOT_CLOSING;
    TZrUInt32 state;
    TZrUInt64 activeId;
    TZrUInt32 activeCount;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (slot == ZR_NULL || callbackId == 0u ||
        (slot != ZR_NULL && slot->reserved0 != 0u)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, callbackId, 0u);
        return ZR_FALSE;
    }
    if (!native_callback_lock(slot)) {
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, 0u,
                native_callback_load(&slot->lock));
        return ZR_FALSE;
    }
    state = native_callback_load(&slot->state);
    activeId = native_callback_load64(&slot->callbackId);
    activeCount = native_callback_load(&slot->inFlight);
    if (state != ZR_NATIVE_CALLBACK_SLOT_CLOSING || activeId != callbackId) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_NOT_REGISTERED,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, callbackId, activeId);
        return ZR_FALSE;
    }
    if (activeCount != 0u) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, 0u, activeCount);
        return ZR_FALSE;
    }
    /* Clear identity while CLOSING, before making the slot available to a
     * subsequent Register call. */
    native_callback_store64(&slot->callbackId, 0u);
    native_callback_store64(&slot->generation, 0u);
    if (!native_callback_compare_exchange(&slot->state, &expected,
                                          ZR_NATIVE_CALLBACK_SLOT_UNREGISTERED)) {
        native_callback_unlock(slot);
        native_callback_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_NOT_REGISTERED,
                ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, 0u, expected);
        return ZR_FALSE;
    }
    native_callback_unlock(slot);
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCallback_Unregister(
        SZrNativeCallbackSlot *slot,
        TZrUInt64 callbackId,
        TZrUInt32 maxSpins,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 spin;

    /* Keep a caller-provided unbounded wait from turning a stale in-flight
     * count into an uninterruptible native thread.  A later FinishUnregister
     * call can complete the same CLOSING slot after the owner leaves. */
    if (maxSpins > ZR_NATIVE_CALLBACK_LOCK_SPIN_LIMIT) {
        maxSpins = ZR_NATIVE_CALLBACK_LOCK_SPIN_LIMIT;
    }

    if (!ZrCore_NativeCallback_BeginUnregister(slot, callbackId, diagnostic)) {
        return ZR_FALSE;
    }
    if (native_callback_load(&slot->inFlight) == 0u) {
        return ZrCore_NativeCallback_FinishUnregister(slot, callbackId, diagnostic);
    }
    for (spin = 0u; spin < maxSpins; spin++) {
        if (native_callback_load(&slot->inFlight) == 0u) {
            return ZrCore_NativeCallback_FinishUnregister(slot, callbackId, diagnostic);
        }
    }
    native_callback_set_diagnostic(
            diagnostic, ZR_NATIVE_CALL_STATUS_CALLBACK_IN_FLIGHT,
            ZR_NATIVE_CALL_PHASE_CLEANUP, UINT32_MAX, 0u,
            native_callback_load(&slot->inFlight));
    return ZR_FALSE;
}
