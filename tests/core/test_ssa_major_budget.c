#include "zr_vm_core/gc_budget_contract.h"
#include "zr_vm_core/gc_compact.h"
#include "zr_vm_core/gc_major.h"

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

static void test_budget_ledger_accumulates_only_coherent_cursor_steps(void) {
    SZrGcBudget budget = test_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;
    SZrGcBudgetLedger ledger;

    ZrCore_GcBudget_LedgerInit(&ledger);
    assert(ZrCore_GcBudget_EvaluateStep(
            &budget, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT, 0u,
            1u, 2u, 8u, 1u, 0, 0u, &result, &diagnostic));
    assert(ZrCore_GcBudget_LedgerAccumulate(
            &ledger, &result, &diagnostic));
    assert(ledger.cursor == 1u);
    assert(ledger.workDone == 1u);
    assert(ledger.bytesDone == 8u);

    result.status = ZR_GC_BUDGET_STEP_REJECTED;
    result.nextCursor = 0u;
    result.workDone = 0u;
    assert(!ZrCore_GcBudget_LedgerAccumulate(
            &ledger, &result, &diagnostic));
    assert(diagnostic.code == ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET);
    assert(ledger.cursor == 1u);
}

static void test_major_state_machine_preserves_order_and_budget_cursor(void) {
    SZrGcBudget budget = test_budget();
    SZrGcMajorState state;
    SZrGcMajorDiagnostic diagnostic;
    SZrGcBudgetStepResult result;

    ZrCore_GcMajor_Init(&state);
    assert(ZrCore_GcMajor_Begin(
            &state, 7u, ZR_GC_MAJOR_FLAG_CONCURRENT_MARK, &diagnostic));
    assert(state.phase == ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT);
    assert(ZrCore_GcMajor_Step(
            &state, &budget, ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT,
            1u, 1u, 8u, 1u, 0, 0u, &result, &diagnostic));
    assert(state.cursor == 1u);
    assert(ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_CONCURRENT_MARK,
            ZR_TRUE, &diagnostic));
    assert(!ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_SWEEP, ZR_TRUE, &diagnostic));
    assert(diagnostic.code == ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION);
    assert(ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_REMARK, ZR_TRUE, &diagnostic));
    assert(ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_SWEEP, ZR_TRUE, &diagnostic));
    assert(ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_COMPACT, ZR_TRUE, &diagnostic));
    assert(ZrCore_GcMajor_Advance(
            &state, ZR_GC_MAJOR_PHASE_COMPLETE, ZR_TRUE, &diagnostic));
    assert(state.phase == ZR_GC_MAJOR_PHASE_COMPLETE);
    assert(ZrCore_GcMajor_Begin(
            &state, 8u, ZR_GC_MAJOR_FLAG_CONCURRENT_MARK, &diagnostic));
    assert(ZrCore_GcMajor_Cancel(&state, &diagnostic));
    assert(ZrCore_GcMajor_Cancel(&state, &diagnostic));
}

static void test_selective_compaction_skips_pinned_regions_and_respects_budget(void) {
    SZrGarbageCollectRegionDescriptor regions[4] = {
        {1u, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, 1000u, 800u, 400u,
         10u, 0u},
        {2u, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, 1000u, 900u, 600u,
         8u, 0u},
        {3u, ZR_GARBAGE_COLLECT_REGION_KIND_PINNED, 1000u, 700u, 650u,
         4u, 0u},
        {4u, ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT, 1000u, 500u, 500u,
         1u, 0u}
    };
    SZrGcCompactRequest request = {
        regions, 4u, 500u, 25u, ZR_TRUE
    };
    SZrGcCompactPlan plan;
    SZrGcCompactDiagnostic diagnostic;

    assert(ZrCore_GcCompact_Plan(&request, &plan, &diagnostic));
    assert(plan.mode == ZR_GC_COMPACT_MODE_SELECTIVE_MOVING);
    assert(plan.candidateRegionCount == 2u);
    assert(plan.pinnedRegionCount == 2u);
    assert(plan.pinnedBytes == 1200u);
    assert(plan.plannedBytes == 400u);
    assert(plan.eligible);
    assert(plan.deferred);
    assert(ZrCore_GcCompact_Validate(&plan, &diagnostic));

    request.allowMoving = ZR_FALSE;
    assert(ZrCore_GcCompact_Plan(&request, &plan, &diagnostic));
    assert(plan.mode == ZR_GC_COMPACT_MODE_NON_MOVING);
    assert(!plan.eligible);
    assert(plan.deferred);
    assert(plan.plannedBytes == 0u);
}

static void test_compaction_rejects_malformed_region_facts(void) {
    SZrGarbageCollectRegionDescriptor region = {
        1u, ZR_GARBAGE_COLLECT_REGION_KIND_OLD, 10u, 11u, 1u, 1u, 0u
    };
    SZrGcCompactRequest request = {&region, 1u, 0u, 0u, ZR_TRUE};
    SZrGcCompactPlan plan;
    SZrGcCompactDiagnostic diagnostic;

    assert(!ZrCore_GcCompact_Plan(&request, &plan, &diagnostic));
    assert(diagnostic.code == ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION);
}

int main(void) {
    test_accepts_bounded_mark_work_and_advances_cursor();
    test_records_budget_hit_without_publishing_cursor();
    test_reports_atomic_overrun_and_pressure();
    test_defers_compaction_at_coherent_boundary();
    test_rejects_malformed_budget_and_overflow();
    test_budget_ledger_accumulates_only_coherent_cursor_steps();
    test_major_state_machine_preserves_order_and_budget_cursor();
    test_selective_compaction_skips_pinned_regions_and_respects_budget();
    test_compaction_rejects_malformed_region_facts();
    return 0;
}
