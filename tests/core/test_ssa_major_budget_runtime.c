#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

#include <assert.h>
#include <string.h>

static SZrGcBudget make_budget(void) {
    SZrGcBudget budget;
    ZrCore_GcBudget_Init(&budget);
    budget.maxWorkUnits = 1u;
    budget.maxElapsedUs = 100u;
    budget.maxBytes = 256u;
    budget.maxObjects = 4u;
    budget.pressureThresholdBytes = 1000u;
    budget.maxAtomicPauseUs = 20u;
    return budget;
}

int main(void) {
    SZrGlobalState global;
    SZrGarbageCollector collector;
    SZrGcBudget budget = make_budget();
    SZrGcBudget copy;
    SZrGcBudgetStepResult result;
    SZrGcBudgetStepResult stats;
    SZrState state;

    memset(&global, 0, sizeof(global));
    memset(&collector, 0, sizeof(collector));
    global.garbageCollector = &collector;
    memset(&state, 0, sizeof(state));
    state.global = &global;

    /* Set/Get is transactional: malformed updates do not replace state. */
    assert(ZrCore_Gc_SetBudget(&state, &budget));
    assert(ZrCore_GarbageCollector_GetBudget(&global, &copy));
    assert(copy.maxWorkUnits == 1u);
    copy.magic ^= 1u;
    assert(!ZrCore_GarbageCollector_SetBudget(&global, &copy));
    assert(ZrCore_GarbageCollector_GetBudget(&global, &copy));
    assert(copy.magic == budget.magic);

    /* A tiny budget advances one cursor unit, then rejects the next slice. */
    assert(ZrCore_GarbageCollector_EvaluateBudgetStep(
            &global, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
            1u, 5u, 32u, 1u, 0u, &result));
    assert(result.status == ZR_GC_BUDGET_STEP_ACCEPTED);
    assert(result.nextCursor == 1u);
    assert(collector.budgetCursor == 1u);
    assert(ZrCore_GarbageCollector_EvaluateBudgetStep(
            &global, ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
            2u, 5u, 32u, 1u, 0u, &result));
    assert(result.status == ZR_GC_BUDGET_STEP_REJECTED);
    assert(collector.budgetCursor == 1u);
    assert(collector.budgetFallback);

    /* Debt/pressure and atomic overrun are surfaced without cursor loss. */
    budget.maxWorkUnits = 8u;
    assert(ZrCore_GarbageCollector_SetBudget(&global, &budget));
    collector.gcDebtSize = 2048u;
    assert(ZrCore_GarbageCollector_EvaluateBudgetStep(
            &global, ZR_GC_BUDGET_PHASE_REMARK,
            1u, 5u, 32u, 1u, 25u, &result));
    assert(result.status == ZR_GC_BUDGET_STEP_OVER_BUDGET);
    assert(result.pressure);
    assert(result.overBudgetCount == 1u);
    assert(collector.budgetOverBudgetCount == 1u);
    assert(ZrCore_Gc_GetStats(&state, &stats));
    assert(stats.pressure && stats.debtBytes == 2048);
    assert(stats.nextCursor == 0u); /* over-budget slices do not publish */

    /* Compact work is deferred unless explicitly allowed by the contract. */
    assert(ZrCore_GarbageCollector_EvaluateBudgetStep(
            &global, ZR_GC_BUDGET_PHASE_COMPACT,
            1u, 1u, 64u, 1u, 0u, &result));
    assert(result.status == ZR_GC_BUDGET_STEP_DEFERRED);
    assert(result.pauseReason == ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION);
    assert(collector.budgetCursor == 0u);
    return 0;
}
