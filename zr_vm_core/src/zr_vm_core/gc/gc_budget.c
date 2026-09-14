#include "zr_vm_core/gc_budget_contract.h"

#include <limits.h>
#include <string.h>

static void gc_budget_ledger_diag(
        SZrGcBudgetDiagnostic *diagnostic,
        EZrGcBudgetDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->field = field;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrUInt64 gc_budget_sat_add(TZrUInt64 left, TZrUInt64 right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

const TZrChar *ZrCore_GcBudget_StatusName(EZrGcBudgetStepStatus status) {
    switch (status) {
        case ZR_GC_BUDGET_STEP_ACCEPTED:
            return "accepted";
        case ZR_GC_BUDGET_STEP_REJECTED:
            return "rejected";
        case ZR_GC_BUDGET_STEP_DEFERRED:
            return "deferred";
        case ZR_GC_BUDGET_STEP_OVER_BUDGET:
            return "over-budget";
        default:
            return "unknown";
    }
}

const TZrChar *ZrCore_GcBudget_PauseReasonName(EZrGcBudgetPauseReason reason) {
    switch (reason) {
        case ZR_GC_BUDGET_PAUSE_NONE:
            return "none";
        case ZR_GC_BUDGET_PAUSE_BUDGET:
            return "budget";
        case ZR_GC_BUDGET_PAUSE_SAFEPOINT:
            return "safepoint";
        case ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION:
            return "deferred-relocation";
        case ZR_GC_BUDGET_PAUSE_PRESSURE:
            return "pressure";
        case ZR_GC_BUDGET_PAUSE_OOM:
            return "oom";
        case ZR_GC_BUDGET_PAUSE_CANCELLED:
            return "cancelled";
        case ZR_GC_BUDGET_PAUSE_COMPLETE:
            return "complete";
        default:
            return "unknown";
    }
}

void ZrCore_GcBudget_LedgerInit(SZrGcBudgetLedger *ledger) {
    if (ledger == ZR_NULL) {
        return;
    }
    memset(ledger, 0, sizeof(*ledger));
    ledger->magic = ZR_GC_BUDGET_CONTRACT_MAGIC;
    ledger->schemaVersion = ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION;
    ledger->consistentBoundary = ZR_TRUE;
}

TZrBool ZrCore_GcBudget_LedgerValidate(
        const SZrGcBudgetLedger *ledger,
        SZrGcBudgetDiagnostic *diagnostic) {
    ZrCore_GcBudget_DiagnosticClear(diagnostic);
    if (ledger == ZR_NULL) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (ledger->magic != ZR_GC_BUDGET_CONTRACT_MAGIC) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC,
                              1u, ZR_GC_BUDGET_CONTRACT_MAGIC,
                              ledger->magic);
        return ZR_FALSE;
    }
    if (ledger->schemaVersion != ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_SCHEMA,
                              2u, ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION,
                              ledger->schemaVersion);
        return ZR_FALSE;
    }
    if (ledger->lastStatus > ZR_GC_BUDGET_STEP_OVER_BUDGET ||
        ledger->lastPhase >= ZR_GC_BUDGET_PHASE_COUNT ||
        ledger->lastPauseReason >= ZR_GC_BUDGET_PAUSE_COUNT ||
        !ledger->consistentBoundary) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                              3u, 1u, ledger->lastPhase);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcBudget_LedgerAccumulate(
        SZrGcBudgetLedger *ledger,
        const SZrGcBudgetStepResult *result,
        SZrGcBudgetDiagnostic *diagnostic) {
    TZrBool advancesCursor;

    ZrCore_GcBudget_DiagnosticClear(diagnostic);
    if (ledger == ZR_NULL || result == ZR_NULL) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcBudget_LedgerValidate(ledger, diagnostic) ||
        result->status > ZR_GC_BUDGET_STEP_OVER_BUDGET ||
        result->phase >= ZR_GC_BUDGET_PHASE_COUNT ||
        result->pauseReason >= ZR_GC_BUDGET_PAUSE_COUNT ||
        !result->consistentBoundary) {
        if (diagnostic != ZR_NULL &&
            diagnostic->code == ZR_GC_BUDGET_DIAGNOSTIC_NONE) {
            gc_budget_ledger_diag(diagnostic,
                                  ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                                  4u, 1u, result->phase);
        }
        return ZR_FALSE;
    }

    advancesCursor = result->status == ZR_GC_BUDGET_STEP_ACCEPTED ||
                     result->status == ZR_GC_BUDGET_STEP_OVER_BUDGET;
    if ((advancesCursor && result->nextCursor < ledger->cursor) ||
        (!advancesCursor && result->nextCursor != ledger->cursor)) {
        gc_budget_ledger_diag(diagnostic,
                              ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                              5u, ledger->cursor, result->nextCursor);
        return ZR_FALSE;
    }

    if (advancesCursor) {
        ledger->cursor = result->nextCursor;
    }
    ledger->lastStatus = result->status;
    ledger->lastPhase = result->phase;
    ledger->lastPauseReason = result->pauseReason;
    ledger->workDone = gc_budget_sat_add(ledger->workDone,
                                         result->workDone);
    ledger->elapsedUs = gc_budget_sat_add(ledger->elapsedUs,
                                          result->elapsedUs);
    ledger->bytesDone = gc_budget_sat_add(ledger->bytesDone,
                                          result->bytesDone);
    ledger->objectsDone = gc_budget_sat_add(ledger->objectsDone,
                                            result->objectsDone);
    ledger->overBudgetCount = gc_budget_sat_add(
            ledger->overBudgetCount, result->overBudgetCount);
    ledger->compactDeferredCount = gc_budget_sat_add(
            ledger->compactDeferredCount, result->compactDeferredCount);
    ledger->debtBytes = result->debtBytes;
    ledger->pressure = (TZrBool)(ledger->pressure || result->pressure);
    ledger->consistentBoundary = result->consistentBoundary;
    return ZR_TRUE;
}
