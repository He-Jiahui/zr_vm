#ifndef ZR_VM_PARSER_EXEC_IR_EXECBC_VM_H
#define ZR_VM_PARSER_EXEC_IR_EXECBC_VM_H

#include "zr_vm_core/execution_contract.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser/exec_ir_projections.h"

typedef struct SZrExecBcVmPcMapEntry {
    TZrUInt32 pc;
    TZrExecIrInstructionId instructionId;
    TZrExecIrBlockId blockId;
    TZrExecIrSourceId sourceId;
} SZrExecBcVmPcMapEntry;

typedef struct SZrExecBcVmEmission {
    SZrFunction *function;                 /* Core GC managed; caller roots it immediately. */
    SZrExecBcVmPcMapEntry *pcMap;          /* Native owned by this emission. */
    TZrUInt32 pcMapCount;
} SZrExecBcVmEmission;

/* Emit the supported scalar/control subset as an ordinary Core VM function.
 * The output record must be fresh and zero-cleared. Release an earlier
 * emission with ZrParser_ExecBcVmEmission_Free before reusing that record.
 * On success the function is temporarily rooted only during construction;
 * the caller must root it before performing any operation that can collect.
 * Free() releases only the native PC map and clears this record; Core owns
 * the successful function until State_Destroy. On failure output stays empty
 * and a partially constructed function is freed after allocation failures. If
 * revoking a newly added GC root fails, the function remains Core managed
 * until state teardown so a possibly registered root cannot point at freed
 * storage. */
ZR_PARSER_API TZrBool ZrParser_ExecBcProjection_MaterializeVmFunction(
        SZrState *state, const SZrExecBcProjection *projection,
        SZrExecBcVmEmission *output, SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API void ZrParser_ExecBcVmEmission_Free(
        SZrState *state, SZrExecBcVmEmission *emission);

#endif
