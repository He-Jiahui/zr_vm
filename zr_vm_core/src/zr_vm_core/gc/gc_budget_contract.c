#include "zr_vm_core/gc_budget_contract.h"

#include <limits.h>
#include <string.h>

enum {
    ZR_GC_BUDGET_FIELD_MAGIC = 1u,
    ZR_GC_BUDGET_FIELD_SCHEMA = 2u,
    ZR_GC_BUDGET_FIELD_FLAGS = 3u,
    ZR_GC_BUDGET_FIELD_PHASE = 4u,
    ZR_GC_BUDGET_FIELD_CURSOR = 5u,
    ZR_GC_BUDGET_FIELD_WORK = 6u,
    ZR_GC_BUDGET_FIELD_ELAPSED = 7u,
    ZR_GC_BUDGET_FIELD_BYTES = 8u,
    ZR_GC_BUDGET_FIELD_OBJECTS = 9u,
    ZR_GC_BUDGET_FIELD_COMPACT = 10u,
    ZR_GC_BUDGET_FIELD_OVERFLOW = 11u
};

static void gc_budget_set_diagnostic(
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

void ZrCore_GcBudget_DiagnosticClear(SZrGcBudgetDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

const TZrChar *ZrCore_GcBudget_DiagnosticName(EZrGcBudgetDiagnosticCode code) {
    switch (code) {
        case ZR_GC_BUDGET_DIAGNOSTIC_NONE:
            return "none";
        case ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC:
            return "invalid-magic";
        case ZR_GC_BUDGET_DIAGNOSTIC_INVALID_SCHEMA:
            return "invalid-schema";
        case ZR_GC_BUDGET_DIAGNOSTIC_UNKNOWN_FLAGS:
            return "unknown-flags";
        case ZR_GC_BUDGET_DIAGNOSTIC_INVALID_PHASE:
            return "invalid-phase";
        case ZR_GC_BUDGET_DIAGNOSTIC_OVERFLOW:
            return "overflow";
        case ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET:
            return "invalid-budget";
        case ZR_GC_BUDGET_DIAGNOSTIC_COUNT:
        default:
            return "unknown";
    }
}

void ZrCore_GcBudget_Init(SZrGcBudget *budget) {
    if (budget == ZR_NULL) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    budget->magic = ZR_GC_BUDGET_CONTRACT_MAGIC;
    budget->schemaVersion = ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION;
    budget->flags = ZR_GC_BUDGET_FLAG_REPORT_PRESSURE;
}

TZrBool ZrCore_GcBudget_Validate(
        const SZrGcBudget *budget,
        SZrGcBudgetDiagnostic *diagnostic) {
    ZrCore_GcBudget_DiagnosticClear(diagnostic);
    if (budget == ZR_NULL) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
                                 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (budget->magic != ZR_GC_BUDGET_CONTRACT_MAGIC) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC,
                                 ZR_GC_BUDGET_FIELD_MAGIC,
                                 ZR_GC_BUDGET_CONTRACT_MAGIC,
                                 budget->magic);
        return ZR_FALSE;
    }
    if (budget->schemaVersion != ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_INVALID_SCHEMA,
                                 ZR_GC_BUDGET_FIELD_SCHEMA,
                                 ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION,
                                 budget->schemaVersion);
        return ZR_FALSE;
    }
    if ((budget->flags & ~ZR_GC_BUDGET_FLAG_KNOWN_MASK) != 0u ||
        budget->reserved != 0u) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_UNKNOWN_FLAGS,
                                 ZR_GC_BUDGET_FIELD_FLAGS,
                                 ZR_GC_BUDGET_FLAG_KNOWN_MASK,
                                 budget->flags);
        return ZR_FALSE;
    }
    if (budget->maxElapsedUs == 0u && budget->maxWorkUnits == 0u &&
        budget->maxBytes == 0u && budget->maxObjects == 0u) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                                 ZR_GC_BUDGET_FIELD_WORK,
                                 1u, 0u);
        return ZR_FALSE;
    }
    if ((budget->flags & ZR_GC_BUDGET_FLAG_ALLOW_COMPACT) != 0u &&
        budget->compactBudgetBytes == 0u) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                                 ZR_GC_BUDGET_FIELD_COMPACT,
                                 1u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool gc_budget_phase_valid(EZrGcBudgetPhase phase) {
    return phase >= ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT &&
           phase <= ZR_GC_BUDGET_PHASE_COMPACT;
}

static TZrBool gc_budget_add_u64(TZrUInt64 left, TZrUInt64 right, TZrUInt64 *out) {
    if (right > UINT64_MAX - left) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool gc_budget_exceeds(TZrUInt64 value, TZrUInt64 limit) {
    return limit != 0u && value > limit;
}

TZrBool ZrCore_GcBudget_EvaluateStep(
        const SZrGcBudget *budget,
        EZrGcBudgetPhase phase,
        TZrUInt64 cursor,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrInt64 debtBytes,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *result,
        SZrGcBudgetDiagnostic *diagnostic) {
    TZrUInt64 nextCursor;

    ZrCore_GcBudget_DiagnosticClear(diagnostic);
    if (result == ZR_NULL ||
        !ZrCore_GcBudget_Validate(budget, diagnostic)) {
        if (result == ZR_NULL && diagnostic != ZR_NULL) {
            gc_budget_set_diagnostic(diagnostic,
                                     ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
                                     0u, 1u, 0u);
        }
        return ZR_FALSE;
    }
    memset(result, 0, sizeof(*result));
    result->status = ZR_GC_BUDGET_STEP_REJECTED;
    result->phase = phase;
    result->pauseReason = ZR_GC_BUDGET_PAUSE_NONE;
    result->nextCursor = cursor;
    result->elapsedUs = elapsedUs;
    result->bytesDone = bytes;
    result->objectsDone = objects;
    result->debtBytes = debtBytes;
    result->maxAtomicPauseUs = atomicPauseUs;
    result->consistentBoundary = ZR_TRUE;

    if (!gc_budget_phase_valid(phase) || workUnits == 0u) {
        gc_budget_set_diagnostic(diagnostic,
                                 phase == ZR_GC_BUDGET_PHASE_IDLE ||
                                         phase >= ZR_GC_BUDGET_PHASE_COUNT
                                     ? ZR_GC_BUDGET_DIAGNOSTIC_INVALID_PHASE
                                     : ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
                                 phase == ZR_GC_BUDGET_PHASE_IDLE ||
                                         phase >= ZR_GC_BUDGET_PHASE_COUNT
                                     ? ZR_GC_BUDGET_FIELD_PHASE
                                     : ZR_GC_BUDGET_FIELD_WORK,
                                 1u, workUnits);
        return ZR_FALSE;
    }
    if (!gc_budget_add_u64(cursor, workUnits, &nextCursor)) {
        gc_budget_set_diagnostic(diagnostic,
                                 ZR_GC_BUDGET_DIAGNOSTIC_OVERFLOW,
                                 ZR_GC_BUDGET_FIELD_CURSOR,
                                 UINT64_MAX, cursor);
        return ZR_FALSE;
    }

    if (phase == ZR_GC_BUDGET_PHASE_COMPACT &&
        ((budget->flags & ZR_GC_BUDGET_FLAG_ALLOW_COMPACT) == 0u ||
         bytes > budget->compactBudgetBytes)) {
        result->status = ZR_GC_BUDGET_STEP_DEFERRED;
        result->pauseReason = ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION;
        result->compactDeferredCount = 1u;
        return ZR_TRUE;
    }
    if (gc_budget_exceeds(elapsedUs, budget->maxElapsedUs) ||
        gc_budget_exceeds(workUnits, budget->maxWorkUnits) ||
        gc_budget_exceeds(bytes, budget->maxBytes) ||
        gc_budget_exceeds(objects, budget->maxObjects)) {
        result->pauseReason = ZR_GC_BUDGET_PAUSE_BUDGET;
        return ZR_TRUE;
    }

    result->status = ZR_GC_BUDGET_STEP_ACCEPTED;
    result->nextCursor = nextCursor;
    result->workDone = workUnits;
    if (budget->maxAtomicPauseUs != 0u &&
        atomicPauseUs > budget->maxAtomicPauseUs) {
        result->status = ZR_GC_BUDGET_STEP_OVER_BUDGET;
        result->pauseReason = ZR_GC_BUDGET_PAUSE_BUDGET;
        result->overBudgetCount = 1u;
    }
    if ((budget->flags & ZR_GC_BUDGET_FLAG_REPORT_PRESSURE) != 0u &&
        budget->pressureThresholdBytes != 0u && debtBytes > 0 &&
        (TZrUInt64)debtBytes >= budget->pressureThresholdBytes) {
        result->pressure = ZR_TRUE;
        if (result->pauseReason == ZR_GC_BUDGET_PAUSE_NONE) {
            result->pauseReason = ZR_GC_BUDGET_PAUSE_PRESSURE;
        }
    }
    if (result->pauseReason == ZR_GC_BUDGET_PAUSE_NONE) {
        result->pauseReason = ZR_GC_BUDGET_PAUSE_NONE;
    }
    return ZR_TRUE;
}
