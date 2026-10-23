#ifndef ZR_VM_EXECUTION_BUDGET_FAST_H
#define ZR_VM_EXECUTION_BUDGET_FAST_H

#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/state.h"

/* One countdown covers mutator, resource and exact instruction-limit bounds.
 * The local remaining value can stay in a register between observable calls.
 * Its address is registered on state so real (including inline integer) errors
 * and nonlocal exits can settle admitted work before abandoning this C frame. */
typedef struct SZrExecutionAdmission {
    struct SZrExecutionAdmission *previous;
    SZrExecutionBudget *budget;
    SZrCallInfo **callInfo;
    const TZrInstruction **programCounter;
    TZrUInt32 *remaining;
    TZrUInt32 window;
    TZrUInt32 mutatorRemaining;
    TZrUInt32 resourceRemaining;
} SZrExecutionAdmission;

static inline void execution_admission_flush(SZrExecutionAdmission *admission) {
    if (admission != ZR_NULL) {
        TZrUInt32 delta = admission->window - *admission->remaining;
        if (delta != 0u) {
            admission->mutatorRemaining -= delta;
            if (admission->budget != ZR_NULL) {
                TZrUInt64 count = admission->budget->executedInstructions;
                admission->budget->executedInstructions = (TZrUInt64)-1 - count < delta
                        ? (TZrUInt64)-1 : count + delta;
                admission->resourceRemaining -= delta;
            }
            admission->window = *admission->remaining;
        }
    }
}

/* Invalidate after helpers/reentry: they may change the budget or its limit.
 * Resource intervals survive VM calls/returns, even though windows do not. */
static inline void execution_admission_suspend(SZrExecutionAdmission *admission) {
    if (admission != ZR_NULL) {
        execution_admission_flush(admission);
        admission->window = *admission->remaining = 0u;
    }
}

static inline void execution_admission_refresh(SZrExecutionAdmission *admission, SZrState *state) {
    execution_admission_suspend(admission);
    if (admission->budget != state->executionBudget) {
        admission->budget = state->executionBudget;
        admission->resourceRemaining = 0u;
    }
}

/* Check before admission, retaining Poll's resource-reason precedence. */
static inline TZrBool execution_admission_prepare(SZrExecutionAdmission *admission, SZrState *state) {
    TZrUInt32 window = admission->mutatorRemaining;
    SZrExecutionBudget *budget = admission->budget;
    if (budget != ZR_NULL) {
        if (admission->resourceRemaining == 0u || budget->termination != ZR_EXECUTION_TERMINATION_NONE ||
            (budget->hasInstructionLimit && budget->executedInstructions >= budget->maxInstructions)) {
            if (!ZrCore_ExecutionBudget_Poll(state, ZR_FALSE)) return ZR_FALSE;
            if (budget->hasInstructionLimit && budget->executedInstructions >= budget->maxInstructions) {
                budget->termination = ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT;
                return ZrCore_ExecutionBudget_Poll(state, ZR_FALSE);
            }
            admission->resourceRemaining = 256u;
        }
        if (window > admission->resourceRemaining) window = admission->resourceRemaining;
        if (budget->hasInstructionLimit) {
            TZrUInt64 available = budget->maxInstructions - budget->executedInstructions;
            if (available < window) window = (TZrUInt32)available;
        }
    }
    admission->window = *admission->remaining = window;
    return ZR_TRUE;
}

/* Called while all C-local contexts are still alive, before longjmp. */
void execution_admission_restore(SZrState *state, SZrExecutionAdmission *saved);
/* The real no-return error helper publishes the current instruction and stack
 * before allocating the Error; this also settles the pending budget count. */
void execution_admission_publish_error(SZrState *state);
#endif
