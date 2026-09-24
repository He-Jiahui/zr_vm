#ifndef ZR_VM_CORE_EXEC_IR_DEOPT_AGGREGATE_H
#define ZR_VM_CORE_EXEC_IR_DEOPT_AGGREGATE_H

#include "zr_vm_core/exec_ir_state_map.h"

/* Validate every recipe before any consumer traverses the side tables. */
TZrBool zr_exec_ir_deopt_aggregates_validate(
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic);

/* The shared preflight and the checkpoint's live range must be valid first. */
TZrBool zr_exec_ir_deopt_aggregates_live(
        const SZrExecIrFunction *function, const SZrExecIrDeoptState *state,
        const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry,
        SZrExecIrDiagnostic *diagnostic);

/* Append owned, compact recipe copies to an unpublished prepared target. */
TZrBool zr_exec_ir_deopt_aggregates_prepare(
        const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry,
        SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic);

#endif
