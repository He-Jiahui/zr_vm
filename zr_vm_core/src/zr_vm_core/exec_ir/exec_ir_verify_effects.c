#include "zr_vm_core/exec_ir.h"
#include "exec_ir_verify_effect_backedges.h"

#include <stdlib.h>
#include <string.h>

static void zr_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic,
                                   EZrExecutionDiagnosticCode code,
                                   const SZrExecIrFunction *function,
                                   TZrUInt32 blockId,
                                   TZrUInt32 instructionId,
                                   TZrUInt32 expected,
                                   TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = instructionId != 0u && instructionId <= function->instructionCount
                               ? function->instructions[instructionId - 1u].sourceId
                               : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrExecIrBlockId zr_exec_ir_containing_block(const SZrExecIrFunction *function,
                                                    TZrUInt32 instructionIndex) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (instructionIndex >= block->instructions.start &&
            instructionIndex - block->instructions.start < block->instructions.count) {
            return block->id;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool zr_exec_ir_range_valid(SZrExecIrRange range,
                                      TZrUInt32 count,
                                      const void *pool) {
    if (range.start > count || range.count > count - range.start) {
        return ZR_FALSE;
    }
    if (range.count != 0u && pool == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_memory_token_valid(
        TZrExecIrMemoryTokenId token) {
    if (token == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) return ZR_FALSE;
    if (!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) return ZR_TRUE;
    return (TZrBool)(ZR_EXEC_IR_MEMORY_TOKEN_REGION(token) <
                         ZR_EXEC_IR_MEMORY_CLASS_COUNT &&
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(token) != 0u);
}

static TZrBool zr_exec_ir_memory_token_matches(
        TZrExecIrMemoryTokenId token, TZrUInt32 regionMask) {
    if (!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) return ZR_TRUE;
    return (TZrBool)((regionMask &
                      ((TZrUInt32)1u << ZR_EXEC_IR_MEMORY_TOKEN_REGION(token))) != 0u);
}

static TZrBool zr_exec_ir_effect_storage_valid(const SZrExecIrFunction *function,
                                               SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;

    if ((function->valueCount != 0u && function->values == ZR_NULL) ||
        function->valueCount > function->valueCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        function->instructionCount > function->instructionCapacity ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        function->blockCount > function->blockCapacity ||
        (function->operandCount != 0u && function->operandPool == ZR_NULL) ||
        function->operandCount > function->operandCapacity ||
        (function->resultCount != 0u && function->resultPool == ZR_NULL) ||
        function->resultCount > function->resultCapacity ||
        (function->memoryTokenCount != 0u && function->memoryTokenPool == ZR_NULL) ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        function->phiCount > function->phiCapacity ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        function->predecessorCount > function->predecessorCapacity ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        function->successorCount > function->successorCapacity) {
        zr_exec_ir_effect_diag(diagnostic,
                               ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                               function,
                               ZR_EXEC_IR_BLOCK_ID_INVALID,
                               0u,
                               0u,
                               0u);
        return ZR_FALSE;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (!zr_exec_ir_range_valid(block->instructions,
                                    function->instructionCount,
                                    function->instructions) ||
            !zr_exec_ir_range_valid(block->predecessors,
                                    function->predecessorCount,
                                    function->predecessors) ||
            !zr_exec_ir_range_valid(block->successors,
                                    function->successorCount,
                                    function->successors) ||
            !zr_exec_ir_range_valid(block->phis,
                                    function->phiCount,
                                    function->phiPool) ||
            !zr_exec_ir_range_valid(block->effectPhiIncomings,
                                    function->phiIncomingCount,
                                    function->phiIncoming)) {
            zr_exec_ir_effect_diag(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function,
                                   block->id,
                                   0u,
                                   0u,
                                   0u);
            return ZR_FALSE;
        }
        if ((block->effectPhiResult == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
            (block->effectPhiIncomings.count == 0u)) {
            zr_exec_ir_effect_diag(diagnostic,
                                   ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                   function, block->id, 0u,
                                   block->effectPhiResult,
                                   block->effectPhiIncomings.count);
            return ZR_FALSE;
        }
        {
            TZrUInt32 region;
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (!zr_exec_ir_range_valid(block->memoryPhiIncomings[region],
                                            function->phiIncomingCount,
                                            function->phiIncoming) ||
                    ((block->memoryPhiResults[region] ==
                      ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) !=
                     (block->memoryPhiIncomings[region].count == 0u))) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, block->id, 0u,
                                           function->phiIncomingCount,
                                           block->memoryPhiIncomings[region].count);
                    return ZR_FALSE;
                }
                if (block->memoryPhiResults[region] !=
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    (!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(
                             block->memoryPhiResults[region]) ||
                     ZR_EXEC_IR_MEMORY_TOKEN_REGION(
                             block->memoryPhiResults[region]) !=
                        (EZrExecIrMemoryClass)region ||
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                             block->memoryPhiResults[region]) == 0u)) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, block->id, 0u, region,
                                           block->memoryPhiResults[region]);
                    return ZR_FALSE;
                }
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_exec_ir_block_has_predecessor(const SZrExecIrFunction *function,
                                                const SZrExecIrBlock *block,
                                                TZrExecIrBlockId predecessor) {
    TZrUInt32 index;
    for (index = block->predecessors.start;
         index < block->predecessors.start + block->predecessors.count;
         ++index) {
        if (function->predecessors[index] == predecessor) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_verify_phi_predecessors(const SZrExecIrFunction *function,
                                                  SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        if (!zr_exec_ir_range_valid(block->predecessors, function->predecessorCount,
                                    function->predecessors) ||
            !zr_exec_ir_range_valid(block->phis, function->phiCount, function->phiPool)) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function, block->id, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            if (!zr_exec_ir_range_valid(phi->incomings, function->phiIncomingCount,
                                        function->phiIncoming)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                       function, block->id, 0u, 0u, 0u);
                return ZR_FALSE;
            }
            if (phi->incomings.count != block->predecessors.count) {
                zr_exec_ir_effect_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                       function,
                                       block->id,
                                       0u,
                                       block->predecessors.count,
                                       phi->incomings.count);
                return ZR_FALSE;
            }
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start + phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[incomingIndex];
                TZrUInt32 predecessorIndex = incomingIndex - phi->incomings.start;
                if (!zr_exec_ir_block_has_predecessor(function, block, incoming->predecessor)) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                           function,
                                           block->id,
                                           0u,
                                           block->predecessors.count,
                                           incoming->predecessor);
                    return ZR_FALSE;
                }
                if (function->predecessors[block->predecessors.start + predecessorIndex] !=
                    incoming->predecessor) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                           function,
                                           block->id,
                                           0u,
                                           function->predecessors[block->predecessors.start + predecessorIndex],
                                           incoming->predecessor);
                    return ZR_FALSE;
                }
                /* Parallel edges from the same source have separate ordered
                 * incoming slots. Position, not source-block uniqueness,
                 * identifies the predecessor edge occurrence. */
            }
        }
    }
    return ZR_TRUE;
}

static TZrUInt16 zr_exec_ir_required_flags(const SZrExecIrOpcodeInfo *info,
                                           const SZrExecIrInstruction *instruction) {
    TZrUInt16 required = 0u;
    (void)instruction;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_THROW;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_GC;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    return required;
}

static TZrBool zr_exec_ir_instruction_observable(
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOpcodeInfo *info) {
    TZrUInt16 required;
    if (info == ZR_NULL) return ZR_FALSE;
    required = zr_exec_ir_required_flags(info, instruction);
    return (TZrBool)(info->memoryWrites != 0u || required != 0u ||
                     (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u);
}

static TZrExecIrMemoryTokenId *zr_exec_ir_memory_cfg_slot(
        TZrExecIrMemoryTokenId *storage,
        TZrUInt32 blockIndex,
        TZrUInt32 region) {
    return &storage[(size_t)blockIndex * ZR_EXEC_IR_MEMORY_CLASS_COUNT + region];
}

static TZrBool *zr_exec_ir_memory_cfg_known_slot(
        TZrBool *storage,
        TZrUInt32 blockIndex,
        TZrUInt32 region) {
    return &storage[(size_t)blockIndex * ZR_EXEC_IR_MEMORY_CLASS_COUNT + region];
}

static TZrBool zr_exec_ir_verify_memory_cfg(
        const SZrExecIrFunction *function,
        const TZrBool *backedges,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrMemoryTokenId *terminal;
    TZrBool *known;
    size_t slotCount;
    TZrUInt32 blockIndex;
    TZrUInt32 iteration;
    TZrBool valid = ZR_TRUE;

    if (function->blockCount == 0u) return ZR_TRUE;
#if SIZE_MAX <= UINT32_MAX
    if ((size_t)function->blockCount >
            SIZE_MAX / ZR_EXEC_IR_MEMORY_CLASS_COUNT / sizeof(*terminal)) {
        zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               function, ZR_EXEC_IR_BLOCK_ID_INVALID, 0u, 0u, 0u);
        return ZR_FALSE;
    }
#endif
    slotCount = (size_t)function->blockCount * ZR_EXEC_IR_MEMORY_CLASS_COUNT;
    terminal = (TZrExecIrMemoryTokenId *)calloc(slotCount, sizeof(*terminal));
    known = (TZrBool *)calloc(slotCount, sizeof(*known));
    if (terminal == ZR_NULL || known == ZR_NULL) {
        free(terminal);
        free(known);
        zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, ZR_EXEC_IR_BLOCK_ID_INVALID, 0u, 0u, 0u);
        return ZR_FALSE;
    }

    for (iteration = 0u; iteration <= function->blockCount; ++iteration) {
        TZrBool changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrExecIrMemoryTokenId current[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
            TZrBool currentKnown[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
            TZrUInt32 region;
            TZrUInt32 instructionIndex;

            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (block->memoryPhiResults[region] !=
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
                    current[region] = block->memoryPhiResults[region];
                    currentKnown[region] = ZR_TRUE;
                } else if (block->predecessors.count == 0u) {
                    currentKnown[region] = ZR_TRUE;
                } else {
                    TZrBool haveIncoming = ZR_FALSE;
                    TZrExecIrMemoryTokenId incoming =
                            ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
                    TZrUInt32 predecessorIndex;
                    currentKnown[region] = ZR_TRUE;
                    for (predecessorIndex = block->predecessors.start;
                         predecessorIndex < block->predecessors.start +
                                               block->predecessors.count;
                         ++predecessorIndex) {
                        TZrExecIrBlockId predecessor =
                                function->predecessors[predecessorIndex];
                        if (!*zr_exec_ir_memory_cfg_known_slot(
                                    known, predecessor - 1u, region)) {
                            currentKnown[region] = ZR_FALSE;
                            break;
                        }
                        if (!haveIncoming) {
                            incoming = *zr_exec_ir_memory_cfg_slot(
                                    terminal, predecessor - 1u, region);
                            haveIncoming = ZR_TRUE;
                        } else if (incoming != *zr_exec_ir_memory_cfg_slot(
                                               terminal, predecessor - 1u, region)) {
                            currentKnown[region] = ZR_FALSE;
                            break;
                        }
                    }
                    if (currentKnown[region]) current[region] = incoming;
                }
            }
            for (instructionIndex = block->instructions.start;
                 instructionIndex < block->instructions.start +
                                       block->instructions.count;
                 ++instructionIndex) {
                const SZrExecIrInstruction *instruction =
                        &function->instructions[instructionIndex];
                TZrUInt32 tokenIndex;
                if (zr_exec_ir_range_valid(instruction->memoryIn,
                                            function->memoryTokenCount,
                                            function->memoryTokenPool)) {
                    for (tokenIndex = instruction->memoryIn.start;
                         tokenIndex < instruction->memoryIn.start +
                                           instruction->memoryIn.count;
                         ++tokenIndex) {
                        TZrExecIrMemoryTokenId token =
                                function->memoryTokenPool[tokenIndex];
                        if (zr_exec_ir_memory_token_valid(token) &&
                            ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) {
                            region = (TZrUInt32)ZR_EXEC_IR_MEMORY_TOKEN_REGION(token);
                            if (current[region] == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
                                current[region] = token;
                                currentKnown[region] = ZR_TRUE;
                            }
                        }
                    }
                }
                if (!zr_exec_ir_range_valid(instruction->memoryOut,
                                             function->memoryTokenCount,
                                             function->memoryTokenPool)) {
                    continue;
                }
                for (tokenIndex = instruction->memoryOut.start;
                     tokenIndex < instruction->memoryOut.start +
                                       instruction->memoryOut.count;
                     ++tokenIndex) {
                    TZrExecIrMemoryTokenId token =
                            function->memoryTokenPool[tokenIndex];
                    if (zr_exec_ir_memory_token_valid(token)) {
                        region = (TZrUInt32)ZR_EXEC_IR_MEMORY_TOKEN_REGION(token);
                        if (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) {
                            current[region] = token;
                            currentKnown[region] = ZR_TRUE;
                        }
                    }
                }
            }
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                TZrExecIrMemoryTokenId *oldToken =
                        zr_exec_ir_memory_cfg_slot(terminal, blockIndex, region);
                TZrBool *oldKnown =
                        zr_exec_ir_memory_cfg_known_slot(known, blockIndex, region);
                if (*oldToken != current[region] || *oldKnown != currentKnown[region]) {
                    *oldToken = current[region];
                    *oldKnown = currentKnown[region];
                    changed = ZR_TRUE;
                }
            }
        }
        if (!changed) break;
    }

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 region;
        TZrUInt32 instructionIndex;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            TZrExecIrMemoryTokenId phiResult = block->memoryPhiResults[region];
            TZrExecIrMemoryTokenId firstInput =
                    ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
            TZrExecIrMemoryTokenId firstOutput =
                    ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
            TZrBool hasMemoryTouch = ZR_FALSE;
            TZrBool hasMemoryInput = ZR_FALSE;
            TZrUInt32 firstInstruction = 0u;
            TZrUInt32 predecessorIndex;

            for (instructionIndex = block->instructions.start;
                 instructionIndex < block->instructions.start +
                                       block->instructions.count;
                 ++instructionIndex) {
                const SZrExecIrInstruction *instruction =
                        &function->instructions[instructionIndex];
                TZrUInt32 tokenIndex;
                if (zr_exec_ir_range_valid(instruction->memoryIn,
                                           function->memoryTokenCount,
                                           function->memoryTokenPool)) {
                    for (tokenIndex = instruction->memoryIn.start;
                         tokenIndex < instruction->memoryIn.start +
                                           instruction->memoryIn.count;
                         ++tokenIndex) {
                    TZrExecIrMemoryTokenId token =
                            function->memoryTokenPool[tokenIndex];
                    if (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token) &&
                        ZR_EXEC_IR_MEMORY_TOKEN_REGION(token) ==
                            (EZrExecIrMemoryClass)region &&
                        !hasMemoryTouch) {
                        firstInput = token;
                        hasMemoryInput = ZR_TRUE;
                        hasMemoryTouch = ZR_TRUE;
                        firstInstruction = instructionIndex + 1u;
                    }
                }
                }
                if (zr_exec_ir_range_valid(instruction->memoryOut,
                                           function->memoryTokenCount,
                                           function->memoryTokenPool)) {
                    for (tokenIndex = instruction->memoryOut.start;
                         tokenIndex < instruction->memoryOut.start +
                                           instruction->memoryOut.count;
                         ++tokenIndex) {
                    TZrExecIrMemoryTokenId token =
                            function->memoryTokenPool[tokenIndex];
                    if (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token) &&
                        ZR_EXEC_IR_MEMORY_TOKEN_REGION(token) ==
                            (EZrExecIrMemoryClass)region &&
                        !hasMemoryTouch) {
                        firstOutput = token;
                        hasMemoryTouch = ZR_TRUE;
                        firstInstruction = instructionIndex + 1u;
                    }
                }
                }
            }

            if (phiResult != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) {
                TZrUInt32 maximumVersion = 0u;
                if (block->memoryPhiIncomings[region].count !=
                    block->predecessors.count) {
                    zr_exec_ir_effect_diag(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                            function, block->id, firstInstruction,
                            block->predecessors.count,
                            block->memoryPhiIncomings[region].count);
                    valid = ZR_FALSE;
                    break;
                }
                for (predecessorIndex = 0u;
                     predecessorIndex < block->predecessors.count;
                     ++predecessorIndex) {
                    const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[
                            block->memoryPhiIncomings[region].start + predecessorIndex];
                    TZrExecIrBlockId predecessor = function->predecessors[
                            block->predecessors.start + predecessorIndex];
                    TZrExecIrMemoryTokenId expected = *zr_exec_ir_memory_cfg_slot(
                            terminal, predecessor - 1u, region);
                    TZrBool backedge = backedges[
                            block->predecessors.start + predecessorIndex];
                    if (incoming->predecessor != predecessor ||
                        !*zr_exec_ir_memory_cfg_known_slot(
                            known, predecessor - 1u, region) ||
                        incoming->value != expected ||
                        ((!ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(incoming->value) &&
                          !(incoming->value ==
                                ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID && !backedge)) ||
                         (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(incoming->value) &&
                          ZR_EXEC_IR_MEMORY_TOKEN_REGION(incoming->value) !=
                              (EZrExecIrMemoryClass)region))) {
                        zr_exec_ir_effect_diag(
                                diagnostic,
                                incoming->predecessor != predecessor
                                    ? ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH
                                    : ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                function, block->id, firstInstruction,
                                expected, incoming->value);
                        valid = ZR_FALSE;
                        break;
                    }
                    if (!backedge &&
                        ZR_EXEC_IR_MEMORY_TOKEN_VERSION(incoming->value) >
                            maximumVersion) {
                        maximumVersion = ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                                incoming->value);
                    }
                }
                if (!valid) break;
                if (ZR_EXEC_IR_MEMORY_TOKEN_VERSION(phiResult) <= maximumVersion ||
                    (hasMemoryTouch &&
                     ((hasMemoryInput && firstInput != phiResult) ||
                      (!hasMemoryInput &&
                       ZR_EXEC_IR_MEMORY_TOKEN_VERSION(firstOutput) <=
                           ZR_EXEC_IR_MEMORY_TOKEN_VERSION(phiResult))))) {
                    zr_exec_ir_effect_diag(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                            function, block->id, firstInstruction,
                            phiResult,
                            hasMemoryInput ? firstInput : firstOutput);
                    valid = ZR_FALSE;
                    break;
                }
            } else if (block->predecessors.count != 0u && hasMemoryTouch) {
                TZrExecIrMemoryTokenId expected =
                        ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
                TZrBool haveExpected = ZR_FALSE;
                for (predecessorIndex = 0u;
                     predecessorIndex < block->predecessors.count;
                     ++predecessorIndex) {
                    TZrExecIrBlockId predecessor = function->predecessors[
                            block->predecessors.start + predecessorIndex];
                    TZrExecIrMemoryTokenId incoming = *zr_exec_ir_memory_cfg_slot(
                            terminal, predecessor - 1u, region);
                    if (!*zr_exec_ir_memory_cfg_known_slot(
                                known, predecessor - 1u, region)) {
                        haveExpected = ZR_FALSE;
                        break;
                    }
                    if (!haveExpected) {
                        expected = incoming;
                        haveExpected = ZR_TRUE;
                    } else if (expected != incoming) {
                        zr_exec_ir_effect_diag(
                                diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                function, block->id, firstInstruction,
                                expected, incoming);
                        valid = ZR_FALSE;
                        break;
                    }
                }
                if (!valid) break;
                if (haveExpected && expected != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    ((hasMemoryInput && firstInput != expected) ||
                     (!hasMemoryInput &&
                      ZR_EXEC_IR_MEMORY_TOKEN_VERSION(firstOutput) <=
                          ZR_EXEC_IR_MEMORY_TOKEN_VERSION(expected)))) {
                    zr_exec_ir_effect_diag(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                            function, block->id, firstInstruction,
                            expected,
                            hasMemoryInput ? firstInput : firstOutput);
                    valid = ZR_FALSE;
                    break;
                }
            }
        }
        if (!valid) break;
    }

    free(terminal);
    free(known);
    return valid;
}

static TZrBool zr_exec_ir_verify_effect_cfg(
        const SZrExecIrFunction *function,
        const TZrBool *backedges,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrEffectTokenId *terminal;
    TZrBool *known;
    TZrUInt32 blockIndex;
    TZrUInt32 iteration;
    TZrBool valid = ZR_TRUE;

    if (function->blockCount == 0u) return ZR_TRUE;
    terminal = (TZrExecIrEffectTokenId *)calloc(function->blockCount,
                                                  sizeof(*terminal));
    known = (TZrBool *)calloc(function->blockCount, sizeof(*known));
    if (terminal == ZR_NULL || known == ZR_NULL) {
        free(terminal);
        free(known);
        zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, ZR_EXEC_IR_BLOCK_ID_INVALID, 0u, 0u, 0u);
        return ZR_FALSE;
    }

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = block->predecessors.start;
             edgeIndex < block->predecessors.start + block->predecessors.count;
             ++edgeIndex) {
            if (function->predecessors[edgeIndex] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                function->predecessors[edgeIndex] > function->blockCount) {
                zr_exec_ir_effect_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                       function, block->id, 0u,
                                       function->blockCount,
                                       function->predecessors[edgeIndex]);
                free(terminal);
                free(known);
                return ZR_FALSE;
            }
        }
    }

    /* A small fixed-point pass propagates an effect chain through pure blocks
     * before checking joins.  This also handles cleanup/exception blocks that
     * only forward the incoming token. */
    for (iteration = 0u; iteration <= function->blockCount; ++iteration) {
        TZrBool changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrExecIrEffectTokenId current = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
            TZrBool currentKnown = ZR_FALSE;
            TZrUInt32 instructionIndex;

            if (block->effectPhiResult != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
                current = block->effectPhiResult;
                currentKnown = ZR_TRUE;
            } else if (block->predecessors.count == 0u) {
                currentKnown = ZR_TRUE;
            } else {
                TZrUInt32 predecessorIndex;
                TZrExecIrEffectTokenId incoming = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
                currentKnown = ZR_TRUE;
                for (predecessorIndex = block->predecessors.start;
                     predecessorIndex < block->predecessors.start + block->predecessors.count;
                     ++predecessorIndex) {
                    TZrExecIrBlockId predecessor = function->predecessors[predecessorIndex];
                    if (!known[predecessor - 1u]) {
                        currentKnown = ZR_FALSE;
                        break;
                    }
                    if (predecessorIndex == block->predecessors.start) {
                        incoming = terminal[predecessor - 1u];
                    } else if (incoming != terminal[predecessor - 1u]) {
                        currentKnown = ZR_FALSE;
                        break;
                    }
                }
                if (currentKnown) current = incoming;
            }
            for (instructionIndex = block->instructions.start;
                 instructionIndex < block->instructions.start + block->instructions.count;
                 ++instructionIndex) {
                const SZrExecIrInstruction *instruction =
                        &function->instructions[instructionIndex];
                const SZrExecIrOpcodeInfo *info =
                        ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
                if (zr_exec_ir_instruction_observable(instruction, info)) {
                    current = instruction->effectOut;
                    currentKnown = ZR_TRUE;
                }
            }
            if (terminal[blockIndex] != current || known[blockIndex] != currentKnown) {
                terminal[blockIndex] = current;
                known[blockIndex] = currentKnown;
                changed = ZR_TRUE;
            }
        }
        if (!changed) break;
    }

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrExecIrEffectTokenId firstEffectIn = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        TZrExecIrInstructionId firstInstruction = ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
        TZrUInt32 instructionIndex;
        TZrUInt32 predecessorIndex;

        for (instructionIndex = block->instructions.start;
             instructionIndex < block->instructions.start + block->instructions.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
            const SZrExecIrOpcodeInfo *info =
                    ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
            if (zr_exec_ir_instruction_observable(instruction, info)) {
                firstEffectIn = instruction->effectIn;
                firstInstruction = instructionIndex + 1u;
                break;
            }
        }

        if (block->effectPhiResult != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
            TZrExecIrEffectTokenId maximumIncoming = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
            if (!zr_exec_ir_range_valid(block->effectPhiIncomings,
                                        function->phiIncomingCount,
                                        function->phiIncoming) ||
                block->effectPhiIncomings.count != block->predecessors.count) {
                zr_exec_ir_effect_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                       function, block->id, firstInstruction,
                                       block->predecessors.count,
                                       block->effectPhiIncomings.count);
                valid = ZR_FALSE;
                break;
            }
            for (predecessorIndex = 0u;
                 predecessorIndex < block->predecessors.count;
                 ++predecessorIndex) {
                const SZrExecIrPhiIncoming *incoming =
                        &function->phiIncoming[block->effectPhiIncomings.start + predecessorIndex];
                TZrExecIrBlockId predecessor =
                        function->predecessors[block->predecessors.start + predecessorIndex];
                TZrExecIrEffectTokenId expected = terminal[predecessor - 1u];
                if (!known[predecessor - 1u] || incoming->predecessor != predecessor ||
                    incoming->value != expected) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           incoming->predecessor != predecessor
                                               ? ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH
                                               : ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                           function, block->id, firstInstruction,
                                           expected, incoming->value);
                    valid = ZR_FALSE;
                    break;
                }
                if (!backedges[block->predecessors.start + predecessorIndex] &&
                    incoming->value > maximumIncoming) {
                    maximumIncoming = incoming->value;
                }
            }
            if (!valid) break;
            if (block->effectPhiResult <= maximumIncoming ||
                (firstInstruction != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                 firstEffectIn != block->effectPhiResult)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                       function, block->id, firstInstruction,
                                       block->effectPhiResult,
                                       firstInstruction != ZR_EXEC_IR_INSTRUCTION_ID_INVALID
                                           ? firstEffectIn
                                           : maximumIncoming);
                valid = ZR_FALSE;
                break;
            }
        } else if (block->predecessors.count != 0u &&
                   firstInstruction != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrExecIrEffectTokenId expected = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
            TZrBool haveExpected = ZR_FALSE;
            for (predecessorIndex = 0u;
                 predecessorIndex < block->predecessors.count;
                 ++predecessorIndex) {
                TZrExecIrBlockId predecessor =
                        function->predecessors[block->predecessors.start + predecessorIndex];
                if (!known[predecessor - 1u]) {
                    haveExpected = ZR_FALSE;
                    break;
                }
                if (!haveExpected) {
                    expected = terminal[predecessor - 1u];
                    haveExpected = ZR_TRUE;
                } else if (expected != terminal[predecessor - 1u]) {
                    zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                           function, block->id, firstInstruction,
                                           expected, terminal[predecessor - 1u]);
                    valid = ZR_FALSE;
                    break;
                }
            }
            if (!valid) break;
            if (haveExpected && expected != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                firstEffectIn != expected) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                       function, block->id, firstInstruction,
                                       expected, firstEffectIn);
                valid = ZR_FALSE;
                break;
            }
        }
    }

    free(terminal);
    free(known);
    return valid;
}

TZrBool ZrCore_ExecIr_VerifyEffects(const SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrBool *backedges = ZR_NULL;
    TZrExecIrMemoryTokenId latestMemory = ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
    TZrExecIrMemoryTokenId latestMemoryByRegion[ZR_EXEC_IR_MEMORY_CLASS_COUNT] = {0};
    TZrExecIrBlockId latestMemoryBlock = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrExecIrEffectTokenId latestEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
    TZrExecIrBlockId latestEffectBlock = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (!zr_exec_ir_effect_storage_valid(function, diagnostic)) {
        return ZR_FALSE;
    }
    if (!zr_exec_ir_verify_phi_predecessors(function, diagnostic)) {
        return ZR_FALSE;
    }
    if (!zr_exec_ir_classify_backedges(function, &backedges, diagnostic)) {
        return ZR_FALSE;
    }
    if (!zr_exec_ir_verify_memory_cfg(function, backedges, diagnostic)) {
        free(backedges);
        return ZR_FALSE;
    }
    if (!zr_exec_ir_verify_effect_cfg(function, backedges, diagnostic)) {
        free(backedges);
        return ZR_FALSE;
    }
    free(backedges);
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
        TZrExecIrBlockId blockId = zr_exec_ir_containing_block(function, index);
        TZrUInt16 required;
        TZrUInt32 tokenIndex;
        TZrUInt32 taggedMemoryInRegions = 0u;
        TZrUInt32 taggedMemoryOutRegions = 0u;
        TZrBool allMemoryInTagged = ZR_TRUE;
        TZrBool allMemoryOutTagged = ZR_TRUE;
        TZrBool observable;
        if (info == ZR_NULL) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                   function, blockId, index + 1u, 0u, instruction->opcode);
            return ZR_FALSE;
        }
        required = zr_exec_ir_required_flags(info, instruction);
        if (blockId != latestMemoryBlock) {
            memset(latestMemoryByRegion, 0, sizeof(latestMemoryByRegion));
            latestMemoryBlock = blockId;
        }
        if ((instruction->flags & required) != required) {
            zr_exec_ir_effect_diag(diagnostic,
                                   (required & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u
                                       ? ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE
                                       : ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                   function, blockId, index + 1u, required, instruction->flags);
            return ZR_FALSE;
        }
        if (!zr_exec_ir_range_valid(instruction->memoryIn, function->memoryTokenCount,
                                    function->memoryTokenPool) ||
            !zr_exec_ir_range_valid(instruction->memoryOut, function->memoryTokenCount,
                                    function->memoryTokenPool) ||
            !zr_exec_ir_range_valid(instruction->operands,
                                    function->operandCount,
                                    function->operandPool) ||
            !zr_exec_ir_range_valid(instruction->results,
                                    function->resultCount,
                                    function->resultPool) ||
            !zr_exec_ir_range_valid(instruction->phiRange,
                                    function->phiIncomingCount,
                                    function->phiIncoming) ||
            !zr_exec_ir_range_valid(instruction->successorRange,
                                    function->successorCount,
                                    function->successors)) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                   function, blockId, index + 1u, 0u, 0u);
            return ZR_FALSE;
        }
        if (info->memoryReads != 0u && instruction->memoryIn.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        if (info->memoryWrites != 0u && instruction->memoryOut.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        for (tokenIndex = instruction->memoryIn.start;
             tokenIndex < instruction->memoryIn.start + instruction->memoryIn.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (!zr_exec_ir_memory_token_valid(token) ||
                !zr_exec_ir_memory_token_matches(token, info->memoryReads)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, blockId, index + 1u, latestMemory, token);
                return ZR_FALSE;
            }
            if (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) {
                EZrExecIrMemoryClass region = ZR_EXEC_IR_MEMORY_TOKEN_REGION(token);
                TZrExecIrMemoryTokenId version = ZR_EXEC_IR_MEMORY_TOKEN_VERSION(token);
                taggedMemoryInRegions |= (TZrUInt32)1u << region;
                if (latestMemoryByRegion[region] != 0u &&
                    version != latestMemoryByRegion[region]) {
                    zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, blockId, index + 1u,
                                           latestMemoryByRegion[region], version);
                    return ZR_FALSE;
                }
                if (version > latestMemoryByRegion[region])
                    latestMemoryByRegion[region] = version;
            } else {
                allMemoryInTagged = ZR_FALSE;
                if (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    token < latestMemory) {
                    zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, blockId, index + 1u, latestMemory, token);
                    return ZR_FALSE;
                }
                latestMemory = token;
            }
        }
        if (allMemoryInTagged &&
            (taggedMemoryInRegions & info->memoryReads) != info->memoryReads) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u,
                                   info->memoryReads, taggedMemoryInRegions);
            return ZR_FALSE;
        }
        for (tokenIndex = instruction->memoryOut.start;
             tokenIndex < instruction->memoryOut.start + instruction->memoryOut.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (!zr_exec_ir_memory_token_valid(token) ||
                !zr_exec_ir_memory_token_matches(token, info->memoryWrites)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, blockId, index + 1u,
                                       latestMemory == UINT32_MAX ? latestMemory : latestMemory + 1u,
                                       token);
                return ZR_FALSE;
            }
            if (ZR_EXEC_IR_MEMORY_TOKEN_IS_TAGGED(token)) {
                EZrExecIrMemoryClass region = ZR_EXEC_IR_MEMORY_TOKEN_REGION(token);
                TZrExecIrMemoryTokenId version = ZR_EXEC_IR_MEMORY_TOKEN_VERSION(token);
                taggedMemoryOutRegions |= (TZrUInt32)1u << region;
                if (version <= latestMemoryByRegion[region]) {
                    zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, blockId, index + 1u,
                                           latestMemoryByRegion[region] + 1u, version);
                    return ZR_FALSE;
                }
                latestMemoryByRegion[region] = version;
            } else {
                allMemoryOutTagged = ZR_FALSE;
                if (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                    token <= latestMemory) {
                    zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                           function, blockId, index + 1u,
                                           latestMemory == UINT32_MAX ? latestMemory : latestMemory + 1u,
                                           token);
                    return ZR_FALSE;
                }
                latestMemory = token;
            }
        }
        if (allMemoryOutTagged &&
            (taggedMemoryOutRegions & info->memoryWrites) != info->memoryWrites) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, blockId, index + 1u,
                                   info->memoryWrites, taggedMemoryOutRegions);
            return ZR_FALSE;
        }
        observable = (TZrBool)(info->memoryWrites != 0u || required != 0u ||
                               (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u);
        if (observable) {
            TZrBool effectOrderInvalid = ZR_FALSE;
            if (latestEffect != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) {
                if (blockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
                    latestEffectBlock != ZR_EXEC_IR_BLOCK_ID_INVALID &&
                    blockId == latestEffectBlock) {
                    effectOrderInvalid = (TZrBool)(instruction->effectIn != latestEffect);
                } else if (blockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                           latestEffectBlock == ZR_EXEC_IR_BLOCK_ID_INVALID) {
                    effectOrderInvalid = (TZrBool)(instruction->effectIn < latestEffect);
                }
            }
            if (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut <= instruction->effectIn ||
                effectOrderInvalid) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                       function, blockId, index + 1u, latestEffect,
                                       instruction->effectIn);
                return ZR_FALSE;
            }
            latestEffect = instruction->effectOut;
            latestEffectBlock = blockId;
        }
    }
    return ZR_TRUE;
}

/**
 * @fn static void zr_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 blockId, TZrUInt32 instructionId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 在effects拒绝分支重新构造可选诊断，避免前一次错误的source/位置字段混入当前错误；sourceId仅从当前有效指令位置解析。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空须是与function及所有输入池独立的可写完整诊断记录，每次调用先整体清零。
 * @param[in] code 写入本次诊断的错误分类。
 * @param[in] function diagnostic非空时须非空且可读；读取token，instructionId有效时还读取对应instructions元素。
 * @param[in] blockId 直接记录的块上下文，不在此检查有效性。
 * @param[in] instructionId 直接记录的1基指令位置；0或超出instructionCount时sourceId写0。
 * @param[in] expected 写入expectedVersion的预期上下文值。
 * @param[in] actual 写入actualVersion的实际上下文值。
 *
 * ## Returns
 * 无返回值；diagnostic为空直接返回，否则先清零再完成本次诊断，不分配或转移任何资源。
 *
 * ## Constraints and lifecycle
 * 本助手自身每次非空调用都memset，不能写成只有公共入口才清零；反之，公共成功路径不调用它，所以成功不保证旧诊断清空。有效指令ID的source读取仍依赖instructions真实可读，未认证任意指针；诊断不得别名任何仍活输入。所有借用只限本次调用，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:7：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:14：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:17：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:22：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:871：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:904：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:522：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:609：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrExecIrBlockId zr_exec_ir_containing_block(const SZrExecIrFunction *function, TZrUInt32 instructionIndex)
 *
 * ## Summary
 * 为最终逐指令effects检查寻找第一条包含该零基指令索引的块范围，以提供块内链重置和诊断上下文；不构建或修改CFG索引。
 *
 * ## Parameters
 * @param[in] function 非空函数借用；blocks及其blockCount项须稳定可读。
 * @param[in] instructionIndex 零基指令索引；查询本身不验证它小于instructionCount。
 *
 * ## Returns
 * @return 返回按块数组顺序第一个包含索引的block.id；找不到时返回INVALID，供无块/未归属指令的线性规则使用。
 *
 * ## Constraints and lifecycle
 * 通过先比较start再做减法判定范围，避免start+count相加；不判断块ID规范性、重复/重叠归属或指令覆盖完整性。此类结构责任属于上层通用/STRUCTURE验证，本查询不能替代。无分配或借用输出，无同步；读取期间不得修改或释放blocks。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:29：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:34：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:862：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:876：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:1007：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_range_valid(SZrExecIrRange range, TZrUInt32 count, const void *pool)
 *
 * ## Summary
 * 在访问侧池前验证逻辑子范围，并要求非空范围具有非空池地址；减法式比较避免为检查而计算可能溢出的start+count。
 *
 * ## Parameters
 * @param[in] range 零基起点和元素数；即使count0，起点也必须不超过总count。
 * @param[in] count 池的逻辑元素数，不能以多余物理capacity代替。
 * @param[in] pool 只作NULL测试的地址；range.count0时可为空，本函数不读取池元素。
 *
 * ## Returns
 * @return TRUE 表示范围在逻辑计数内且非空范围有地址；FALSE 表示起点/计数越界或非空范围缺池。没有诊断或资源输出。
 *
 * ## Constraints and lifecycle
 * 只检查整数关系与NULL，不证明真实分配大小、来源、对齐、可读性或池间不重叠。当前调用点负责按具体结构解释单位并在失败时报告。无分配、无参数保留、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:42：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:45：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:48：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:105：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:207：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:888：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:419：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:768：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_memory_token_valid( TZrExecIrMemoryTokenId token)
 *
 * ## Summary
 * 识别effects校验所接受的内存token编码：拒绝INVALID0，保留非零旧式无tag编号，并对tagged编码校验region界限与非零version。
 *
 * ## Parameters
 * @param[in] token 待检查的按值token，不读取其代表的内存对象。
 *
 * ## Returns
 * @return TRUE 表示非零legacy token，或region小于MEMORY_CLASS_COUNT且version非0的tagged token；FALSE不表示运行时对象不存在，只是本编码不接受。
 *
 * ## Constraints and lifecycle
 * 本函数只检查编码，不检查opcode的允许region、版本顺序、池归属或CFG来源；匹配和链关系由后续助手完成。不分配、无指针借用或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:54：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:56：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:58：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:386：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:407：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:922：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:963：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_memory_token_matches( TZrExecIrMemoryTokenId token, TZrUInt32 regionMask)
 *
 * ## Summary
 * 在token编码已验证后检查tagged token的region是否属于opcode声明的读/写集合；legacy token保留兼容路径，不施加tagged region匹配。
 *
 * ## Parameters
 * @param[in] token 已由memory_token_valid检查的编码；tagged region须在可安全移位的合法范围内。
 * @param[in] regionMask 当前opcode的memoryReads或memoryWrites位掩码。
 *
 * ## Returns
 * @return legacy token直接返回TRUE；tagged token仅在region对应位存在时返回TRUE。此返回值不重新验证INVALID、version或版本顺序。
 *
 * ## Constraints and lifecycle
 * 不得跳过前置编码检查：本函数会直接按region移位，不为任意畸形编码提供安全兜底。两处实际调用均在valid失败时以||短路，只有有效token才进入本函数。纯值查询，不分配或保留资源。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:63：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:65：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:922：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:923：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:963：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:964：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:556：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_effect_storage_valid(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为可直接调用的effects验证建立基础存储形状：核对十组池的count/capacity与非空逻辑池地址，再检查每块的指令/边/PHI范围及effect/memory PHI的存在配对。
 *
 * ## Parameters
 * @param[in] function 已排除NULL的函数记录；非空指针对应的声明范围须真实可读。
 * @param[out] diagnostic 可空且独立可写；拒绝时由effect_diag先清零再写，成功不写。
 *
 * ## Returns
 * @return TRUE 表示本助手负责的存储形状与块PHI配对通过；FALSE表示计数/地址、范围或PHI token编码失败，输入不变，无本地资源待释放。
 *
 * ## Constraints and lifecycle
 * 允许count0时池为NULL，也不强制capacity0配对；不认证任意地址或实际分配长度。不检查function.id、token非零、规范block.id、CFG互反/终结器、SSA值身份或所有指令子范围。memoryPhiResult非零时须tagged、region等于对应槽且version非0；effect/memory result是否INVALID与incoming count是否0必须一致。仅作为下一PHI/CFG/逐指令阶段的前置条件，不是完整函数合法性证明。diagnostic不别名输入；无分配和内部同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:70：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:74：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:93：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:105：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:129：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:141：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:154：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:841：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:752：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:768：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_block_has_predecessor(const SZrExecIrFunction *function, const SZrExecIrBlock *block, TZrExecIrBlockId predecessor)
 *
 * ## Summary
 * 查询某块的前驱列表是否含指定源块，供标量PHI先排除外来前驱；它只判断成员关系，精确边出现顺序由调用者继续核对。
 *
 * ## Parameters
 * @param[in] function 拥有predecessors池的非空稳定借用。
 * @param[in] block 前驱逻辑范围已经验证、与function同一上下文的块借用。
 * @param[in] predecessor 待比较的块ID；按值相等，不在此认证ID范围。
 *
 * ## Returns
 * @return 找到任一相等项返回TRUE，否则FALSE；没有输出池或诊断。
 *
 * ## Constraints and lifecycle
 * 不检查range、池NULL、重复前驱或CFG互反关系。并行边的重复源块是允许的，不能用这个成员查询替代按位置匹配incoming。无分配、不保留借用、无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:175：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:179：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:228：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:238：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:401：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:670：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_phi_predecessors(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在effects和memory CFG传播前核对标量PHI incoming与块前驱的出现次数、成员及顺序；同源并行边各保留独立位置，而非按源块去重。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，基础池形状由effect_storage_valid先验证；本函数再校验所读范围。
 * @param[out] diagnostic 可空独立诊断，拒绝时清零后填写INVALID_RANGE或PHI_PREDECESSOR_MISMATCH，成功不写。
 *
 * ## Returns
 * @return TRUE 表示所有被块引用的标量PHI的incoming范围、数量及逐位置前驱均匹配；FALSE即停止于首个发现的不匹配，输入不变。
 *
 * ## Constraints and lifecycle
 * 本函数校验每块predecessors/phis范围，以及各phi.incomings范围和count；不验证incoming.value或phi.result的SSA身份，也不把前驱成员本身认证为合法BlockId，后续回边分类还会查ID。先成员、后顺序的检查解释了外来前驱和倒序前驱诊断；两个完全相同源的并行边仍可通过。无临时分配、无修复或排序；diagnostic不能覆盖输入。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:189：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:195：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:207：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:213：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:228：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:238：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:249：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:844：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:401：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:641：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:670：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrUInt16 zr_exec_ir_required_flags(const SZrExecIrOpcodeInfo *info, const SZrExecIrInstruction *instruction)
 *
 * ## Summary
 * 把opcode schema中四种可能发生的运行时能力映射为指令必须显式携带的MAY_*标志，供可观察性分类和最终缺flag检查复用。
 *
 * ## Parameters
 * @param[in] info 非空opcode schema借用；读取MAY_ALLOCATE/THROW/GC/SUSPEND四个位。
 * @param[in] instruction 保留的上下文参数；当前实现只(void)忽略，不读取内容，允许NULL。
 *
 * ## Returns
 * @return 返回要求的四位指令flag子集；无对应schema位时为0，不产生诊断。
 *
 * ## Constraints and lifecycle
 * 不从bindingRow、实际指令额外flags或能力推断更多必需位；instruction内容当前不参与结果。schema记录须在读取期间存活稳定，不接管、不分配，无同步。调用者对info NULL负责，observable助手先排除NULL，公开最终阶段在OpcodeInfo失败后早退。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:258：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:261：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:262：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:274：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:870：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:875：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:364：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:696：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_instruction_observable( const SZrExecIrInstruction *instruction, const SZrExecIrOpcodeInfo *info)
 *
 * ## Summary
 * 按schema判断一条指令是否推进可观察effect链，用于跨块终端token传播和寻找块内第一个effect使用点。
 *
 * ## Parameters
 * @param[in] instruction 传给required_flags的上下文；该下层当前忽略此参数，本函数不直接解引用。
 * @param[in] info 可空opcode schema借用；NULL直接返回FALSE，未知opcode由公开入口最终阶段另报错。
 *
 * ## Returns
 * @return 有memoryWrites、至少一个必需MAY_* flag或DROP effect时为TRUE；纯memoryReads本身不会使其为TRUE。info为空或三类均无时为FALSE。
 *
 * ## Constraints and lifecycle
 * 按schema而非instruction实际flags决定可观察性；FALSE不是该指令合法的证明。调用者最后仍须查unknown opcode、必需flag和token关系。无分配、无输出借用，不执行真实内存写入、GC或调度。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:269：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:273：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:275：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:698：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:725：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:1002：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrExecIrMemoryTokenId *zr_exec_ir_memory_cfg_slot( TZrExecIrMemoryTokenId *storage, TZrUInt32 blockIndex, TZrUInt32 region)
 *
 * ## Summary
 * 将零基块序号和memory region转换为临时CFG表的一维槽地址，统一传播阶段和PHI检查的行布局。
 *
 * ## Parameters
 * @param[in] storage 活的可写memory token临时数组地址；本助手只计算地址，不读取或修改元素。
 * @param[in] blockIndex 小于分配行数的零基块序号。
 * @param[in] region 小于ZR_EXEC_IR_MEMORY_CLASS_COUNT的region槽序号。
 *
 * ## Returns
 * @return 返回storage内部对应元素的可写借用指针；不分配、不转移所有权，caller不得单独free。该借用在上层释放terminal/known时失效。
 *
 * ## Constraints and lifecycle
 * 不检查NULL、索引、乘加或实际长度；调用方先按blockCount×MEMORY_CLASS_COUNT分配并保证索引范围和可表示性。返回地址有意别名输入数组；这不代表允许覆盖其它input/diagnostic。当前只在verify_memory_cfg内消费，未被保存到function或返回给公开caller；无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:279：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:313：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:314：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:418：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:607：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool *zr_exec_ir_memory_cfg_known_slot( TZrBool *storage, TZrUInt32 blockIndex, TZrUInt32 region)
 *
 * ## Summary
 * 将零基块序号和memory region转换为临时CFG表的一维槽地址，统一传播阶段和PHI检查的行布局。
 *
 * ## Parameters
 * @param[in] storage 活的可写known布尔临时数组地址；本助手只计算地址，不读取或修改元素。
 * @param[in] blockIndex 小于分配行数的零基块序号。
 * @param[in] region 小于ZR_EXEC_IR_MEMORY_CLASS_COUNT的region槽序号。
 *
 * ## Returns
 * @return 返回storage内部对应元素的可写借用指针；不分配、不转移所有权，caller不得单独free。该借用在上层释放terminal/known时失效。
 *
 * ## Constraints and lifecycle
 * 不检查NULL、索引、乘加或实际长度；调用方先按blockCount×MEMORY_CLASS_COUNT分配并保证索引范围和可表示性。返回地址有意别名输入数组；这不代表允许覆盖其它input/diagnostic。当前只在verify_memory_cfg内消费，未被保存到function或返回给公开caller；无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:286：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:313：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:315：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:420：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:608：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_memory_cfg( const SZrExecIrFunction *function, const TZrBool *backedges, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为tagged region token建立每块终端值及known事实，经过有限轮传播后校验memory PHI与块首个memory touch的衔接；使用已分类的真实回边跳过仅针对前向入口的版本上界比较。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数；基础池/块范围、标量PHI形状和前驱ID须由此前storage/phi/backedge阶段建立安全条件。
 * @param[in] backedges 以predecessors池绝对索引寻址的只读分类数组，长度覆盖predecessorCount；没有前驱位置会被读取时才可NULL。
 * @param[out] diagnostic 可空独立诊断；失败通过effect_diag重建，成功不清旧内容。
 *
 * ## Returns
 * @return TRUE表示本助手按已知传播事实和代码中的PHI/首touch规则通过；零块直接TRUE。FALSE为容量、OOM、PHI数量/顺序、终端匹配或版本衔接拒绝；本地terminal/known均已释放。
 *
 * ## Constraints and lifecycle
 * terminal/known各按块×region分配，SIZE_MAX<=UINT32_MAX配置先做乘法容量检查；分配任一个失败都释放两者。传播只发布有效tagged token，legacy序号的单调规则留给公开最终扫描；最多blockCount+1轮并可提前稳定退出。存在PHI时incoming逐位置匹配known前驱终端及region；非回边的INVALID输入允许作为初始状态，回边不适用这个例外，PHI版本须大于非回边最大输入版本。首touch若是读必须消费PHI/统一前驱token，若先写则version须推进；无PHI分支仅在haveExpected成立时施加对应已知输入约束，不能夸大成证明所有CFG事实已知。所有分析为临时值，不改输入或发布borrow；diagnostic不得与输入/分类池重叠，无同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:293：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:304：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:305：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:313：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:316：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:324：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:431：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:494：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:518：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:538：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:546：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:560：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:589：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:607：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:850：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:213：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:242：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:316：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:133：本单元职责、调用或相关断言证据。
 */

/**
 * @fn static TZrBool zr_exec_ir_verify_effect_cfg( const SZrExecIrFunction *function, const TZrBool *backedges, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为可观察effect链计算每块终端token并穿过纯块传播，之后检查显式effect PHI的有序incoming、前向版本约束和第一个可观察指令的输入；没有PHI时核对已知统一前驱输入。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，基础块范围和前驱池已通过前置阶段；读取opcode schema判断可观察指令。
 * @param[in] backedges 与predecessors池绝对索引对应的分类借用；被读取位置必须存在。
 * @param[out] diagnostic 可空独立诊断；拒绝时清零后写入，成功不写。
 *
 * ## Returns
 * @return 零块或本阶段检查通过返回TRUE；OOM、前驱ID、PHI范围/数量/次序/终端值或effect输入关系失败返回FALSE。两个本地数组在所有取得后返回路径释放。
 *
 * ## Constraints and lifecycle
 * 先分配terminal/known，再检查前驱ID，然后最多blockCount+1轮传播；纯块可转发统一已知前驱，带PHI先以其result建立入口。显式PHI要求每个incoming前驱位置和值匹配且known；结果大于所有非回边incoming，并由第一个可观察指令消费。无PHI且有首个可观察指令时，分歧的已知前驱token拒绝；未形成haveExpected时不在该分支强加相等约束，纯块的未知事实也不能被文档描述成全图完整证明。unknown opcode在传播中视不可观察，稍后公开入口最终扫描仍拒绝。只分析数字token，不执行effect、不改CFG；无借用输出或锁。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:612：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:622：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:623：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:634：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:654：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:657：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:732：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:754：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:765：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:771：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:783：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:808：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:819：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:854：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:87：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:116：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:330：本单元职责、调用或相关断言证据。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_VerifyEffects(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 独立检查ExecIR描述符中的schema必需flags、memory token与可观察effect链；依次完成基础存储、标量PHI前驱、回边分类、memory/effect CFG关系和最终逐指令检查，不执行IR指令。
 *
 * ## Parameters
 * @param[in] function 可空的只读函数描述符；NULL被拒绝。非空时各声明池须真实可读，调用期间不得并发修改/释放；支持独立effects夹具和无块线性形式，不要求本接口先通过完整SSA。
 * @param[in,out] diagnostic 可空且与function、所有输入池独立的完整可写诊断；失败时重建相应错误，非空function的成功调用保留原内容。
 *
 * ## Returns
 * @return TRUE表示本接口各effects检查通过；FALSE为NULL、存储/范围/PHI/CFG/token/flag/未知opcode或分配拒绝。所有回边及本地分析分配在返回前释放，不交付池所有权；输入池、计数、generation和缓存不变。
 *
 * ## Constraints and lifecycle
 * NULL function分支单独清零非空diagnostic并写INVALID_ARGUMENT；非NULL时只有错误分支的助手清诊断，成功不能用旧diagnostic.code判断此次结果。tagged输入须合法并匹配schema region，读消费该region当前版本、写严格推进；每组全部tagged时还要求覆盖schema读/写掩码，混有legacy token时采用兼容规则，legacy编号另按最终指令/池次序保持单调。tagged region局部状态随查询到的块变化重置；可观察effect须非零且out>in，同块紧接前一effect，跨有效块关系由CFG阶段处理，无块上下文采用线性回退规则。独立调用不验证完整SSA、所有值身份、规范块编号或CFG终结器/互反全部条件；不能用TRUE替代VerifyFunction ALL，后者还可能在stateMap验证失败。计数/NULL检查不认证任意地址或别名；输入可共享只读，diagnostic由caller独占，无内部同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:824：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:834：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:841：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:844：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:847：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:850：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:854：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:858：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:870：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:880：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:888：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:922：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:952：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:963：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:995：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:1002：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:1007：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:1016：本单元职责、调用或相关断言证据。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:697：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:87：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier.c:739：本单元职责、调用或相关断言证据。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:151：本单元职责、调用或相关断言证据。
 */
