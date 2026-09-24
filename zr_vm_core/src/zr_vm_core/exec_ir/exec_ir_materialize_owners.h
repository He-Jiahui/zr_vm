#ifndef ZR_VM_CORE_EXEC_IR_MATERIALIZE_OWNERS_H
#define ZR_VM_CORE_EXEC_IR_MATERIALIZE_OWNERS_H

#include "zr_vm_core/exec_ir_state_map_liveness.h"

TZrBool zr_state_map_owners_valid(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic);
TZrBool zr_state_map_resolve_owners(
        const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared,
        SZrExecIrDiagnostic *diagnostic);

#endif
