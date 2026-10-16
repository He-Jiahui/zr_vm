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

/* A value-producing throwing terminator publishes its result only after the
 * operation has completed normally.  The exceptional successor is entered
 * without that result in the value environment.  Keep this edge check here,
 * next to the dominance check, because a block-level dominance relation alone
 * cannot express the instruction-level split at a throwing terminator. */
static TZrBool zr_exec_ir_ssa_result_unavailable_on_exception_edge(
        const SZrExecIrFunction *function,
        const SZrExecIrSsaDominance *dominance,
        TZrExecIrInstructionId definitionInstruction,
        TZrExecIrBlockId useBlock,
        TZrExecIrBlockId phiSuccessor,
        TZrBool *outOfMemory) {
    const SZrExecIrInstruction *definition;
    const SZrExecIrOpcodeInfo *definitionInfo;
    TZrExecIrBlockId definitionBlock;
    const SZrExecIrBlock *definitionContainer;
    SZrExecIrRange edgeRange;
    TZrUInt8 *visited = ZR_NULL;
    TZrExecIrBlockId *stack = ZR_NULL;
    TZrUInt32 stackCount = 0u;
    TZrUInt32 successorIndex;
    TZrBool hasExceptionalSuccessor = ZR_FALSE;

    if (outOfMemory != ZR_NULL) {
        *outOfMemory = ZR_FALSE;
    }

    if (function == ZR_NULL || dominance == ZR_NULL ||
        definitionInstruction == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
        definitionInstruction > function->instructionCount ||
        useBlock == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        useBlock > function->blockCount ||
        dominance->instructionBlocks == ZR_NULL) {
        return ZR_FALSE;
    }
    definition = &function->instructions[definitionInstruction - 1u];
    definitionInfo = ZrCore_ExecIr_OpcodeInfo(
            (EZrExecIrOpcode)definition->opcode);
    if (definitionInfo == ZR_NULL ||
        (definitionInfo->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) == 0u ||
        (definitionInfo->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) == 0u ||
        definition->results.count == 0u) {
        return ZR_FALSE;
    }
    definitionBlock = dominance->instructionBlocks[definitionInstruction - 1u];
    if (definitionBlock == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        definitionBlock > function->blockCount) {
        return ZR_FALSE;
    }
    definitionContainer = &function->blocks[definitionBlock - 1u];
    /* Early builders stored terminator edges only on the containing block.
     * Prefer the instruction-owned range when present, but accept that
     * compatibility representation while the structural phase is still the
     * authority for range validity. */
    edgeRange = definition->successorRange.count != 0u
                    ? definition->successorRange
                    : definitionContainer->successorRange;
    if (edgeRange.count == 0u || edgeRange.start > function->successorCount ||
        edgeRange.count > function->successorCount - edgeRange.start) {
        return ZR_FALSE;
    }

    /* A PHI consumes its value on the actual predecessor-to-successor edge.
     * The defining block alone cannot distinguish normal and exceptional
     * incoming edges from the same throwing terminator. */
    if (useBlock == definitionBlock &&
        phiSuccessor != ZR_EXEC_IR_BLOCK_ID_INVALID) {
        for (successorIndex = edgeRange.start;
             successorIndex < edgeRange.start + edgeRange.count;
             ++successorIndex) {
            TZrExecIrBlockId successor = function->successors[successorIndex];
            if (successor == phiSuccessor &&
                (function->blocks[successor - 1u].flags &
                 ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u) {
                return ZR_TRUE;
            }
        }
    }

    /* Seed only explicitly marked exceptional successors.  Once an exception
     * is raised, every block reachable from that successor is potentially on
     * the exceptional path, even if a handler branches through an ordinary
     * (unflagged) cleanup/join block.  This conservative closure is what
     * prevents a result from being used after an exceptional path rejoins. */
    for (successorIndex = edgeRange.start;
         successorIndex < edgeRange.start + edgeRange.count;
         ++successorIndex) {
        TZrExecIrBlockId successor = function->successors[successorIndex];
        if (successor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            successor > function->blockCount ||
            (function->blocks[successor - 1u].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 0u ||
            successor == definitionBlock) {
            continue;
        }
        if (successor == useBlock) {
            return ZR_TRUE;
        }
        hasExceptionalSuccessor = ZR_TRUE;
    }
    if (!hasExceptionalSuccessor) {
        return ZR_FALSE;
    }
    if (sizeof(*stack) > SIZE_MAX / (size_t)function->blockCount) {
        if (outOfMemory != ZR_NULL) {
            *outOfMemory = ZR_TRUE;
        }
        return ZR_FALSE;
    }
    visited = (TZrUInt8 *)calloc((size_t)function->blockCount,
                                 sizeof(*visited));
    stack = (TZrExecIrBlockId *)malloc((size_t)function->blockCount *
                                       sizeof(*stack));
    if (visited == ZR_NULL || stack == ZR_NULL) {
        free(visited);
        free(stack);
        if (outOfMemory != ZR_NULL) {
            *outOfMemory = ZR_TRUE;
        }
        return ZR_FALSE;
    }
    for (successorIndex = edgeRange.start;
         successorIndex < edgeRange.start + edgeRange.count;
         ++successorIndex) {
        TZrExecIrBlockId successor = function->successors[successorIndex];
        if (successor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            successor > function->blockCount) {
            continue;
        }
        if ((function->blocks[successor - 1u].flags &
             ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u &&
            visited[successor - 1u] == 0u) {
            visited[successor - 1u] = 1u;
            stack[stackCount++] = successor;
        }
    }
    while (stackCount != 0u) {
        TZrExecIrBlockId blockId = stack[--stackCount];
        const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
        /* Returning to the definition re-executes the throwing terminator.
         * Its normal edge establishes a new result; the earlier exceptional
         * path must not poison that result or a direct normal PHI incoming. */
        if (blockId == definitionBlock) {
            continue;
        }
        if (blockId == useBlock) {
            free(visited);
            free(stack);
            return ZR_TRUE;
        }
        for (successorIndex = block->successorRange.start;
             successorIndex < block->successorRange.start +
                               block->successorRange.count;
             ++successorIndex) {
            TZrExecIrBlockId successor = function->successors[successorIndex];
            if (successor != ZR_EXEC_IR_BLOCK_ID_INVALID &&
                successor <= function->blockCount &&
                visited[successor - 1u] == 0u) {
                visited[successor - 1u] = 1u;
                stack[stackCount++] = successor;
            }
        }
    }
    free(visited);
    free(stack);
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_ssa_report_exception_edge(
        SZrExecIrDiagnostic *diagnostic,
        const SZrExecIrFunction *function,
        TZrExecIrInstructionId instructionId,
        TZrExecIrBlockId blockId,
        TZrExecIrSourceId sourceId,
        TZrExecIrInstructionId definitionInstruction,
        TZrExecIrValueId valueId) {
    zr_exec_ir_set_diagnostic(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE,
                              function, instructionId, blockId,
                              definitionInstruction, valueId);
    if (diagnostic != ZR_NULL) {
        diagnostic->sourceId = sourceId;
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

    /* Parameters, captures and implicit frame roots exist at entry without an
     * ordinary defining instruction.  They remain distinct from missing defs. */
    for (instructionIndex = 0u;
         instructionIndex < function->valueCount;
         ++instructionIndex) {
        const SZrExecIrValue *value = &function->values[instructionIndex];
        if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) == 0u) {
            continue;
        }
        if (value->definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            free(definitionKinds);
            free(definitionInstructions);
            free(definitionBlocks);
            zr_exec_ir_set_diagnostic(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                    function, value->definition, function->entryBlockId,
                    value->id, value->id);
            return ZR_FALSE;
        }
        definitionKinds[value->id] = 3u;
        definitionBlocks[value->id] = function->entryBlockId;
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
            if (kind == 0u) {
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        function, instructionIndex + 1u, useBlock,
                        valueId, valueId);
                if (diagnostic != ZR_NULL) {
                    diagnostic->sourceId = instruction->sourceId;
                }
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                return ZR_FALSE;
            }
            /* A guarded cleanup observes the independently defined state
             * flag. The payload need not dominate this cleanup join. The
             * opcode's owner/cleanup shape was checked structurally above;
             * the undefined-ID rejection still applies. */
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) continue;
            if (kind == 1u) {
                TZrBool exceptionTraversalOutOfMemory = ZR_FALSE;
                if (zr_exec_ir_ssa_result_unavailable_on_exception_edge(
                            function, dominance, definitionInstructions[valueId],
                            useBlock, ZR_EXEC_IR_BLOCK_ID_INVALID,
                            &exceptionTraversalOutOfMemory)) {
                    zr_exec_ir_ssa_report_exception_edge(
                            diagnostic, function, instructionIndex + 1u,
                            useBlock, instruction->sourceId,
                            definitionInstructions[valueId], valueId);
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
                if (exceptionTraversalOutOfMemory) {
                    zr_exec_ir_set_diagnostic(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                            function, instructionIndex + 1u, useBlock,
                            function->blockCount, 0u);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->sourceId = instruction->sourceId;
                    }
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
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
            if (phi->incomings.count != block->predecessorRange.count) {
                TZrExecIrInstructionId site = block->terminatorInstructionId;
                zr_exec_ir_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                        function, site, block->id, block->predecessorRange.count,
                        phi->incomings.count);
                if (diagnostic != ZR_NULL &&
                    site != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                    site <= function->instructionCount) {
                    diagnostic->sourceId = function->instructions[site - 1u].sourceId;
                }
                free(definitionKinds);
                free(definitionInstructions);
                free(definitionBlocks);
                return ZR_FALSE;
            }
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start +
                                   phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming =
                        &function->phiIncoming[incomingIndex];
                TZrExecIrValueId valueId = incoming->value;
                TZrUInt8 kind = definitionKinds[valueId];
                TZrBool dominatesEdge = ZR_TRUE;
                TZrExecIrBlockId expectedPredecessor = function->predecessors[
                        block->predecessorRange.start + incomingIndex -
                        phi->incomings.start];
                if (incoming->predecessor != expectedPredecessor) {
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
                            expectedPredecessor,
                            incoming->predecessor);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->sourceId = sourceId;
                    }
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
                if (kind == 0u) {
                    TZrExecIrInstructionId site =
                            block->terminatorInstructionId;
                    zr_exec_ir_set_diagnostic(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                            function, site, block->id, valueId, valueId);
                    free(definitionKinds);
                    free(definitionInstructions);
                    free(definitionBlocks);
                    return ZR_FALSE;
                }
                if (kind == 1u || kind == 2u) {
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
                if (kind == 1u) {
                    TZrBool exceptionTraversalOutOfMemory = ZR_FALSE;
                    if (zr_exec_ir_ssa_result_unavailable_on_exception_edge(
                            function, dominance, definitionInstructions[valueId],
                            incoming->predecessor, block->id,
                            &exceptionTraversalOutOfMemory)) {
                        TZrExecIrInstructionId site = block->terminatorInstructionId;
                        TZrExecIrSourceId sourceId =
                                (site != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                                 site <= function->instructionCount)
                                        ? function->instructions[site - 1u].sourceId
                                        : 0u;
                        zr_exec_ir_ssa_report_exception_edge(
                                diagnostic, function, site, block->id, sourceId,
                                definitionInstructions[valueId], valueId);
                        free(definitionKinds);
                        free(definitionInstructions);
                        free(definitionBlocks);
                        return ZR_FALSE;
                    }
                    if (exceptionTraversalOutOfMemory) {
                        TZrExecIrInstructionId site = block->terminatorInstructionId;
                        TZrExecIrSourceId sourceId =
                                (site != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                                 site <= function->instructionCount)
                                        ? function->instructions[site - 1u].sourceId
                                        : 0u;
                        zr_exec_ir_set_diagnostic(
                                diagnostic,
                                ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                function, site, block->id,
                                function->blockCount, 0u);
                        if (diagnostic != ZR_NULL) {
                            diagnostic->sourceId = sourceId;
                        }
                        free(definitionKinds);
                        free(definitionInstructions);
                        free(definitionBlocks);
                        return ZR_FALSE;
                    }
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

/**
 * @fn static void zr_exec_ir_set_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 blockId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 在 SSA 验证的拒绝分支填写错误分类、函数身份及定义/使用上下文；它只更新本助手负责的字段，供上层选择性补充 sourceId。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空；非空须独立可写，保留本助手未赋值的原字段。
 * @param[in] code 本次拒绝的诊断分类。
 * @param[in] function 可空的只读借用；非空只读取 functionToken，空值写入 token0。
 * @param[in] instructionId 直接记录的指令上下文，0表示无指令位置；不在此校验范围。
 * @param[in] blockId 直接记录的块上下文，0表示无块位置。
 * @param[in] expected 写入 expectedVersion 的32位上下文，具体含义由调用分支决定。
 * @param[in] actual 写入 actualVersion 的32位上下文，可能是值ID、块ID或计数。
 *
 * ## Returns
 * 无返回值；空诊断不产生写入，非空时完成六个指定字段赋值。
 *
 * ## Constraints and lifecycle
 * 本函数不 memset，不写 sourceId，也不承诺重置诊断的其他字段。正常入口 VerifyFunction 已先清空诊断；本文件部分分支会随后覆盖 sourceId。需要完整新诊断的直接调用方须自行先初始化。diagnostic 不得覆盖 function 或当前验证仍要读取的输入池；不验证别名或地址可读性。不分配、不保存借用、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:7：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:17：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:338：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:627：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:724：本单元职责、调用或相关断言证据。
 */

/**
 * @typedef SZrExecIrSsaDominance
 *
 * ## Summary
 * 保存一次 SSA 校验私有的可达性和支配分析，使验证不需要相信或改写输入函数 cached immediateDominator；它不是公开ABI、持久缓存或运行时对象根。
 *
 * ## Fields
 * - reachable：按零基块序号保存入口可达标志，成功非零块分析具有 blockCount 字节。
 * - dominators：blockCount行、每行rowBytes字节；位定义同为零基块序号，只有可达块行参与交集，不可达块行保持0。
 * - instructionBlocks：instructionCount个1基块ID；有指令的非零块分析要求每条指令恰属于一个块；没有指令时可为NULL。
 * - rowBytes：构建时按块数取位图行宽，与两池行寻址共同解释；零块空结果为0。
 *
 * ## Invariants and lifecycle
 * 全零是可安全释放的空状态；build入口直接清零，所以复用持有池的记录前必须先Free。非零块成功时reachable与dominators均存在，instructionCount非0时instructionBlocks存在；每池独立分配且唯一由此记录拥有。
 * build失败已释放部分取得的池并清零，成功所有权交由私有入口在验证后Free。记录自身不会因Free失效，但旧池及所有元素/行借用会失效；不得按位复制形成双重所有者。
 * 三池只对应构建时那份函数的块/指令编号及CFG；输入结构变更后不得复用旧分析。它不拥有function，也不延长输入池生命周期；访问期间输入不得修改/释放，分析记录及写阶段须独占，无内部同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:25：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:76：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:102：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:133：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:191：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:206：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:222：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:240：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:312：构造、消费或统一释放边界。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:935：构造、消费或统一释放边界。
 */

/**
 * @fn static void zr_exec_ir_ssa_dominance_free(SZrExecIrSsaDominance *dominance)
 *
 * ## Summary
 * 收回一次 SSA 校验私有的可达性、支配集合与指令所属块分析；同时清零记录，使失败回退和正常结束使用相同释放出口。
 *
 * ## Parameters
 * @param[in,out] dominance 可空；非空须为空记录，或唯一拥有本模块成功/部分构造取得的三个可独立 free 的池。
 *
 * ## Returns
 * 无返回值；非空记录的旧池释放后所有字段清零，记录本身仍有效，旧池及元素借用失效。
 *
 * ## Constraints and lifecycle
 * 三个池的所有权不能通过按位复制交给多个释放者。可对已清零记录重复调用；不接受未初始化或外借且不可 free 的池。它不释放 function、不改其缓存分析。调用期间独占记录及其池，读者不得继续使用释放前借用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:32：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:108：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:211：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:935：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static void zr_exec_ir_ssa_set_bit(TZrUInt8 *row, TZrUInt32 bit)
 *
 * ## Summary
 * 向临时支配位图加入一个零基块序号，用于入口自身和每轮交集结果的自支配关系。
 *
 * ## Parameters
 * @param[in,out] row 覆盖 bit/8 所在字节的可读写行；修改该字节并保留其他位。
 * @param[in] bit 零基位序号，由已界定的块索引提供。
 *
 * ## Returns
 * 无返回值；对应位被置1，没有失败码或边界诊断。
 *
 * ## Constraints and lifecycle
 * 不检查空指针、行长度或位索引，调用方保证所选字节属于当前活的分析行。仅改临时位图，不能与仍需独立读取的数据重叠。无分配、无原子同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:41：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:249：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:297：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_test_bit(const TZrUInt8 *row, TZrUInt32 bit)
 *
 * ## Summary
 * 读取已构建支配集合的一位，作为块支配查询的最后一步。
 *
 * ## Parameters
 * @param[in] row 至少覆盖 bit/8 所在字节的只读借用。
 * @param[in] bit 合法零基位序号，不是1基 BlockId。
 *
 * ## Returns
 * @return TRUE 表示该位非零，FALSE 表示该位为0；返回按值布尔，不带资源。
 *
 * ## Constraints and lifecycle
 * 不校验指针或长度；调用方已排除非法/不可达块并选择相应 useBlock 行。借用仅用于本次读取，不延长分析池生命周期，不提供并发修改保护。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:46：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:327：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static void zr_exec_ir_ssa_mask_last_byte(TZrUInt8 *row, TZrUInt32 blockCount, TZrUInt32 rowBytes)
 *
 * ## Summary
 * 清除支配位图末字节中超出 blockCount 的填充位，使初始化为全1的候选集合只表示真实块。
 *
 * ## Parameters
 * @param[in,out] row 余数非0时须能读写 rowBytes-1 字节；余数为0时不访问行。
 * @param[in] blockCount 位图对应的块数；其除8余数决定合法末字节位。
 * @param[in] rowBytes 与 blockCount 对应的行字节数；余数非0时须大于0。
 *
 * ## Returns
 * 无返回值；仅在 blockCount 不是8的倍数时屏蔽末字节高位。
 *
 * ## Constraints and lifecycle
 * 不验证 rowBytes 与块数的一致性，也不检查存储。当前两处调用均使用构造函数计算的同一行宽；不得将不匹配行宽作为安全检查的替代。行由上层拥有，函数不分配或保留借用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:52：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:191：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:245：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:267：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_build_dominance( const SZrExecIrFunction *function, SZrExecIrSsaDominance *dominance, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为一次校验重新构建可达块和支配位图，并记录每条指令唯一所属块；不相信 function.blocks 中缓存的 immediateDominator，避免用待验证制品自报的分析作为证明。
 *
 * ## Parameters
 * @param[in] function 非空只读函数；块/指令及前驱后继池、逻辑范围、1基ID须已具备上层通用验证建立的安全读取条件。
 * @param[out] dominance 独立可写的空记录；入口直接清零，不会先释放旧池，已有所有权须先 Free。
 * @param[in,out] diagnostic 可空且独立于 function、其池和 dominance；失败经局部助手写相应字段，成功不主动清空。
 *
 * ## Returns
 * @return TRUE 表示分析已完整交付；零块时交付全零记录。FALSE 表示入口块、尺寸、分配或指令归属失败，此函数已释放取得的分析池并清零输出；诊断保留具体分支上下文。
 *
 * ## Constraints and lifecycle
 * 先用后继搜索入口可达闭包，再在可达前驱的支配集合上求交并加入自身；不可达块行保持0，忽略不可达前驱。非零块要求有效入口；所有指令必须恰属于一个块，重复归属和遗漏都拒绝。这里只重复检查自身需要的部分条件，不替代所有 CFG 结构验证；前驱/后继互反与终结器约束在可选 STRUCTURE 阶段。临时 stack/candidate 在所有返回路径释放；成功输出三池由唯一上层释放。输入及缓存不改，无锁，不允许输入被并发修改/释放；输出与输入及诊断须不重叠。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:62：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:76：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:156：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:175：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:222：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:254：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:307：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:930：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:740：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_block_dominates( const SZrExecIrSsaDominance *dominance, TZrExecIrBlockId definitionBlock, TZrExecIrBlockId useBlock, TZrUInt32 blockCount)
 *
 * ## Summary
 * 把1基定义块与使用块转为临时支配矩阵查询；先拒绝越界及入口不可达的块，再读取使用块的支配行。
 *
 * ## Parameters
 * @param[in] dominance 与 blockCount 及当前函数匹配的已完成分析借用；本函数不检查记录是否为空。
 * @param[in] definitionBlock 待证明的定义块ID；0或大于 blockCount 返回FALSE。
 * @param[in] useBlock 被查询使用块ID；0或大于 blockCount 返回FALSE。
 * @param[in] blockCount 与 reachable 长度及矩阵行数一致的真实块数。
 *
 * ## Returns
 * @return TRUE 表示两个块都可达且定义位在使用块的支配集合中；FALSE 也包括非法ID或任一块不可达。
 *
 * ## Constraints and lifecycle
 * 只有ID范围检查通过后才读取 reachable；未提供对 dominance 指针、行宽或分配来源的校验。自支配取决于构建结果，不读取缓存 immediateDominator。不分配、不修改、不保存借用；分析释放或重建后不能继续查询旧记录。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:312：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:318：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:325：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:759：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:766：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:854：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_report_dominance( SZrExecIrDiagnostic *diagnostic, const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId, TZrExecIrBlockId blockId, TZrExecIrSourceId sourceId, TZrExecIrInstructionId definitionInstruction, TZrExecIrValueId valueId)
 *
 * ## Summary
 * 统一生成定义未支配使用点/PHI边的 SSA 拒绝结果，补上实际使用位置的 sourceId。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空的独立可写诊断；更新本助手涉及字段，不整体清零。
 * @param[in] function 可空只读函数，供基础助手提取 functionToken。
 * @param[in] instructionId 使用点或PHI所在块终结器的指令位置，允许0。
 * @param[in] blockId 使用块或PHI目标块ID。
 * @param[in] sourceId 与拒绝使用点相匹配的来源ID，允许0。
 * @param[in] definitionInstruction 写入 expectedVersion 的普通定义指令ID；PHI定义无普通指令时可为0。
 * @param[in] valueId 写入 actualVersion 的使用值ID。
 *
 * ## Returns
 * @return 始终返回FALSE，方便上层以同一拒绝语义结束；本助手不接管或释放上层临时池。
 *
 * ## Constraints and lifecycle
 * 基础助手先写分类和位置，再仅在非空诊断时补 sourceId。上层负责释放三张定义表及外围支配分析。诊断不能与输入或这些仍活的表重叠，无分配、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:330：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:338：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:341：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:771：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:865：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_result_unavailable_on_exception_edge( const SZrExecIrFunction *function, const SZrExecIrSsaDominance *dominance, TZrExecIrInstructionId definitionInstruction, TZrExecIrBlockId useBlock, TZrExecIrBlockId phiSuccessor, TZrBool *outOfMemory)
 *
 * ## Summary
 * 检查带结果的 MAY_THROW 终结器结果是否被异常边及其后续路径使用；块支配不足以描述正常完成才发布结果的语义，因此普通使用和PHI前驱边都需额外检查。
 *
 * ## Parameters
 * @param[in] function 只读函数借用；显式空值/部分ID不适用时返回FALSE，但其余可读池和CFG范围/ID仍须由调用方保证。
 * @param[in] dominance 与函数匹配的分析借用；空值或缺 instructionBlocks 返回FALSE。
 * @param[in] definitionInstruction 1基普通定义指令ID；无效ID返回FALSE。
 * @param[in] useBlock 普通使用块，或PHI的实际前驱块；无效ID返回FALSE。
 * @param[in] phiSuccessor PHI目标块ID；普通使用传INVALID。用于区分同一定义块的正常/异常出边。
 * @param[out] outOfMemory 可空独立布尔槽；若非空，入口先清为FALSE，尺寸/分配失败置TRUE；调用者须读取它区别否定结果。
 *
 * ## Returns
 * @return TRUE 表示存在本次定义尚未正常发布结果的异常路径。FALSE 表示不适用、未找到该路径，或遍历无法分配；后一种由 outOfMemory=TRUE 区分，不能把FALSE单独解释为验证成功。
 *
 * ## Constraints and lifecycle
 * 直接PHI边先识别指向异常标记目标的边；普通路径从显式 EXCEPTION 后继出发，继续经过普通cleanup/join块。回到定义块时停止该路径，因重新执行终结器的正常边会建立新结果；同址异常PHI仍由前置边检查拒绝。优先使用指令 successorRange，空时兼容所属块范围；不提供任意畸形CFG的完整验证，特别直接PHI分支仍依赖安全后继ID。visited/stack仅在需要遍历时分配，所有返回前释放；不把借用或结果表发布给caller。当前两处调用均提供局部 outOfMemory 并把失败转为OOM拒绝。诊断、output和输入别名未动态检查；输入须稳定，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:347：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:370：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:401：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:412：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:450：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:456：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:483：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:489：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:726：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:739：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:875：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:893：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_invoke_result_availability.c:176：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_invoke_result_availability.c:260：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_ssa_report_exception_edge( SZrExecIrDiagnostic *diagnostic, const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId, TZrExecIrBlockId blockId, TZrExecIrSourceId sourceId, TZrExecIrInstructionId definitionInstruction, TZrExecIrValueId valueId)
 *
 * ## Summary
 * 把异常路径使用尚未正常发布的结果统一记为 EXCEPTION_EDGE，并附上定义/使用身份；这里只报告，不改变CFG或修复值环境。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空、独立可写；更新分类、身份和sourceId，未触及字段仍保留。
 * @param[in] function 可空只读函数，基础助手读取其 token或写0。
 * @param[in] instructionId 普通使用指令或PHI目标终结器位置。
 * @param[in] blockId 拒绝的使用块或PHI目标块ID。
 * @param[in] sourceId 拒绝位置来源ID，允许0。
 * @param[in] definitionInstruction 写入 expectedVersion 的抛出终结器定义ID。
 * @param[in] valueId 写入 actualVersion 的不可用结果ID。
 *
 * ## Returns
 * @return 始终返回FALSE；不自动释放caller的定义表或支配分析。
 *
 * ## Constraints and lifecycle
 * 它仅在异常可达查询返回TRUE后调用；遍历分配失败走独立OOM分支，不能混作EXCEPTION_EDGE。空诊断无写入；非空诊断不整体清零且不得别名输入或仍需释放的临时记录。无分配、无借用保留、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:515：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:523：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:527：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:730：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:885：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:1285：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:1479：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_ssa_with_dominance( const SZrExecIrFunction *function, const SZrExecIrSsaDominance *dominance, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在临时支配事实基础上收集入口外部值、普通结果和PHI结果，拒绝重复定义或未定义使用，并分别验证普通指令次序、跨块支配、PHI实际前驱边和异常结果可用性。
 *
 * ## Parameters
 * @param[in] function 非空、已完成上层通用值ID/池/范围及条件清理形状检查的只读函数；本助手不重复这些全部检查。
 * @param[in] dominance 与当前函数相同的成功分析借用；有块时 instructionBlocks 和支配行必须完整匹配。
 * @param[in,out] diagnostic 可空独立输出；各失败分支填写上下文，成功不主动清空旧内容。
 *
 * ## Returns
 * @return TRUE 表示本助手检查通过；valueCount0直接通过，不表示整个函数/CFG/状态图合法。FALSE 表示定义、支配、PHI边、异常结果或分配失败；三张本地定义表均在返回前释放，输入和分析不变。
 *
 * ## Constraints and lifecycle
 * definitionKinds 的0/1/2/3分别表示缺失、普通结果、PHI结果、EXTERNAL_ENTRY。外部值须无普通definition，普通结果不可重定义，PHI结果也不可与已收集定义冲突。只在实际使用时拒绝缺定义，不承诺每个闲置非外部值都有定义；上层先保证值ID可安全索引。普通同块/线性使用要求定义指令严格更早，PHI结果从块入口可用；PHI incoming按前驱数组位置逐项对应，允许同源并行边的不同出现。DROP_IF_INITIALIZED先做未定义ID拒绝，再跳过普通payload支配及异常可用性检查，所需owner/cleanup形状由前置检查负责。异常遍历FALSE+OOM不能当可用。全部分配为本次私有scratch；不发布、不修改cached dominance，不提供锁；输入、分析和diagnostic须保持独立稳定。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:533：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:544：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:579：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:602：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:654：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:699：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:723：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:739：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:783：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:821：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:893：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:918：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:632：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:823：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:935：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:1116：本单元职责、调用或相关断言证据。
 */

/**
 * @fn TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 作为 VerifyFunction 的私有 SSA 阶段入口，先构建本次独立支配分析，再校验定义与使用，最后统一释放分析；不依赖或更新函数缓存的支配信息。
 *
 * ## Parameters
 * @param[in] function 非空、在整个调用期间稳定可读的函数借用；由 VerifyFunction 的通用验证先保证池、范围、值/块/边ID可安全访问。
 * @param[in,out] diagnostic 可空独立诊断；错误时按SSA分支写字段，成功不在本入口主动清零。
 *
 * ## Returns
 * @return TRUE 表示支配构建和SSA检查通过，FALSE 表示任一阶段拒绝或OOM；所有本入口取得的临时分析池均在自身/下层返回前释放，无资源交给调用方。
 *
 * ## Constraints and lifecycle
 * 不是可直接接受任意NULL/畸形存储的独立公开验证器：function在构造分析时直接解引用。VerifyFunction先清诊断并做通用检查，是否附加CFG互反/终结器检查由STRUCTURE level决定；SSA通过不能外推其它level或随后stateMap通过。不改变函数池、generation或cached immediateDominator，不延长其生命周期。diagnostic不得与function或读取池重叠；无锁，禁止调用期间修改/释放输入。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:924：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:929：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:930：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:933：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:935：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.h:6：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:724：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909：本单元职责、调用或相关断言证据。
 */
