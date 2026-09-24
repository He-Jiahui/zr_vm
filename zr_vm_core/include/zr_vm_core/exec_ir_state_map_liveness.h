#ifndef ZR_VM_CORE_EXEC_IR_STATE_MAP_LIVENESS_H
#define ZR_VM_CORE_EXEC_IR_STATE_MAP_LIVENESS_H

#include "zr_vm_core/exec_ir_owner_state.h"

/* Private analysis of an already verified function. Rows use zero-based
 * instruction indices and bits use zero-based value indices. */
typedef struct SZrStateMapLiveness {
    TZrUInt8 *before;
    TZrUInt8 *after;
    TZrUInt8 *semanticBefore;
    TZrUInt8 *semanticAfter;
    size_t rowBytes;
} SZrStateMapLiveness;

ZR_CORE_API EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness);
ZR_CORE_API void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness,
                                        TZrExecIrInstructionId instruction,
                                        TZrExecIrValueId value,
                                        TZrBool after);

ZR_CORE_API TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);
/* COUNT rejects unavailable semantic reads. Conditional/inactive states
 * are permitted only for UNIQUE/SHARED cleanup-only liveness. */
ZR_CORE_API EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);

#endif
