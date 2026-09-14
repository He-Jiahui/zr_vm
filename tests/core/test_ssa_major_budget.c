#include "zr_vm_core/gc_budget_contract.h"

#include <assert.h>
#include <stdint.h>

static SZrGcBudget test_budget(void) {
    SZrGcBudget budget;

    ZrCore_GcBudget_Init(&budget);
    budget.maxElapsedUs = 100u;
    budget.maxWorkUnits = 4u;
    budget.maxBytes = 256u;
    budget.maxObjects = 4u;
    budget.compactBudgetBytes = 128u;
    budget.pressureThresholdBytes = 1000u;
    budget.maxAtomicPauseUs = 20u;
    return budget;
}

static void test_accepts_bounded_mark_work_and_advances_cursor(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;

    assert(ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT, 10u,
            1u, 5u, 32u, 1u, 0, 0u, &result, &diagnostic));
    assert(result.status == ZR_GC_BUDGET_STEP_ACCEPTED);
    assert(result.nextCursor == 11u);
    assert(result.workDone == 1u);
    assert(result.consistentBoundary);
    assert(result.pauseReason == ZR_GC_BUDGET_PAUSE_NONE);
}

static void test_records_budget_hit_without_publishing_cursor(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;

    assert(ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_SWEEP, 20u,
            5u, 5u, 32u, 1u, 0, 0u, &result, &diagnostic));
    assert(result.status == ZR_GC_BUDGET_STEP_REJECTED);
    assert(result.pauseReason == ZR_GC_BUDGET_PAUSE_BUDGET);
    assert(result.nextCursor == 20u);
    assert(result.workDone == 0u);
    assert(result.consistentBoundary);
}

static void test_reports_atomic_overrun_and_pressure(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;

    assert(ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_REMARK, 30u,
            1u, 5u, 32u, 1u, 1500, 25u, &result, &diagnostic));
    assert(result.status == ZR_GC_BUDGET_STEP_OVER_BUDGET);
    assert(result.overBudgetCount == 1u);
    assert(result.pressure);
    assert(result.pauseReason == ZR_GC_BUDGET_PAUSE_BUDGET);
    assert(result.nextCursor == 31u);
}

static void test_defers_compaction_at_coherent_boundary(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;

    assert(ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_COMPACT, 40u,
            1u, 5u, 256u, 1u, 0, 0u, &result, &diagnostic));
    assert(result.status == ZR_GC_BUDGET_STEP_DEFERRED);
    assert(result.pauseReason == ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION);
    assert(result.compactDeferredCount == 1u);
    assert(result.nextCursor == 40u);
    assert(result.consistentBoundary);
}

static void test_rejects_malformed_budget_and_overflow(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;

    budget.flags |= ((TZrUInt32)1u << 31u);
    assert(!ZrCore_GcBudget_Validate(&budget, &diagnostic));
    assert(diagnostic.code == ZR_GC_BUDGET_DIAGNOSTIC_UNKNOWN_FLAGS);

    ZrCore_GcBudget_Init(&budget);
    budget.maxWorkUnits = 1u;
    assert(!ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT, UINT64_MAX,
            1u, 0u, 0u, 0u, 0, 0u, &result, &diagnostic));
    assert(diagnostic.code == ZR_GC_BUDGET_DIAGNOSTIC_OVERFLOW);
}

int main(void) {
    test_accepts_bounded_mark_work_and_advances_cursor();
    test_records_budget_hit_without_publishing_cursor();
    test_reports_atomic_overrun_and_pressure();
    test_defers_compaction_at_coherent_boundary();
    test_rejects_malformed_budget_and_overflow();
    return 0;
}
