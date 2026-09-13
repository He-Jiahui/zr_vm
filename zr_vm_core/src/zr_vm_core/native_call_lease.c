#include "zr_vm_core/native_call_contract.h"

#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include <stdint.h>
#include <string.h>

static void native_call_lease_set_diagnostic(
        SZrNativeCallDiagnostic *diagnostic,
        EZrNativeCallStatus status,
        EZrNativeCallPhase phase,
        TZrUInt32 parameterIndex,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    TZrUInt64 sourceId;

    if (diagnostic == ZR_NULL) {
        return;
    }
    sourceId = diagnostic->sourceId;
    diagnostic->status = status;
    diagnostic->phase = phase;
    diagnostic->parameterIndex = parameterIndex;
    diagnostic->operationIndex = parameterIndex;
    diagnostic->sourceId = sourceId;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

TZrBool ZrCore_NativeCall_LeaseBegin(
        SZrState *state,
        const SZrNativeCallPlan *plan,
        SZrNativeCallLease *lease,
        SZrNativeCallDiagnostic *diagnostic) {
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (diagnostic != ZR_NULL) {
        if (plan != ZR_NULL) {
            diagnostic->sourceId = plan->sourceId;
        } else if (lease != ZR_NULL) {
            diagnostic->sourceId = lease->sourceId;
        }
    }
    if (state == ZR_NULL || plan == ZR_NULL || lease == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    /* Beginning a second lease over an active record would orphan its pins
     * and roots.  Leave the original lease untouched so its owner can end it
     * and retry later. */
    if (lease->active == ZR_TRUE) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ALREADY_ACTIVE,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 1u);
        return ZR_FALSE;
    }
    if (lease->active > ZR_TRUE) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, ZR_TRUE,
                lease->active);
        return ZR_FALSE;
    }
    if (lease->nativeEntered > ZR_TRUE || lease->attachedByCall > ZR_TRUE ||
        lease->reserved0 != 0u) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u,
                ((TZrUInt64)lease->nativeEntered << 16u) |
                        ((TZrUInt64)lease->attachedByCall << 8u) |
                        lease->reserved0);
        return ZR_FALSE;
    }
    if (lease->active == ZR_FALSE &&
        (lease->state != ZR_NULL || lease->pinCount != 0u ||
         lease->rootCount != 0u || lease->sourceId != 0u ||
         lease->pinObligationCount != 0u ||
         lease->rootObligationCount != 0u ||
         lease->requiredPinCount != 0u || lease->requiredRootCount != 0u)) {
        /* Reuse is supported only after LeaseEnd has cleared the record.  A
         * non-zero inactive record could still own handles; refusing to
         * overwrite it avoids leaking those resources. */
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_VALIDATE, UINT32_MAX, 0u, 1u);
        return ZR_FALSE;
    }
    if (!ZrCore_NativeCall_ValidatePlan(plan, diagnostic)) {
        return ZR_FALSE;
    }
    memset(lease, 0, sizeof(*lease));
    if (state->gcDomain == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ATTACH_REQUIRED,
                ZR_NATIVE_CALL_PHASE_ATTACH, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_NativeEnter(state, plan->nativeSafepointMode)) {
        native_call_lease_set_diagnostic(
                diagnostic,
                state->threadStatus == ZR_THREAD_STATUS_EXECUTION_TERMINATED
                        ? ZR_NATIVE_CALL_STATUS_BUDGET
                        : ZR_NATIVE_CALL_STATUS_NATIVE_ENTER_FAILED,
                ZR_NATIVE_CALL_PHASE_ATTACH, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    lease->state = state;
    lease->nativeSafepointMode = plan->nativeSafepointMode;
    lease->sourceId = plan->sourceId;
    lease->requiredPinCount = plan->pinCount;
    lease->requiredRootCount = plan->rootCount;
    lease->nativeEntered = ZR_TRUE;
    lease->active = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCall_LeasePinValue(
        SZrNativeCallLease *lease,
        const SZrTypeValue *value,
        SZrNativeCallDiagnostic *diagnostic) {
    SZrRawObject *object;

    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (diagnostic != ZR_NULL && lease != ZR_NULL) {
        diagnostic->sourceId = lease->sourceId;
    }
    if (lease == ZR_NULL || !lease->active || lease->state == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NOT_ACTIVE,
                ZR_NATIVE_CALL_PHASE_PIN, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (lease->pinObligationCount >= ZR_NATIVE_CALL_MAX_PINNED_VALUES) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, lease->pinObligationCount,
                ZR_NATIVE_CALL_MAX_PINNED_VALUES,
                lease->pinObligationCount);
        return ZR_FALSE;
    }
    /* A nullable/non-managed value has no pin record; treating it as a
     * successful no-op keeps cleanup symmetric and mirrors the core GC pin
     * helper's non-GC behavior. */
    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        lease->pinObligationCount++;
        return ZR_TRUE;
    }
    object = ZrCore_Value_GetRawObject(value);
    if (object != ZR_NULL &&
        !ZrCore_GcDomain_ObjectBelongsToState(lease->state, object)) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, UINT32_MAX, 1u, 0u);
        return ZR_FALSE;
    }
    if (lease->pinCount >= ZR_NATIVE_CALL_MAX_PINNED_VALUES ||
        !ZrCore_Gc_NativeCallPinValue(
                lease->state, value, &lease->pins[lease->pinCount])) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, lease->pinCount, 0u, 0u);
        return ZR_FALSE;
    }
    lease->pinCount++;
    lease->pinObligationCount++;
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCall_LeasePinObject(
        SZrNativeCallLease *lease,
        SZrRawObject *object,
        SZrNativeCallDiagnostic *diagnostic) {
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (diagnostic != ZR_NULL && lease != ZR_NULL) {
        diagnostic->sourceId = lease->sourceId;
    }
    if (lease == ZR_NULL || !lease->active || lease->state == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NOT_ACTIVE,
                ZR_NATIVE_CALL_PHASE_PIN, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (lease->pinObligationCount >= ZR_NATIVE_CALL_MAX_PINNED_VALUES) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, lease->pinObligationCount,
                ZR_NATIVE_CALL_MAX_PINNED_VALUES,
                lease->pinObligationCount);
        return ZR_FALSE;
    }
    /* The underlying GC pin helper treats a null object as a successful
     * no-op.  Keep that property here so nullable callback/view lanes can
     * share the same cleanup path without manufacturing a fake pin. */
    if (object == ZR_NULL) {
        lease->pinObligationCount++;
        return ZR_TRUE;
    }
    if (!ZrCore_GcDomain_ObjectBelongsToState(lease->state, object)) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, UINT32_MAX, 1u, 0u);
        return ZR_FALSE;
    }
    if (lease->pinCount >= ZR_NATIVE_CALL_MAX_PINNED_VALUES ||
        !ZrCore_Gc_NativeCallPinObject(
                lease->state, object, &lease->pins[lease->pinCount])) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_PIN_FAILED,
                ZR_NATIVE_CALL_PHASE_PIN, lease->pinCount, 0u, 0u);
        return ZR_FALSE;
    }
    lease->pinCount++;
    lease->pinObligationCount++;
    return ZR_TRUE;
}

TZrBool ZrCore_NativeCall_LeaseRootObject(
        SZrNativeCallLease *lease,
        SZrRawObject *object,
        SZrNativeCallDiagnostic *diagnostic) {
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (diagnostic != ZR_NULL && lease != ZR_NULL) {
        diagnostic->sourceId = lease->sourceId;
    }
    if (lease == ZR_NULL || !lease->active || lease->state == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_NOT_ACTIVE,
                ZR_NATIVE_CALL_PHASE_ROOT, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (lease->rootObligationCount >= ZR_NATIVE_CALL_MAX_ROOTS) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ROOT_FAILED,
                ZR_NATIVE_CALL_PHASE_ROOT, lease->rootObligationCount,
                ZR_NATIVE_CALL_MAX_ROOTS,
                lease->rootObligationCount);
        return ZR_FALSE;
    }
    if (object == ZR_NULL) {
        /* Optional callback/view arguments may have a null owner.  A null
         * root is a successful no-op, but still acknowledges the root
         * obligation so InvokeResolved can distinguish it from an invoker
         * that forgot to visit the operation entirely. */
        lease->rootObligationCount++;
        return ZR_TRUE;
    }
    if (!ZrCore_GcDomain_ObjectBelongsToState(lease->state, object)) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ROOT_FAILED,
                ZR_NATIVE_CALL_PHASE_ROOT, UINT32_MAX, 1u, 0u);
        return ZR_FALSE;
    }
    if (lease->rootCount >= ZR_NATIVE_CALL_MAX_ROOTS ||
        !ZrCore_GcRootHandle_Create(
                lease->state, object, &lease->roots[lease->rootCount])) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ROOT_FAILED,
                ZR_NATIVE_CALL_PHASE_ROOT, lease->rootCount, 0u, 0u);
        return ZR_FALSE;
    }
    lease->rootCount++;
    lease->rootObligationCount++;
    return ZR_TRUE;
}

void ZrCore_NativeCall_LeaseEnd(
        SZrNativeCallLease *lease,
        SZrNativeCallDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 rootCount;
    TZrUInt32 pinCount;
    TZrUInt64 sourceId = 0u;

    if (diagnostic != ZR_NULL) {
        if (lease != ZR_NULL) {
            sourceId = lease->sourceId;
        }
        ZrCore_NativeCall_DiagnosticClear(diagnostic);
        diagnostic->sourceId = sourceId;
    }
    if (lease == ZR_NULL) {
        return;
    }
    /* A lease is a boolean state machine.  If a caller hands cleanup a
     * partially initialized record with an out-of-range active byte, do not
     * interpret arbitrary counters/handles as live resources.  Clearing the
     * record makes the failure recoverable and avoids an unsafe release walk. */
    if (lease->active != ZR_TRUE) {
        memset(lease, 0, sizeof(*lease));
        return;
    }
    /* Lease records normally come from LeaseBegin, but cleanup is also used
     * on partial/error paths.  Clamp corrupted counters before indexing the
     * fixed arrays so a malformed or interrupted record cannot turn cleanup
     * into an out-of-bounds walk. */
    rootCount = lease->rootCount > ZR_NATIVE_CALL_MAX_ROOTS
                        ? ZR_NATIVE_CALL_MAX_ROOTS
                        : lease->rootCount;
    pinCount = lease->pinCount > ZR_NATIVE_CALL_MAX_PINNED_VALUES
                       ? ZR_NATIVE_CALL_MAX_PINNED_VALUES
                       : lease->pinCount;
    if (lease->state != ZR_NULL) {
        for (index = rootCount; index > 0u; index--) {
            ZrCore_GcRootHandle_Release(
                    lease->state, &lease->roots[index - 1u]);
        }
        /* ZrCore_Gc_NativeCallUnpin deliberately accepts a null global: it
         * still clears the per-pin mark/record and only skips the optional
         * global unignore bookkeeping.  Run it for every concrete record even
         * on a partially torn-down state so a late cleanup cannot leave a
         * managed object permanently marked as native-pinned. */
        for (index = pinCount; index > 0u; index--) {
            ZrCore_Gc_NativeCallUnpin(
                    lease->state->global, &lease->pins[index - 1u]);
        }
        if (lease->nativeEntered == ZR_TRUE) {
            ZrCore_GcDomain_NativeLeave(lease->state);
        }
    }
    memset(lease, 0, sizeof(*lease));
}

TZrBool ZrCore_NativeCall_Attach(
        SZrState *ownerState,
        SZrState *mutatorState,
        SZrNativeCallDiagnostic *diagnostic) {
    ZrCore_NativeCall_DiagnosticClear(diagnostic);
    if (ownerState == ZR_NULL || mutatorState == ZR_NULL) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_INVALID_ARGUMENT,
                ZR_NATIVE_CALL_PHASE_ATTACH, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_MutatorAttach(ownerState, mutatorState)) {
        native_call_lease_set_diagnostic(
                diagnostic, ZR_NATIVE_CALL_STATUS_ATTACH_FAILED,
                ZR_NATIVE_CALL_PHASE_ATTACH, UINT32_MAX, 0u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrCore_NativeCall_Detach(SZrState *mutatorState) {
    if (mutatorState != ZR_NULL) {
        ZrCore_GcDomain_MutatorDetach(mutatorState);
    }
}
