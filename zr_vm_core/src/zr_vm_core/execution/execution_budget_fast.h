#ifndef ZR_VM_EXECUTION_BUDGET_FAST_H
#define ZR_VM_EXECUTION_BUDGET_FAST_H

#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/state.h"

/* Keep admission counts exact even across throws and reentrant native calls.
 * Only the expensive resource checks are sampled, at most 256 instructions
 * apart. Native/GC boundaries retain their immediate resource checks. */
static inline TZrBool execution_budget_admit_instruction(
        SZrState *state, SZrExecutionBudget *budget, TZrUInt32 *untilResourcePoll) {
    if (*untilResourcePoll == 0u ||
        budget->termination != ZR_EXECUTION_TERMINATION_NONE ||
        (budget->hasInstructionLimit && budget->executedInstructions >= budget->maxInstructions)) {
        if (!ZrCore_ExecutionBudget_Poll(state, ZR_TRUE)) {
            return ZR_FALSE;
        }
        *untilResourcePoll = 255u;
    } else {
        --*untilResourcePoll;
        if (budget->executedInstructions != (TZrUInt64)-1) {
            ++budget->executedInstructions;
        }
    }
    return ZR_TRUE;
}

#endif
