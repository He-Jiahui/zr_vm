#include "exec_ir_verify_ssa.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                      EZrExecutionDiagnosticCode code,
                                      const SZrExecIrFunction *function,
                                      TZrUInt32 instructionId,
                                      TZrUInt32 blockId,
                                      TZrUInt32 expected,
                                      TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->blockId = blockId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

typedef struct SZrExecIrSsaDominance {
    TZrUInt8 *reachable;
    TZrUInt8 *dominators;
    TZrExecIrBlockId *instructionBlocks;
    TZrUInt32 rowBytes;
} SZrExecIrSsaDominance;

static void zr_exec_ir_ssa_dominance_free(SZrExecIrSsaDominance *dominance) {
    if (dominance != ZR_NULL) {
        free(dominance->reachable);
        free(dominance->dominators);
        free(dominance->instructionBlocks);
        memset(dominance, 0, sizeof(*dominance));
    }
}

static void zr_exec_ir_ssa_set_bit(TZrUInt8 *row, TZrUInt32 bit) {
    row[bit / 8u] = (TZrUInt8)(row[bit / 8u] |
                               (TZrUInt8)(1u << (bit % 8u)));
}

static TZrBool zr_exec_ir_ssa_test_bit(const TZrUInt8 *row,
                                        TZrUInt32 bit) {
    return (TZrBool)((row[bit / 8u] &
                      (TZrUInt8)(1u << (bit % 8u))) != 0u);
}

static void zr_exec_ir_ssa_mask_last_byte(TZrUInt8 *row,
                                           TZrUInt32 blockCount,
                                           TZrUInt32 rowBytes) {
    TZrUInt32 remainder = blockCount % 8u;
    if (remainder != 0u) {
        row[rowBytes - 1u] = (TZrUInt8)(row[rowBytes - 1u] &
                                        (TZrUInt8)((1u << remainder) - 1u));
    }
}

/* Build dominance sets without mutating the function's cached analysis
 * fields.  Verification may run on an artifact produced by another process,
 * so immediateDominator is treated as an untrusted hint rather than proof. */
static TZrBool zr_exec_ir_ssa_build_dominance(
        const SZrExecIrFunction *function,
        SZrExecIrSsaDominance *dominance,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrBlockId *stack = ZR_NULL;
    TZrUInt8 *candidate = ZR_NULL;
    size_t matrixBytes;
    TZrUInt32 stackCount = 0u;
    TZrUInt32 blockIndex;
    TZrBool changed;

    memset(dominance, 0, sizeof(*dominance));
    if (function->blockCount == 0u) {
        return ZR_TRUE;
    }
    if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        function->entryBlockId > function->blockCount) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  function,
                                  0u,
                                  function->entryBlockId,
                                  ZR_EXEC_IR_BLOCK_ID_ENTRY,
                                  function->entryBlockId);
        return ZR_FALSE;
    }
    if (function->blockCount != 0u &&
        sizeof(*stack) > SIZE_MAX / (size_t)function->blockCount) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  function,
                                  0u,
                                  0u,
                                  UINT32_MAX,
                                  function->blockCount);
        return ZR_FALSE;
    }
    dominance->reachable = (TZrUInt8 *)calloc(
            (size_t)function->blockCount, sizeof(*dominance->reachable));
    stack = (TZrExecIrBlockId *)malloc(
            (size_t)function->blockCount * sizeof(*stack));
    if (dominance->reachable == ZR_NULL || stack == ZR_NULL) {
        free(stack);
        zr_exec_ir_ssa_dominance_free(dominance);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  function,
                                  0u,
                                  0u,
                                  function->blockCount,
                                  0u);
        return ZR_FALSE;
    }

    if (function->instructionCount != 0u) {
        if (sizeof(*dominance->instructionBlocks) >
            SIZE_MAX / (size_t)function->instructionCount) {
            free(stack);
            zr_exec_ir_ssa_dominance_free(dominance);
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                      function,
                                      0u,
                                      0u,
                                      UINT32_MAX,
                                      function->instructionCount);
            return ZR_FALSE;
        }
        dominance->instructionBlocks = (TZrExecIrBlockId *)calloc(
                (size_t)function->instructionCount,
                sizeof(*dominance->instructionBlocks));
        if (dominance->instructionBlocks == ZR_NULL) {
            free(stack);
            zr_exec_ir_ssa_dominance_free(dominance);
            zr_exec_ir_set_diagnostic(diagnostic,
                                      ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                      function,
                                      0u,
                                      0u,
                                      function->instructionCount,
                                      0u);
            return ZR_FALSE;
        }
        for (blockIndex = 0u; blockIndex < function->blockCount;
             ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 instructionIndex;
            for (instructionIndex = block->instructionRange.start;
                 instructionIndex < block->instructionRange.start +
                                     block->instructionRange.count;
                 ++instructionIndex) {
                if (dominance->instructionBlocks[instructionIndex] !=
                    ZR_EXEC_IR_BLOCK_ID_INVALID) {
                    free(stack);
                    zr_exec_ir_ssa_dominance_free(dominance);
                    zr_exec_ir_set_diagnostic(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                            function, instructionIndex + 1u, block->id, 0u,
                            block->id);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->sourceId =
                                function->instructions[instructionIndex].sourceId;
                    }
                    return ZR_FALSE;
                }
                dominance->instructionBlocks[instructionIndex] = block->id;
            }
        }
        for (blockIndex = 0u; blockIndex < function->instructionCount;
             ++blockIndex) {
            if (dominance->instructionBlocks[blockIndex] ==
                ZR_EXEC_IR_BLOCK_ID_INVALID) {
                free(stack);
                zr_exec_ir_ssa_dominance_free(dominance);
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                        function, blockIndex + 1u, 0u, function->blockCount,
                        ZR_EXEC_IR_BLOCK_ID_INVALID);
                if (diagnostic != ZR_NULL) {
                    diagnostic->sourceId = function->instructions[blockIndex].sourceId;
                }
                return ZR_FALSE;
            }
        }
    }

    dominance->rowBytes =
            (TZrUInt32)(((size_t)function->blockCount + 7u) / 8u);
    if ((size_t)function->blockCount > SIZE_MAX / dominance->rowBytes) {
        free(stack);
        zr_exec_ir_ssa_dominance_free(dominance);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  function,
                                  0u,
                                  0u,
                                  UINT32_MAX,
                                  function->blockCount);
        return ZR_FALSE;
    }
    matrixBytes = (size_t)function->blockCount * dominance->rowBytes;
    dominance->dominators = (TZrUInt8 *)calloc(matrixBytes, sizeof(TZrUInt8));
    candidate = (TZrUInt8 *)malloc(dominance->rowBytes);
    if (dominance->dominators == ZR_NULL || candidate == ZR_NULL) {
        free(candidate);
        free(stack);
        zr_exec_ir_ssa_dominance_free(dominance);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  function,
                                  0u,
                                  0u,
                                  (TZrUInt32)matrixBytes,
                                  0u);
        return ZR_FALSE;
    }

    dominance->reachable[function->entryBlockId - 1u] = 1u;
    stack[stackCount++] = function->entryBlockId;
    while (stackCount != 0u) {
        TZrExecIrBlockId blockId = stack[--stackCount];
        const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
        TZrUInt32 successorIndex;
        for (successorIndex = block->successorRange.start;
             successorIndex < block->successorRange.start +
                               block->successorRange.count;
             ++successorIndex) {
            TZrExecIrBlockId successor = function->successors[successorIndex];
            if (dominance->reachable[successor - 1u] == 0u) {
                dominance->reachable[successor - 1u] = 1u;
                stack[stackCount++] = successor;
            }
        }
    }

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        TZrUInt8 *row = dominance->dominators +
                        (size_t)blockIndex * dominance->rowBytes;
        if (dominance->reachable[blockIndex] != 0u) {
            memset(row, 0xff, dominance->rowBytes);
            zr_exec_ir_ssa_mask_last_byte(row, function->blockCount,
                                          dominance->rowBytes);
            if (blockIndex + 1u == function->entryBlockId) {
                memset(row, 0, dominance->rowBytes);
                zr_exec_ir_ssa_set_bit(row, blockIndex);
            }
        }
    }

    do {
        changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < function->blockCount;
             ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 predecessorIndex;
            TZrBool havePredecessor = ZR_FALSE;
            TZrUInt8 *row;
            if (dominance->reachable[blockIndex] == 0u ||
                block->id == function->entryBlockId) {
                continue;
            }
            memset(candidate, 0xff, dominance->rowBytes);
            zr_exec_ir_ssa_mask_last_byte(candidate, function->blockCount,
                                          dominance->rowBytes);
            for (predecessorIndex = block->predecessorRange.start;
                 predecessorIndex < block->predecessorRange.start +
                                     block->predecessorRange.count;
                 ++predecessorIndex) {
                TZrExecIrBlockId predecessor =
                        function->predecessors[predecessorIndex];
                const TZrUInt8 *predecessorRow;
                TZrUInt32 byteIndex;
                if (dominance->reachable[predecessor - 1u] == 0u) {
                    continue;
                }
                predecessorRow = dominance->dominators +
                                 (size_t)(predecessor - 1u) *
                                         dominance->rowBytes;
                if (!havePredecessor) {
                    memcpy(candidate, predecessorRow, dominance->rowBytes);
                    havePredecessor = ZR_TRUE;
                } else {
                    for (byteIndex = 0u; byteIndex < dominance->rowBytes;
                         ++byteIndex) {
                        candidate[byteIndex] = (TZrUInt8)(candidate[byteIndex] &
                                                         predecessorRow[byteIndex]);
                    }
                }
            }
            if (!havePredecessor) {
                memset(candidate, 0, dominance->rowBytes);
            }
            zr_exec_ir_ssa_set_bit(candidate, blockIndex);
            row = dominance->dominators +
                  (size_t)blockIndex * dominance->rowBytes;
            if (memcmp(row, candidate, dominance->rowBytes) != 0) {
                memcpy(row, candidate, dominance->rowBytes);
                changed = ZR_TRUE;
            }
        }
    } while (changed != ZR_FALSE);

    free(candidate);
    free(stack);
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_ssa_block_dominates(
        const SZrExecIrSsaDominance *dominance,
        TZrExecIrBlockId definitionBlock,
        TZrExecIrBlockId useBlock,
        TZrUInt32 blockCount) {
    const TZrUInt8 *row;
    if (definitionBlock == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        useBlock == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        definitionBlock > blockCount || useBlock > blockCount ||
        dominance->reachable[definitionBlock - 1u] == 0u ||
        dominance->reachable[useBlock - 1u] == 0u) {
        return ZR_FALSE;
    }
    row = dominance->dominators +
          (size_t)(useBlock - 1u) * dominance->rowBytes;
    return zr_exec_ir_ssa_test_bit(row, definitionBlock - 1u);
}

static TZrBool zr_exec_ir_ssa_report_dominance(
        SZrExecIrDiagnostic *diagnostic,
        const SZrExecIrFunction *function,
        TZrExecIrInstructionId instructionId,
        TZrExecIrBlockId blockId,
        TZrExecIrSourceId sourceId,
        TZrExecIrInstructionId definitionInstruction,
        TZrExecIrValueId valueId) {
    zr_exec_ir_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE,
                              function, instructionId, blockId,
                              definitionInstruction, valueId);
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = sourceId;
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_ssa_block_has_predecessor(
        const SZrExecIrFunction *function,
        const SZrExecIrBlock *block,
        TZrExecIrBlockId predecessor) {
    TZrUInt32 index;

    for (index = block->predecessorRange.start;
         index < block->predecessorRange.start +
                         block->predecessorRange.count;
         ++index) {
        if (function->predecessors[index] == predecessor) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_verify_ssa_with_dominance(
        const SZrExecIrFunction *function,
        const SZrExecIrSsaDominance *dominance,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt8 *definitionKinds = ZR_NULL;
    TZrExecIrInstructionId *definitionInstructions = ZR_NULL;
    TZrExecIrBlockId *definitionBlocks = ZR_NULL;
    size_t valueSlots;
    TZrUInt32 instructionIndex;
    TZrUInt32 blockIndex;

    if (function->valueCount == 0u) {
        return ZR_TRUE;
    }
    if ((size_t)function->valueCount + 1u >
        SIZE_MAX / sizeof(*definitionKinds)) {
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                  function,
                                  0u,
                                  0u,
                                  UINT32_MAX,
                                  function->valueCount);
        return ZR_FALSE;
    }
    valueSlots = (size_t)function->valueCount + 1u;
    definitionKinds = (TZrUInt8 *)calloc(valueSlots, sizeof(*definitionKinds));
    definitionInstructions = (TZrExecIrInstructionId *)calloc(
            valueSlots, sizeof(*definitionInstructions));
    definitionBlocks = (TZrExecIrBlockId *)calloc(valueSlots,
                                                   sizeof(*definitionBlocks));
    if (definitionKinds == ZR_NULL || definitionInstructions == ZR_NULL ||
        definitionBlocks == ZR_NULL) {
        free(definitionKinds);
        free(definitionInstructions);
        free(definitionBlocks);
        zr_exec_ir_set_diagnostic(diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                  function,
                                  0u,
                                  0u,
                                  function->valueCount,
                                  0u);
        return ZR_FALSE;
    }

    /* Collect ordinary instruction definitions first. */
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrExecIrBlockId blockId = function->blockCount != 0u
                                       ? dominance->instructionBlocks[instructionIndex]
                                       : ZR_EXEC_IR_BLOCK_ID_INVALID;
        TZrUInt32 resultIndex;
        for (resultIndex = instruction->resultRange.start;
             resultIndex < instruction->resultRange.start +
                            instruction->resultRange.count;
             ++resultIndex) {
            TZrExecIrValueId valueId = function->results[resultIndex];
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                valueId > function->valueCount) {
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        function, instructionIndex + 1u, blockId,
                        function->valueCount, valueId);
                if (diagnostic != ZR_NULL) {
                    diagnostic->sourceId = instruction->sourceId;
                }
                return ZR_FALSE;
            }
            if (definitionKinds[valueId] != 0u ||
                (function->values[valueId - 1u].definition !=
                         ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                 function->values[valueId - 1u].definition !=
                         instructionIndex + 1u)) {
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                        function, instructionIndex + 1u, blockId, valueId,
                        valueId);
                if (diagnostic != ZR_NULL) {
                    diagnostic->sourceId = instruction->sourceId;
                }
                return ZR_FALSE;
            }
            definitionKinds[valueId] = 1u;
            definitionInstructions[valueId] = instructionIndex + 1u;
            definitionBlocks[valueId] = blockId;
        }
    }

    /* A PHI defines its result at the destination block entry.  It has no
     * ordinary instruction ID, so keep a separate kind and block identity. */
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrExecIrValueId valueId = phi->result;
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
                valueId > function->valueCount) {
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        function, 0u, block->id, function->valueCount,
                        valueId);
                return ZR_FALSE;
            }
            if (definitionKinds[valueId] != 0u) {
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                        function, 0u, block->id, valueId, valueId);
                return ZR_FALSE;
            }
            definitionKinds[valueId] = 2u;
            definitionBlocks[valueId] = block->id;
        }
    }

    /* Ordinary operands must be dominated by their definition. */
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrExecIrBlockId useBlock = function->blockCount != 0u
                                        ? dominance->instructionBlocks[instructionIndex]
                                        : ZR_EXEC_IR_BLOCK_ID_INVALID;
        TZrUInt32 operandIndex;
        for (operandIndex = instruction->operandRange.start;
             operandIndex < instruction->operandRange.start +
                             instruction->operandRange.count;
             ++operandIndex) {
            TZrExecIrValueId valueId = function->operands[operandIndex];
            TZrUInt8 kind = definitionKinds[valueId];
            TZrBool dominatesUse = ZR_TRUE;
            if (kind == 1u) {
                if (function->blockCount == 0u) {
                    dominatesUse = (TZrBool)(definitionInstructions[valueId] <
                                              instructionIndex + 1u);
                } else if (definitionBlocks[valueId] == useBlock) {
                    dominatesUse = (TZrBool)(definitionInstructions[valueId] <
                                              instructionIndex + 1u);
                } else {
                    dominatesUse = zr_exec_ir_ssa_block_dominates(
                            dominance, definitionBlocks[valueId], useBlock,
                            function->blockCount);
                }
            } else if (kind == 2u) {
                /* A PHI result is available at the beginning of its defining
                 * block, so ordinary uses still require block dominance. */
                dominatesUse = zr_exec_ir_ssa_block_dominates(
                        dominance, definitionBlocks[valueId], useBlock,
                        function->blockCount);
            }
            if (kind != 0u && !dominatesUse) {
                zr_exec_ir_ssa_report_dominance(
                        diagnostic, function, instructionIndex + 1u, useBlock,
                        instruction->sourceId, definitionInstructions[valueId],
                        valueId);
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                return ZR_FALSE;
            }
        }
    }

    /* A PHI incoming is evaluated on its predecessor edge, not in the
     * destination block.  Check that edge explicitly. */
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start +
                                   phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming =
                        &function->phiIncoming[incomingIndex];
                TZrExecIrValueId valueId = incoming->value;
                TZrUInt8 kind = definitionKinds[valueId];
                TZrBool dominatesEdge = ZR_TRUE;
                if (!zr_exec_ir_ssa_block_has_predecessor(
                            function, block, incoming->predecessor)) {
                    TZrExecIrInstructionId site = block->terminatorInstructionId;
                    TZrExecIrSourceId sourceId =
                            (site != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                             site <= function->instructionCount)
                                    ? function->instructions[site - 1u].sourceId
                                    : 0u;
                    zr_exec_ir_set_diagnostic(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            function, site, block->id,
                            block->predecessorRange.count,
                            incoming->predecessor);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->sourceId = sourceId;
                    }
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
                if (kind != 0u) {
                    dominatesEdge = zr_exec_ir_ssa_block_dominates(
                            dominance, definitionBlocks[valueId],
                            incoming->predecessor, function->blockCount);
                }
                if (kind != 0u && !dominatesEdge) {
                    TZrExecIrInstructionId site = block->terminatorInstructionId;
                    TZrExecIrSourceId sourceId =
                            (site != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                             site <= function->instructionCount)
                                    ? function->instructions[site - 1u].sourceId
                                    : 0u;
                    zr_exec_ir_ssa_report_dominance(
                            diagnostic, function, site, block->id, sourceId,
                            definitionInstructions[valueId], valueId);
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
            }
        }
    }

    free(definitionKinds);
    free(definitionInstructions);
    free(definitionBlocks);
    return ZR_TRUE;
}

TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function,
                              SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrSsaDominance dominance;
    TZrBool result;

    memset(&dominance, 0, sizeof(dominance));
    if (!zr_exec_ir_ssa_build_dominance(function, &dominance, diagnostic)) {
        return ZR_FALSE;
    }
    result = zr_exec_ir_verify_ssa_with_dominance(function, &dominance,
                                                  diagnostic);
    zr_exec_ir_ssa_dominance_free(&dominance);
    return result;
}
