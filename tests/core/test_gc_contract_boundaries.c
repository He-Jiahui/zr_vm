#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "zr_vm_core/gc_budget_contract.h"
#include "zr_vm_core/gc_compact.h"
#include "zr_vm_core/gc_major.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static SZrGcBudget valid_budget(void) {
    SZrGcBudget budget;
    ZrCore_GcBudget_Init(&budget);
    budget.maxWorkUnits = 8u;
    return budget;
}

static SZrGcBudgetStepResult seeded_result(void) {
    SZrGcBudgetStepResult result = {0};
    result.status = ZR_GC_BUDGET_STEP_ACCEPTED;
    result.phase = ZR_GC_BUDGET_PHASE_SWEEP;
    result.pauseReason = ZR_GC_BUDGET_PAUSE_PRESSURE;
    result.nextCursor = 73u;
    result.workDone = 79u;
    result.elapsedUs = 83u;
    result.bytesDone = 89u;
    result.objectsDone = 97u;
    result.debtBytes = 101;
    result.maxAtomicPauseUs = 103u;
    result.overBudgetCount = 107u;
    result.compactDeferredCount = 109u;
    result.pressure = ZR_TRUE;
    result.consistentBoundary = ZR_TRUE;
    return result;
}

static void reserved_diagnostic(void) {
    SZrGcBudget budget = valid_budget();
    SZrGcBudgetDiagnostic reserved, flags;
    budget.flags |= (TZrUInt32)1u << 31u;
    CHECK(!ZrCore_GcBudget_Validate(&budget, &flags));
    budget = valid_budget();
    budget.reserved = 5u;
    CHECK(!ZrCore_GcBudget_Validate(&budget, &reserved));
    CHECK(reserved.code != ZR_GC_BUDGET_DIAGNOSTIC_NONE);
    CHECK(reserved.field != flags.field);
    CHECK(reserved.expected == 0u && reserved.actual == 5u);
}

static void preflight_result(int nullBudget) {
    SZrGcBudget budget = valid_budget();
    SZrGcBudgetStepResult result = seeded_result();
    SZrGcBudgetDiagnostic diagnostic;
    budget.magic = 0u;
    CHECK(!ZrCore_GcBudget_EvaluateStep(nullBudget ? NULL : &budget,
            ZR_GC_BUDGET_PHASE_MARK_CONCURRENT, 19u, 1u, 0u, 0u, 0u,
            0, 0u, &result, &diagnostic));
    CHECK(diagnostic.code == (nullBudget ? ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT
                                        : ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC));
    /* Callers must never publish the preceding successful witness after a
     * rejected preflight, nor copy an uninitialized local witness. */
    CHECK(result.status == ZR_GC_BUDGET_STEP_REJECTED);
    CHECK(result.phase == ZR_GC_BUDGET_PHASE_IDLE);
    CHECK(result.pauseReason == ZR_GC_BUDGET_PAUSE_NONE);
    CHECK(result.nextCursor == 19u && result.workDone == 0u);
    CHECK(result.bytesDone == 0u && result.objectsDone == 0u && result.elapsedUs == 0u);
    CHECK(result.debtBytes == 0 && result.maxAtomicPauseUs == 0u);
    CHECK(result.overBudgetCount == 0u && result.compactDeferredCount == 0u);
    CHECK(!result.pressure && !result.consistentBoundary);
}

static void phase_diagnostic(EZrGcBudgetPhase phase) {
    SZrGcBudget budget = valid_budget();
    SZrGcBudgetStepResult result = seeded_result();
    SZrGcBudgetDiagnostic diagnostic;
    CHECK(!ZrCore_GcBudget_EvaluateStep(&budget, phase, 19u,
            17u, 0u, 0u, 0u, 0, 0u, &result, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_BUDGET_DIAGNOSTIC_INVALID_PHASE);
    CHECK(diagnostic.actual == (TZrUInt64)phase);
    CHECK(result.status == ZR_GC_BUDGET_STEP_REJECTED && result.nextCursor == 19u);
}

static void work_control(void) {
    SZrGcBudget budget = valid_budget();
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;
    CHECK(!ZrCore_GcBudget_EvaluateStep(&budget, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
            19u, 0u, 0u, 0u, 0u, 0, 0u, &result, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET);
    CHECK(diagnostic.expected == 1u && diagnostic.actual == 0u);
    CHECK(ZrCore_GcBudget_EvaluateStep(&budget, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
            19u, 1u, 0u, 0u, 0u, 0, 0u, &result, &diagnostic));
    CHECK(result.status == ZR_GC_BUDGET_STEP_ACCEPTED && result.nextCursor == 20u);
}

static void major_overflow(void) {
    SZrGcBudget budget = valid_budget();
    SZrGcMajorState state;
    SZrGcMajorDiagnostic diagnostic;
    SZrGcBudgetStepResult result;
    ZrCore_GcMajor_Init(&state);
    CHECK(ZrCore_GcMajor_Begin(&state, 1u, 0u, &diagnostic));
    state.cursor = UINT64_MAX;
    CHECK(!ZrCore_GcMajor_Step(&state, &budget, ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT,
            1u, 0u, 0u, 0u, 0, 0u, &result, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_MAJOR_DIAGNOSTIC_OVERFLOW);
    CHECK(state.cursor == UINT64_MAX && state.workDone == 0u);
    CHECK(result.status == ZR_GC_BUDGET_STEP_REJECTED && result.nextCursor == UINT64_MAX);
}

static void major_negative_phase(void) {
    SZrGcMajorState state, before;
    SZrGcMajorDiagnostic diagnostic;
    ZrCore_GcMajor_Init(&state);
    state.phase = (EZrGcMajorPhase)-1;
    state.cycleId = 7u; /* Isolate phase validation from the nonzero cycle rule. */
    before = state;
    CHECK(!ZrCore_GcMajor_Validate(&state, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE);
    CHECK(!ZrCore_GcMajor_Begin(&state, 8u, 0u, &diagnostic));
    CHECK(state.phase == before.phase && state.cycleId == before.cycleId &&
          state.cursor == before.cursor && state.workDone == before.workDone);
    CHECK(!ZrCore_GcMajor_Cancel(&state, &diagnostic));
    CHECK(state.phase == before.phase && state.cycleId == before.cycleId &&
          state.cursor == before.cursor && state.workDone == before.workDone);
}

static void compact_negative_mode(void) {
    SZrGcCompactPlan plan;
    SZrGcCompactDiagnostic diagnostic;
    ZrCore_GcCompact_Init(&plan);
    plan.mode = (EZrGcCompactMode)-1;
    CHECK(!ZrCore_GcCompact_Validate(&plan, &diagnostic));
}

static void major_negative_next_phase(void) {
    SZrGcMajorState state, before;
    SZrGcMajorDiagnostic diagnostic;
    ZrCore_GcMajor_Init(&state);
    CHECK(ZrCore_GcMajor_Begin(&state, 7u, 0u, &diagnostic));
    before = state;
    CHECK(!ZrCore_GcMajor_Advance(&state, (EZrGcMajorPhase)-1, ZR_TRUE, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE);
    CHECK(state.phase == before.phase && state.cycleId == before.cycleId &&
          state.cursor == before.cursor && state.workDone == before.workDone);
}

static void compact_partition(int overflow) {
    SZrGcCompactPlan plan;
    SZrGcCompactDiagnostic diagnostic;
    ZrCore_GcCompact_Init(&plan);
    plan.regionCount = overflow ? UINT32_MAX : 2u;
    plan.candidateRegionCount = overflow ? UINT32_MAX : 2u;
    plan.pinnedRegionCount = overflow ? 1u : 2u;
    CHECK(!ZrCore_GcCompact_Validate(&plan, &diagnostic));
    CHECK(diagnostic.code == ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION);
}

static void compact_control(void) {
    SZrGcCompactPlan plan;
    SZrGcCompactDiagnostic diagnostic;
    ZrCore_GcCompact_Init(&plan);
    CHECK(ZrCore_GcCompact_Validate(&plan, &diagnostic));
    plan.regionCount = 2u; plan.candidateRegionCount = 1u; plan.pinnedRegionCount = 1u;
    CHECK(ZrCore_GcCompact_Validate(&plan, &diagnostic));
    plan.regionCount = UINT32_MAX; plan.candidateRegionCount = UINT32_MAX; plan.pinnedRegionCount = 0u;
    CHECK(ZrCore_GcCompact_Validate(&plan, &diagnostic));
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (strcmp(argv[1], "reserved") == 0) reserved_diagnostic();
    else if (strcmp(argv[1], "invalid_budget") == 0) preflight_result(0);
    else if (strcmp(argv[1], "null_budget") == 0) preflight_result(1);
    else if (strcmp(argv[1], "idle_phase") == 0) phase_diagnostic(ZR_GC_BUDGET_PHASE_IDLE);
    else if (strcmp(argv[1], "complete_phase") == 0) phase_diagnostic(ZR_GC_BUDGET_PHASE_COMPLETE);
    else if (strcmp(argv[1], "negative_phase") == 0) phase_diagnostic((EZrGcBudgetPhase)-1);
    else if (strcmp(argv[1], "work_control") == 0) work_control();
    else if (strcmp(argv[1], "major_overflow") == 0) major_overflow();
    else if (strcmp(argv[1], "major_negative_phase") == 0) major_negative_phase();
    else if (strcmp(argv[1], "major_negative_next_phase") == 0) major_negative_next_phase();
    else if (strcmp(argv[1], "compact_negative_mode") == 0) compact_negative_mode();
    else if (strcmp(argv[1], "compact_partition") == 0) compact_partition(0);
    else if (strcmp(argv[1], "compact_partition_overflow") == 0) compact_partition(1);
    else if (strcmp(argv[1], "compact_control") == 0) compact_control();
    else return 2;
    return failures == 0 ? 0 : 1;
}
