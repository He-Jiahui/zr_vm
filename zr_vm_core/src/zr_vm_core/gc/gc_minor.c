#include "zr_vm_core/gc_young_allocation.h"

#include <string.h>

static void gc_young_set_diagnostic_minor(
        SZrGcYoungDiagnostic *diagnostic,
        EZrGcYoungDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

static TZrBool gc_young_minor_phase_is_active(EZrGcMinorPhase phase) {
    return phase == ZR_GC_MINOR_PHASE_EVACUATE ||
           phase == ZR_GC_MINOR_PHASE_REWRITE_REFERENCES ||
           phase == ZR_GC_MINOR_PHASE_VERIFY ||
           phase == ZR_GC_MINOR_PHASE_RESUME;
}

void ZrCore_GcMinorTransaction_Init(
        SZrGcMinorTransaction *transaction) {
    if (transaction != ZR_NULL) {
        memset(transaction, 0, sizeof(*transaction));
        transaction->phase = ZR_GC_MINOR_PHASE_IDLE;
        transaction->consistentBoundary = ZR_TRUE;
    }
}

TZrBool ZrCore_GcMinorTransaction_Begin(
        SZrGcMinorTransaction *transaction,
        TZrUInt32 regionCount,
        TZrUInt64 toSpaceBytes,
        TZrUInt32 workBudget,
        TZrBool mutatorsStopped,
        TZrBool rootsAvailable,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL || regionCount == 0u || toSpaceBytes == 0u ||
        workBudget == 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED,
                                      4u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!rootsAvailable) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE,
                                      5u, 1u, 0u);
        return ZR_FALSE;
    }
    if (transaction->phase != ZR_GC_MINOR_PHASE_IDLE &&
        transaction->phase != ZR_GC_MINOR_PHASE_COMPLETE &&
        transaction->phase != ZR_GC_MINOR_PHASE_ABORTED) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_IDLE,
                                      (TZrUInt64)transaction->phase);
        return ZR_FALSE;
    }
    memset(transaction, 0, sizeof(*transaction));
    transaction->phase = ZR_GC_MINOR_PHASE_EVACUATE;
    transaction->regionCount = regionCount;
    transaction->toSpaceBytes = toSpaceBytes;
    transaction->mutatorsStopped = ZR_TRUE;
    transaction->rootsTraced = ZR_TRUE;
    transaction->consistentBoundary = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_EvacuateRegion(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 liveBytes,
        TZrUInt32 objectCount,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_EVACUATE ||
        !transaction->mutatorsStopped || !transaction->rootsTraced) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_EVACUATE,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    if (transaction->regionCursor >= transaction->regionCount || objectCount == 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION,
                                      1u, (TZrUInt64)transaction->regionCount,
                                      (TZrUInt64)transaction->regionCursor);
        return ZR_FALSE;
    }
    if (liveBytes > transaction->toSpaceBytes - transaction->toSpaceUsedBytes) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED,
                                      1u, transaction->toSpaceBytes -
                                              transaction->toSpaceUsedBytes,
                                      liveBytes);
        /* No cursor or byte counter changes: the from-space region remains a
         * complete, uncommitted unit and mutators stay stopped. */
        transaction->consistentBoundary = ZR_FALSE;
        return ZR_FALSE;
    }
    transaction->toSpaceUsedBytes += liveBytes;
    transaction->evacuatedBytes += liveBytes;
    transaction->promotedObjects += (TZrUInt64)objectCount;
    transaction->regionCursor++;
    transaction->consistentBoundary = ZR_FALSE;
    if (transaction->regionCursor == transaction->regionCount) {
        transaction->phase = ZR_GC_MINOR_PHASE_REWRITE_REFERENCES;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_RewriteReferences(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 rewrittenReferences,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_REWRITE_REFERENCES ||
        !transaction->mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_REWRITE_REFERENCES,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    transaction->rewrittenReferences = rewrittenReferences;
    transaction->phase = ZR_GC_MINOR_PHASE_VERIFY;
    transaction->consistentBoundary = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_VerifyForwarding(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 unresolvedForwarding,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_VERIFY ||
        !transaction->mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_VERIFY,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    transaction->unresolvedForwarding = unresolvedForwarding;
    if (unresolvedForwarding != 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING,
                                      1u, 0u, unresolvedForwarding);
        transaction->consistentBoundary = ZR_FALSE;
        return ZR_FALSE;
    }
    transaction->phase = ZR_GC_MINOR_PHASE_RESUME;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_CanResumeMutators(
        const SZrGcMinorTransaction *transaction) {
    return (TZrBool)(transaction != ZR_NULL &&
                     transaction->phase == ZR_GC_MINOR_PHASE_RESUME &&
                     transaction->mutatorsStopped && transaction->rootsTraced &&
                     transaction->unresolvedForwarding == 0u &&
                     transaction->consistentBoundary);
}

TZrBool ZrCore_GcMinorTransaction_ResumeMutators(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcMinorTransaction_CanResumeMutators(transaction)) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
                                      0u, ZR_GC_MINOR_PHASE_RESUME,
                                      (TZrUInt64)transaction->phase);
        return ZR_FALSE;
    }
    transaction->mutatorsStopped = ZR_FALSE;
    transaction->phase = ZR_GC_MINOR_PHASE_COMPLETE;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_Abort(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL || !gc_young_minor_phase_is_active(transaction->phase)) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      transaction == ZR_NULL
                                          ? ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT
                                          : ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, 1u,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    /* Aborting is a safe terminal state only while mutators remain stopped;
     * callers must restore roots or restart the transaction explicitly. */
    transaction->phase = ZR_GC_MINOR_PHASE_ABORTED;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_Gc_RunMinorTransaction(
        struct SZrState *state,
        const SZrGcMinorRequest *request,
        SZrGcMinorResult *result,
        SZrGcYoungDiagnostic *diagnostic) {
    SZrGcMinorTransaction transaction;

    ZR_UNUSED_PARAMETER(state);
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (request == ZR_NULL || result == ZR_NULL) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    memset(result, 0, sizeof(*result));
    ZrCore_GcMinorTransaction_Init(&transaction);
    if (!ZrCore_GcMinorTransaction_Begin(
                &transaction,
                request->regionCount,
                request->toSpaceBytes,
                request->workBudget,
                request->mutatorsStopped,
                request->rootsAvailable,
                diagnostic)) {
        return ZR_FALSE;
    }
    /* This façade deliberately does not invent object/card information.  It
     * records the stop/trace boundary; a collector feeds each actual region
     * through EvacuateRegion and then publishes the result. */
    result->phase = transaction.phase;
    result->regionCursor = transaction.regionCursor;
    result->consistentBoundary = transaction.consistentBoundary;
    result->mutatorsResumed = ZR_FALSE;
    return ZR_TRUE;
}
