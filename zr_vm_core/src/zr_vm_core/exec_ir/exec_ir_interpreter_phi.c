#include "exec_ir_interpreter_internal.h"

#include <stdlib.h>

/* Phi inputs and predecessor IDs have matching positions. Count the selected
 * occurrence of a target in the source's successor row, then find the same
 * occurrence of that source in the target's predecessor row. */
static TZrBool zr_oracle_phi_edge_slot(const SZrExecIrFunction *function,
                                      TZrExecIrBlockId blockId,
                                      TZrExecIrBlockId previous,
                                      TZrUInt32 successorOrdinal,
                                      TZrUInt32 *outSlot,
                                      SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrBlock *source, *target = &function->blocks[blockId - 1u];
    TZrUInt32 occurrence = 0u, index;
    if (previous == ZR_EXEC_IR_BLOCK_ID_INVALID) {
        *outSlot = 0u;
        return ZR_TRUE;
    }
    source = &function->blocks[previous - 1u];
    if (successorOrdinal >= source->successorRange.count ||
        function->successors[source->successorRange.start + successorOrdinal] != blockId) {
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                       function, blockId, source->terminatorInstructionId,
                       0u, blockId, successorOrdinal);
        return ZR_FALSE;
    }
    for (index = 0u; index <= successorOrdinal; ++index) {
        if (function->successors[source->successorRange.start + index] == blockId)
            ++occurrence;
    }
    for (index = 0u; index < target->predecessorRange.count; ++index) {
        if (function->predecessors[target->predecessorRange.start + index] == previous &&
            --occurrence == 0u) {
            *outSlot = index;
            return ZR_TRUE;
        }
    }
    zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                   function, blockId, source->terminatorInstructionId,
                   0u, successorOrdinal + 1u, target->predecessorRange.count);
    return ZR_FALSE;
}

TZrBool zr_oracle_enter(const SZrExecIrFunction *function,
                       TZrExecIrBlockId blockId,
                       TZrExecIrBlockId previous,
                       TZrUInt32 successorOrdinal,
                       SZrExecIrOracleExecutionResult *result,
                       SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
    struct SZrOraclePhiPending {
        SZrExecIrOracleValue value;
        TZrUInt32 ownerState;
    } *pending;
    size_t bytes;
    TZrUInt32 index, edgeSlot;
    if (block->phis.count == 0u) return ZR_TRUE;
    if (!zr_oracle_phi_edge_slot(function, blockId, previous, successorOrdinal,
                                 &edgeSlot, diagnostic)) return ZR_FALSE;
    if (!zr_oracle_bytes(block->phis.count, sizeof(*pending), &bytes)) {
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       function, blockId, 0u, 0u, UINT32_MAX, block->phis.count);
        return ZR_FALSE;
    }
    pending = malloc(bytes);
    if (pending == ZR_NULL) {
        zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                       function, blockId, 0u, 0u, block->phis.count, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < block->phis.count; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[block->phis.start + index];
        if (edgeSlot >= phi->incomings.count) {
            free(pending);
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                           function, blockId, 0u, 0u, edgeSlot + 1u,
                           phi->incomings.count);
            return ZR_FALSE;
        }
        {
            TZrUInt32 input = function->phiIncoming[phi->incomings.start + edgeSlot].value - 1u;
            pending[index].value = result->values[input];
            pending[index].ownerState = result->ownerStates[input];
            if (pending[index].ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED ||
                pending[index].ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) {
                pending[index].ownerState = function->values[phi->result - 1u].ownership ==
                        ZR_EXEC_IR_OWNERSHIP_UNKNOWN ? ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN
                                                   : ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
            }
        }
    }
    for (index = 0u; index < block->phis.count; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[block->phis.start + index];
        result->values[phi->result - 1u] = pending[index].value;
        result->ownerStates[phi->result - 1u] = pending[index].ownerState;
    }
    free(pending);
    return ZR_TRUE;
}
