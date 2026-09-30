#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_common/zr_type_conf.h"
#include "exec_ir_execbc_vm_internal.h"

static TZrBool execbc_vm_phi_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool execbc_vm_phi_type_supported(TZrExecIrTypeToken typeToken) {
    return (TZrBool)(typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                     typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL);
}

static TZrBool execbc_vm_phi_result_slot(
        const SZrExecBcProjection *projection,
        const SZrExecIrPhi *phi,
        TZrUInt32 *slot,
        TZrExecIrTypeToken *typeToken) {
    return execbc_vm_value_slot(projection, phi->result, slot, typeToken);
}

static TZrBool execbc_vm_phi_find_result(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block,
        TZrExecIrValueId result,
        const SZrExecIrPhi **outPhi) {
    TZrUInt32 phiIndex;
    for (phiIndex = 0u; phiIndex < block->phis.count; ++phiIndex) {
        const SZrExecIrPhi *phi =
                &projection->phis[block->phis.start + phiIndex];
        if (phi->result == result) {
            *outPhi = phi;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool execbc_vm_validate_phi_instruction(
        const SZrExecBcProjection *projection,
        TZrUInt32 instructionIndex,
        TZrExecIrBlockId blockId,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecBcInstruction *instruction =
            &projection->instructions[instructionIndex];
    const SZrExecBcBlock *block = execbc_vm_block_at(projection, blockId);
    const SZrExecIrPhi *phi = ZR_NULL;
    TZrUInt32 resultIndex;
    TZrUInt32 incomingIndex;

    if (block == ZR_NULL || instruction->results.count != 1u ||
        !execbc_vm_phi_find_result(
                projection, block,
                projection->results[instruction->results.start], &phi) ||
        instruction->operands.count != phi->incomings.count ||
        (instruction->phiRange.count != 0u &&
         instruction->phiRange.count != phi->incomings.count) ||
        instruction->successorRange.count != 0u ||
        instruction->matchTypeToken != 0u || instruction->layoutId != 0u) {
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                              projection, blockId, instructionIndex + 1u,
                              instruction->sourceId,
                              block != ZR_NULL ? block->predecessors.count : 0u,
                              instruction->operands.count);
    }

    resultIndex = instruction->results.start;
    {
        TZrUInt32 resultSlot;
        TZrExecIrTypeToken resultType;
        if (!execbc_vm_phi_result_slot(projection, phi,
                                       &resultSlot, &resultType) ||
            resultSlot >= projection->physicalSlotCount ||
            projection->results[resultIndex] != phi->result) {
            return execbc_vm_fail(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                    projection, blockId, instructionIndex + 1u,
                    instruction->sourceId, phi->result,
                    projection->results[resultIndex]);
        }
        if (instruction->typeToken != 0u &&
            instruction->typeToken != resultType) {
            return execbc_vm_fail(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                    projection, blockId, instructionIndex + 1u,
                    instruction->sourceId, resultType,
                    instruction->typeToken);
        }
    }
    for (incomingIndex = 0u; incomingIndex < phi->incomings.count;
         ++incomingIndex) {
        const SZrExecIrPhiIncoming *incoming =
                &projection->phiIncomings[phi->incomings.start + incomingIndex];
        TZrExecIrValueId operand =
                projection->operands[instruction->operands.start + incomingIndex];
        if (operand != incoming->value) {
            return execbc_vm_fail(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                  projection, blockId, instructionIndex + 1u,
                                  instruction->sourceId, incoming->value, operand);
        }
        if (instruction->phiRange.count != 0u) {
            const SZrExecIrPhiIncoming *instructionIncoming =
                    &projection->phiIncomings[
                            instruction->phiRange.start + incomingIndex];
            if (instructionIncoming->predecessor != incoming->predecessor ||
                instructionIncoming->value != incoming->value) {
                return execbc_vm_fail(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                        projection, blockId, instructionIndex + 1u,
                        instruction->sourceId, incoming->value,
                        instructionIncoming->value);
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool execbc_vm_phi_move_destination_is_target(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block,
        TZrUInt32 slot) {
    TZrUInt32 phiIndex;
    for (phiIndex = 0u; phiIndex < block->phis.count; ++phiIndex) {
        TZrUInt32 resultSlot;
        TZrExecIrTypeToken resultType;
        const SZrExecIrPhi *phi =
                &projection->phis[block->phis.start + phiIndex];
        if (execbc_vm_phi_result_slot(projection, phi,
                                      &resultSlot, &resultType) &&
            resultSlot == slot &&
            execbc_vm_phi_type_supported(resultType)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool execbc_vm_validate_phi_edge_moves(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *target,
        TZrExecIrBlockId edgeId,
        TZrUInt32 stackSize,
        TZrUInt32 *symbols,
        TZrBool *moveConsumed,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 slot;
    TZrUInt32 moveIndex;
    TZrUInt32 incomingIndex;
    TZrUInt32 temporarySymbol = UINT32_MAX;
    const SZrExecBcBlock *edge = execbc_vm_block_at(projection, edgeId);

    if (edge == ZR_NULL || edge->successors.count != 1u ||
        projection->successors[edge->successors.start] != target->id) {
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                              projection, target->id, 0u, 0u,
                              target->id,
                              edge != ZR_NULL && edge->successors.count != 0u
                                      ? projection->successors[
                                                edge->successors.start]
                                      : edgeId);
    }

    for (slot = 0u; slot < projection->physicalSlotCount; ++slot) {
        symbols[slot] = slot;
    }
    for (moveIndex = 0u; moveIndex < projection->phiMoveCount; ++moveIndex) {
        const SZrExecBcPhiMove *move = &projection->phiMoves[moveIndex];
        TZrUInt32 sourceSymbol;
        if (move->edge != edgeId) {
            continue;
        }
        if (move->sourceSlot >= stackSize ||
            move->destinationSlot >= stackSize ||
            (move->sourceSlot >= projection->physicalSlotCount &&
             (projection->temporarySlotCount == 0u ||
              move->sourceSlot != projection->phiTemporarySlot)) ||
            (move->destinationSlot >= projection->physicalSlotCount &&
             (projection->temporarySlotCount == 0u ||
              move->destinationSlot != projection->phiTemporarySlot)) ||
            (move->destinationSlot < projection->physicalSlotCount &&
             !execbc_vm_phi_move_destination_is_target(
                     projection, target, move->destinationSlot))) {
            return execbc_vm_fail(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                  projection, target->id, 0u, 0u,
                                  projection->physicalSlotCount,
                                  move->destinationSlot);
        }
        if (move->sourceSlot == projection->phiTemporarySlot &&
            projection->temporarySlotCount != 0u) {
            sourceSymbol = temporarySymbol;
        } else {
            sourceSymbol = symbols[move->sourceSlot];
        }
        if (sourceSymbol == UINT32_MAX) {
            return execbc_vm_fail(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                  projection, target->id, 0u, 0u,
                                  projection->phiTemporarySlot,
                                  move->sourceSlot);
        }
        if (move->destinationSlot == projection->phiTemporarySlot &&
            projection->temporarySlotCount != 0u) {
            temporarySymbol = sourceSymbol;
        } else {
            symbols[move->destinationSlot] = sourceSymbol;
        }
        moveConsumed[moveIndex] = ZR_TRUE;
    }

    for (incomingIndex = 0u; incomingIndex < target->predecessors.count;
         ++incomingIndex) {
        if (projection->predecessors[
                    target->predecessors.start + incomingIndex] != edgeId) {
            continue;
        }
        {
            TZrUInt32 phiIndex;
            for (phiIndex = 0u; phiIndex < target->phis.count; ++phiIndex) {
                const SZrExecIrPhi *phi =
                        &projection->phis[target->phis.start + phiIndex];
                const SZrExecIrPhiIncoming *incoming =
                        &projection->phiIncomings[
                                phi->incomings.start + incomingIndex];
                TZrUInt32 sourceSlot = 0u;
                TZrUInt32 destinationSlot = 0u;
                TZrExecIrTypeToken sourceType = 0u;
                TZrExecIrTypeToken destinationType = 0u;
                if (!execbc_vm_value_slot(projection, incoming->value,
                                          &sourceSlot, &sourceType) ||
                    !execbc_vm_phi_result_slot(
                            projection, phi, &destinationSlot,
                            &destinationType) ||
                    destinationSlot >= projection->physicalSlotCount ||
                    sourceType != destinationType ||
                    symbols[destinationSlot] != sourceSlot) {
                    return execbc_vm_fail(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            projection, target->id, 0u, 0u,
                            sourceSlot, destinationSlot);
                }
            }
        }
    }
    return ZR_TRUE;
}

TZrBool execbc_vm_validate_phi_data(
        const SZrExecBcProjection *projection,
        const TZrUInt32 *instructionOwners,
        TZrUInt32 stackSize,
        SZrExecIrDiagnostic *diagnostic) {
    TZrBool *moveConsumed = ZR_NULL;
    TZrBool *phiResultSeen = ZR_NULL;
    TZrUInt32 *symbols = ZR_NULL;
    TZrUInt32 blockIndex;
    TZrUInt32 expectedCopyIndex = 0u;
    TZrUInt32 instructionIndex;
    TZrUInt32 moveIndex;

    if (projection->phiMoveCount != 0u) {
        if ((size_t)projection->phiMoveCount >
            SIZE_MAX / sizeof(*moveConsumed)) {
            return execbc_vm_fail(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  projection, 0u, 0u, 0u,
                                  (TZrUInt32)(SIZE_MAX / sizeof(*moveConsumed)),
                                  projection->phiMoveCount);
        }
        moveConsumed = (TZrBool *)calloc(projection->phiMoveCount,
                                         sizeof(*moveConsumed));
        if (moveConsumed == ZR_NULL) {
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  projection, 0u, 0u, 0u,
                                  projection->phiMoveCount, 0u);
        }
    }
    if (projection->physicalSlotCount != 0u) {
        if ((size_t)projection->physicalSlotCount >
            SIZE_MAX / sizeof(*symbols)) {
            free(moveConsumed);
            return execbc_vm_fail(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  projection, 0u, 0u, 0u,
                                  (TZrUInt32)(SIZE_MAX / sizeof(*symbols)),
                                  projection->physicalSlotCount);
        }
        symbols = (TZrUInt32 *)malloc(
                (size_t)projection->physicalSlotCount * sizeof(*symbols));
        phiResultSeen = (TZrBool *)calloc(projection->physicalSlotCount,
                                          sizeof(*phiResultSeen));
        if (symbols == ZR_NULL || phiResultSeen == ZR_NULL) {
            free(moveConsumed);
            free(symbols);
            free(phiResultSeen);
            return execbc_vm_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  projection, 0u, 0u, 0u,
                                  projection->physicalSlotCount, 0u);
        }
    }

    for (blockIndex = 0u; blockIndex < projection->blockCount; ++blockIndex) {
        const SZrExecBcBlock *block = &projection->blocks[blockIndex];
        TZrUInt32 predIndex;
        TZrUInt32 phiIndex;
        if (block->phis.count != 0u && block->predecessors.count == 0u) {
            free(moveConsumed);
            free(symbols);
            free(phiResultSeen);
            return execbc_vm_fail(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                    projection, block->id, 0u, 0u, 1u, 0u);
        }
        for (phiIndex = 0u; phiIndex < block->phis.count; ++phiIndex) {
            const SZrExecIrPhi *phi =
                    &projection->phis[block->phis.start + phiIndex];
            TZrUInt32 resultSlot;
            TZrExecIrTypeToken resultType;
            TZrUInt32 incomingIndex;
            if (!execbc_vm_phi_range_valid(phi->incomings,
                                           projection->phiIncomingCount) ||
                phi->incomings.count != block->predecessors.count ||
                !execbc_vm_phi_result_slot(projection, phi,
                                           &resultSlot, &resultType) ||
                !execbc_vm_phi_type_supported(resultType) ||
                phiResultSeen[resultSlot] != ZR_FALSE) {
                free(moveConsumed);
                free(symbols);
                free(phiResultSeen);
                return execbc_vm_fail(
                        diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                        projection, block->id, 0u, 0u,
                        block->predecessors.count,
                        phi->incomings.count);
            }
            phiResultSeen[resultSlot] = ZR_TRUE;
            for (incomingIndex = 0u; incomingIndex < phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming =
                        &projection->phiIncomings[
                                phi->incomings.start + incomingIndex];
                TZrUInt32 sourceSlot;
                TZrExecIrTypeToken sourceType;
                TZrExecIrBlockId expectedPredecessor =
                        projection->predecessors[
                                block->predecessors.start + incomingIndex];
                if (incoming->predecessor != expectedPredecessor ||
                    !execbc_vm_value_slot(projection, incoming->value,
                                          &sourceSlot, &sourceType) ||
                    sourceType != resultType) {
                    free(moveConsumed);
                    free(symbols);
                    free(phiResultSeen);
                    return execbc_vm_fail(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            projection, block->id, 0u, 0u,
                            expectedPredecessor, incoming->predecessor);
                }
            }
        }

        for (predIndex = 0u; predIndex < block->predecessors.count;
             ++predIndex) {
            TZrExecIrBlockId edgeId = projection->predecessors[
                    block->predecessors.start + predIndex];
            TZrUInt32 earlierPredIndex;
            for (earlierPredIndex = 0u;
                 block->phis.count != 0u && earlierPredIndex < predIndex;
                 ++earlierPredIndex) {
                if (projection->predecessors[
                            block->predecessors.start + earlierPredIndex] ==
                    edgeId) {
                    free(moveConsumed);
                    free(symbols);
                    free(phiResultSeen);
                    return execbc_vm_fail(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            projection, block->id, 0u, 0u,
                            earlierPredIndex + 1u, predIndex + 1u);
                }
            }
            for (phiIndex = 0u; phiIndex < block->phis.count; ++phiIndex) {
                const SZrExecIrPhi *phi =
                        &projection->phis[block->phis.start + phiIndex];
                const SZrExecIrPhiIncoming *incoming =
                        &projection->phiIncomings[
                                phi->incomings.start + predIndex];
                if (incoming->value == phi->result) {
                    continue;
                }
                if (expectedCopyIndex >= projection->phiCopyCount ||
                    projection->phiCopySources[expectedCopyIndex] !=
                            incoming->value ||
                    projection->phiCopyDestinations[expectedCopyIndex] !=
                            phi->result ||
                    projection->phiCopyEdges[expectedCopyIndex] != edgeId) {
                    TZrUInt32 actualSource =
                            expectedCopyIndex < projection->phiCopyCount
                                    ? projection->phiCopySources[expectedCopyIndex]
                                    : 0u;
                    free(moveConsumed);
                    free(symbols);
                    free(phiResultSeen);
                    return execbc_vm_fail(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            projection, block->id, 0u, 0u,
                            incoming->value, actualSource);
                }
                ++expectedCopyIndex;
            }
            if (block->phis.count != 0u &&
                !execbc_vm_validate_phi_edge_moves(
                        projection, block, edgeId, stackSize, symbols,
                        moveConsumed, diagnostic)) {
                free(moveConsumed);
                free(symbols);
                free(phiResultSeen);
                return ZR_FALSE;
            }
        }
    }
    if (expectedCopyIndex != projection->phiCopyCount) {
        TZrUInt32 actualCount = projection->phiCopyCount;
        free(moveConsumed);
        free(symbols);
        free(phiResultSeen);
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                              projection, 0u, 0u, 0u,
                              expectedCopyIndex, actualCount);
    }
    for (moveIndex = 0u; moveIndex < projection->phiMoveCount; ++moveIndex) {
        if (moveConsumed[moveIndex] == ZR_FALSE) {
            TZrExecIrBlockId edge = projection->phiMoves[moveIndex].edge;
            free(moveConsumed);
            free(symbols);
            free(phiResultSeen);
            return execbc_vm_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                    projection, edge, 0u, 0u, 1u, 0u);
        }
    }

    for (instructionIndex = 0u;
         instructionIndex < projection->instructionCount; ++instructionIndex) {
        if (projection->instructions[instructionIndex].opcode ==
            (TZrUInt16)ZR_EXEC_IR_OPCODE_PHI &&
            !execbc_vm_validate_phi_instruction(
                    projection, instructionIndex,
                    instructionOwners[instructionIndex], diagnostic)) {
            free(moveConsumed);
            free(symbols);
            free(phiResultSeen);
            return ZR_FALSE;
        }
    }

    free(moveConsumed);
    free(symbols);
    free(phiResultSeen);
    return ZR_TRUE;
}
