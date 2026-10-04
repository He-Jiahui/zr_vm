#ifndef ZR_VM_PARSER_EXEC_IR_BRANCH_LOOP_RANGES_H
#define ZR_VM_PARSER_EXEC_IR_BRANCH_LOOP_RANGES_H

#include "zr_vm_parser/exec_ir_branch_facts.h"

/* Private operations from the already validated DAG producer. All vectors are
 * caller-owned scratch; no hook owns/freezes a result or executes the IR. */
typedef struct SZrBranchLoopOperations {
    void (*entry)(const SZrExecIrFunction *, const SZrExecIrBranchFacts *, SZrExecIrBranchValueFact *);
    void (*transfer)(const SZrExecIrModule *, const SZrExecIrFunction *,
            const SZrExecIrBranchFacts *, const SZrExecIrInstruction *, SZrExecIrBranchValueFact *);
    TZrBool (*refine)(const SZrExecIrFunction *, const SZrExecIrInstruction *,
            TZrUInt32, SZrExecIrBranchValueFact *);
    SZrExecIrBranchValueFact (*join)(SZrExecIrBranchValueFact, SZrExecIrBranchValueFact);
} SZrBranchLoopOperations;

typedef enum EZrBranchLoopResult {
    ZR_BRANCH_LOOP_CONVERGED,
    ZR_BRANCH_LOOP_UNSUPPORTED,
    ZR_BRANCH_LOOP_UNPROVEN,
    ZR_BRANCH_LOOP_ERROR
} EZrBranchLoopResult;

/* Core STRUCTURE|SSA and storage validation precede this private call. On
 * CONVERGED, publish complete staged block/edge facts; all other outcomes leave
 * result arrays unchanged. UNSUPPORTED/UNPROVEN require legacy UNKNOWN fallback. */
EZrBranchLoopResult zr_branch_loop_ranges(const SZrExecIrModule *module,
        const SZrExecIrFunction *function, SZrExecIrBranchFacts *result,
        const SZrBranchLoopOperations *operations, SZrExecIrDiagnostic *diagnostic);

#endif
