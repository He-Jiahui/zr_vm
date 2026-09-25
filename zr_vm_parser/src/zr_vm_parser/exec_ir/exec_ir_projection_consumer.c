#include "zr_vm_parser/exec_ir_projections.h"

#include <string.h>

TZrBool ZrParser_ExecBcProjection_ExecutePhiMoves(
        const SZrExecBcProjection *projection, TZrExecIrBlockId edge,
        TZrUInt32 *slots, TZrUInt32 slotCount,
        FZrExecBcPhiMoveConsumer consumer, void *userData,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (projection == ZR_NULL ||
        projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG ||
        edge == ZR_EXEC_IR_BLOCK_ID_INVALID || edge > projection->blockCount ||
        consumer == ZR_NULL || (slotCount != 0u && slots == ZR_NULL)) {
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    for (index = 0u; index < projection->phiMoveCount; ++index) {
        const SZrExecBcPhiMove *move = &projection->phiMoves[index];
        if (move->edge != edge) continue;
        if (move->sourceSlot >= slotCount || move->destinationSlot >= slotCount) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION;
                diagnostic->blockId = edge;
                diagnostic->actualVersion = index;
            }
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < projection->phiMoveCount; ++index) {
        const SZrExecBcPhiMove *move = &projection->phiMoves[index];
        if (move->edge == edge &&
            !consumer(userData, move->destinationSlot, move->sourceSlot)) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION;
                diagnostic->blockId = edge;
                diagnostic->actualVersion = index;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
