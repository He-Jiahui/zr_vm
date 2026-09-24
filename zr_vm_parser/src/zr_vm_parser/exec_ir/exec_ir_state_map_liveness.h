#ifndef ZR_VM_PARSER_EXEC_IR_STATE_MAP_LIVENESS_H
#define ZR_VM_PARSER_EXEC_IR_STATE_MAP_LIVENESS_H

#include "zr_vm_core/exec_ir.h"

/* Private analysis of an already verified function. Rows use zero-based
 * instruction indices and bits use zero-based value indices. */
typedef struct SZrStateMapLiveness {
    TZrUInt8 *before;
    TZrUInt8 *after;
    size_t rowBytes;
} SZrStateMapLiveness;

EZrExecutionDiagnosticCode zr_state_map_liveness_build(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness);
void zr_state_map_liveness_free(SZrStateMapLiveness *liveness);
TZrBool zr_state_map_liveness_contains(const SZrStateMapLiveness *liveness,
                                        TZrExecIrInstructionId instruction,
                                        TZrExecIrValueId value,
                                        TZrBool after);

#endif
