#ifndef ZR_VM_CORE_EXEC_IR_OWNER_STATE_H
#define ZR_VM_CORE_EXEC_IR_OWNER_STATE_H

#include "zr_vm_core/exec_ir_state_map.h"

/* Transient analysis storage, never serialized into an artifact. Build into
 * an empty object, free after use, and do not mutate the function meanwhile. */
typedef struct SZrExecIrOwnerAnalysis {
    TZrUInt8 *before;
    TZrUInt8 *after;
    TZrUInt8 *reachable;
    TZrUInt32 valueCount;
    TZrUInt32 instructionCount;
} SZrExecIrOwnerAnalysis;

ZR_CORE_API TZrBool ZrCore_ExecIr_OwnerAnalysisBuild(
        const SZrExecIrFunction *function, SZrExecIrOwnerAnalysis *analysis,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_ExecIr_OwnerAnalysisFree(SZrExecIrOwnerAnalysis *analysis);
/* COUNT means unreachable, uninitialized, ambiguous, or an invalid query. */
ZR_CORE_API EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);

#endif
