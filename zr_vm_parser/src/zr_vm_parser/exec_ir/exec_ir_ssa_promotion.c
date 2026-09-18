#include "exec_ir_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct SZrPlacePromotion {
    TZrUInt32 placeCount;
    TZrUInt32 blockCount;
    TZrUInt32 originalValueCount;
    TZrExecIrValueId *places;
    TZrUInt32 *placeByValue;
    TZrUInt8 *active;
    TZrUInt8 *uses;
    TZrUInt8 *definitions;
    TZrUInt8 *liveIn;
    TZrUInt8 *frontiers;
    TZrUInt8 *hasPhi;
    TZrUInt32 *phiSlots;
    TZrExecIrBlockId *instructionBlocks;
    TZrExecIrValueId *outDefinitions;
    TZrUInt32 frontierRowBytes;
} SZrPlacePromotion;

static TZrBool promotion_fail(const SZrExecIrFunction *function,
                              SZrExecIrDiagnostic *diagnostic,
                              EZrExecutionDiagnosticCode code,
                              TZrExecIrInstructionId instructionId,
                              TZrExecIrBlockId blockId) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = function->functionToken;
        diagnostic->instructionId = instructionId;
        diagnostic->blockId = blockId;
        if (instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
            instructionId <= function->instructionCount) {
            diagnostic->sourceId =
                    function->instructions[instructionId - 1u].sourceId;
        }
    }
    return ZR_FALSE;
}

static void promotion_free(SZrPlacePromotion *promotion) {
    free(promotion->places);
    free(promotion->placeByValue);
    free(promotion->active);
    free(promotion->uses);
    free(promotion->definitions);
    free(promotion->liveIn);
    free(promotion->frontiers);
    free(promotion->hasPhi);
    free(promotion->phiSlots);
    free(promotion->instructionBlocks);
    free(promotion->outDefinitions);
    memset(promotion, 0, sizeof(*promotion));
}

static void *promotion_calloc(const SZrExecIrFunction *function,
                              TZrSize count,
                              TZrSize elementSize,
                              SZrExecIrDiagnostic *diagnostic) {
    if (count == 0u) return ZR_NULL;
    if (elementSize != 0u && count > SIZE_MAX / elementSize) {
        promotion_fail(function, diagnostic,
                       ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u, 0u);
        return ZR_NULL;
    }
    {
        void *allocation = calloc(count, elementSize);
        if (allocation == ZR_NULL) {
            promotion_fail(function, diagnostic,
                           ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u, 0u);
        }
        return allocation;
    }
}

static TZrBool promotion_is_candidate(const SZrExecIrFunction *function,
                                      TZrExecIrValueId valueId) {
    return (TZrBool)(
            valueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
            valueId <= function->valueCount &&
            (function->values[valueId - 1u].flags &
             ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u);
}

static TZrBool promotion_has_memory_access(
        const SZrExecIrFunction *function) {
    TZrUInt32 instructionIndex;
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        if ((instruction->opcode == ZR_EXEC_IR_OPCODE_LOAD ||
             instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) &&
            instruction->operandRange.count != 0u &&
            promotion_is_candidate(
                    function,
                    function->operands[instruction->operandRange.start])) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool promotion_verify_candidate(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrFunctionId savedId = function->id;
    TZrMetadataToken savedToken = function->functionToken;
    TZrBool result;
    if (function->id == ZR_EXEC_IR_FUNCTION_ID_INVALID) function->id = 1u;
    if (function->functionToken == 0u) function->functionToken = 1u;
    result = ZrCore_ExecIr_VerifyFunction(
            function,
            (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                   ZR_EXEC_IR_VERIFY_SSA),
            diagnostic);
    function->id = savedId;
    function->functionToken = savedToken;
    return result;
}

static TZrBool promotion_allocate_analysis(
        const SZrExecIrFunction *function,
        SZrPlacePromotion *promotion,
        SZrExecIrDiagnostic *diagnostic) {
    TZrSize placeBlocks;
    TZrSize frontierBytes;
    TZrUInt32 valueIndex;
    TZrUInt32 placeIndex = 0u;

    memset(promotion, 0, sizeof(*promotion));
    promotion->blockCount = function->blockCount;
    promotion->originalValueCount = function->valueCount;
    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        if ((function->values[valueIndex].flags &
             ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u) {
            ++promotion->placeCount;
        }
    }
    if (promotion->placeCount == 0u) return ZR_TRUE;
    if (promotion->blockCount != 0u &&
        promotion->placeCount > SIZE_MAX / promotion->blockCount) {
        return promotion_fail(function, diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              0u, 0u);
    }
    placeBlocks = (TZrSize)promotion->placeCount * promotion->blockCount;
    promotion->frontierRowBytes =
            (TZrUInt32)(((TZrSize)promotion->blockCount + 7u) / 8u);
    if (promotion->blockCount != 0u &&
        promotion->frontierRowBytes > SIZE_MAX / promotion->blockCount) {
        return promotion_fail(function, diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              0u, 0u);
    }
    frontierBytes = (TZrSize)promotion->blockCount *
                    promotion->frontierRowBytes;

    promotion->places = (TZrExecIrValueId *)promotion_calloc(
            function, promotion->placeCount, sizeof(*promotion->places),
            diagnostic);
    promotion->placeByValue = (TZrUInt32 *)promotion_calloc(
            function, (TZrSize)function->valueCount + 1u,
            sizeof(*promotion->placeByValue), diagnostic);
    promotion->active = (TZrUInt8 *)promotion_calloc(
            function, promotion->placeCount, sizeof(*promotion->active),
            diagnostic);
    promotion->uses = (TZrUInt8 *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->uses), diagnostic);
    promotion->definitions = (TZrUInt8 *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->definitions),
            diagnostic);
    promotion->liveIn = (TZrUInt8 *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->liveIn), diagnostic);
    promotion->frontiers = (TZrUInt8 *)promotion_calloc(
            function, frontierBytes, sizeof(*promotion->frontiers),
            diagnostic);
    promotion->hasPhi = (TZrUInt8 *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->hasPhi), diagnostic);
    promotion->phiSlots = (TZrUInt32 *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->phiSlots), diagnostic);
    promotion->instructionBlocks = (TZrExecIrBlockId *)promotion_calloc(
            function, function->instructionCount,
            sizeof(*promotion->instructionBlocks), diagnostic);
    promotion->outDefinitions = (TZrExecIrValueId *)promotion_calloc(
            function, placeBlocks, sizeof(*promotion->outDefinitions),
            diagnostic);
    if (promotion->places == ZR_NULL ||
        promotion->placeByValue == ZR_NULL || promotion->active == ZR_NULL ||
        (placeBlocks != 0u &&
         (promotion->uses == ZR_NULL || promotion->definitions == ZR_NULL ||
          promotion->liveIn == ZR_NULL || promotion->hasPhi == ZR_NULL ||
          promotion->phiSlots == ZR_NULL ||
          promotion->outDefinitions == ZR_NULL)) ||
        (frontierBytes != 0u && promotion->frontiers == ZR_NULL) ||
        (function->instructionCount != 0u &&
         promotion->instructionBlocks == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (valueIndex = 0u; valueIndex <= function->valueCount; ++valueIndex) {
        promotion->placeByValue[valueIndex] = UINT32_MAX;
    }
    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        if ((function->values[valueIndex].flags &
             ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u) {
            promotion->places[placeIndex] = valueIndex + 1u;
            promotion->placeByValue[valueIndex + 1u] = placeIndex;
            promotion->active[placeIndex] = 1u;
            ++placeIndex;
        }
    }
    return ZR_TRUE;
}

static TZrBool promotion_build_instruction_blocks(
        const SZrExecIrFunction *function,
        SZrPlacePromotion *promotion,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 instructionIndex;
        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                        block->instructionRange.count;
             ++instructionIndex) {
            if (promotion->instructionBlocks[instructionIndex] !=
                ZR_EXEC_IR_BLOCK_ID_INVALID) {
                return promotion_fail(
                        function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                        instructionIndex + 1u, block->id);
            }
            promotion->instructionBlocks[instructionIndex] = block->id;
        }
    }
    for (blockIndex = 0u; blockIndex < function->instructionCount;
         ++blockIndex) {
        if (promotion->instructionBlocks[blockIndex] ==
            ZR_EXEC_IR_BLOCK_ID_INVALID) {
            return promotion_fail(function, diagnostic,
                                  ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                  blockIndex + 1u, 0u);
        }
    }
    return ZR_TRUE;
}

static void promotion_screen_uses(const SZrExecIrFunction *function,
                                  SZrPlacePromotion *promotion) {
    TZrUInt32 instructionIndex;
    TZrUInt32 incomingIndex;
    TZrUInt32 valueIndex;

    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrUInt32 operandOffset;
        for (operandOffset = 0u;
             operandOffset < instruction->operandRange.count;
             ++operandOffset) {
            TZrExecIrValueId operand = function->operands[
                    instruction->operandRange.start + operandOffset];
            TZrUInt32 placeIndex = operand <= promotion->originalValueCount
                                           ? promotion->placeByValue[operand]
                                           : UINT32_MAX;
            TZrBool supported = (TZrBool)(
                    operandOffset == 0u &&
                    (instruction->opcode == ZR_EXEC_IR_OPCODE_LOAD ||
                     instruction->opcode == ZR_EXEC_IR_OPCODE_STORE));
            if (placeIndex != UINT32_MAX && !supported) {
                promotion->active[placeIndex] = 0u;
            }
        }
    }
    for (incomingIndex = 0u; incomingIndex < function->phiIncomingCount;
         ++incomingIndex) {
        TZrExecIrValueId value = function->phiIncoming[incomingIndex].value;
        TZrUInt32 placeIndex = value <= promotion->originalValueCount
                                       ? promotion->placeByValue[value]
                                       : UINT32_MAX;
        if (placeIndex != UINT32_MAX) promotion->active[placeIndex] = 0u;
    }
    for (valueIndex = 0u; valueIndex < function->gcRootCount; ++valueIndex) {
        TZrExecIrValueId value = function->gcRoots[valueIndex];
        TZrUInt32 placeIndex = value <= promotion->originalValueCount
                                       ? promotion->placeByValue[value]
                                       : UINT32_MAX;
        if (placeIndex != UINT32_MAX) promotion->active[placeIndex] = 0u;
    }
    for (valueIndex = 0u; valueIndex < function->deoptValueCount; ++valueIndex) {
        TZrExecIrValueId value = function->deoptValues[valueIndex];
        TZrUInt32 placeIndex = value <= promotion->originalValueCount
                                       ? promotion->placeByValue[value]
                                       : UINT32_MAX;
        if (placeIndex != UINT32_MAX) promotion->active[placeIndex] = 0u;
    }
}

static TZrBool promotion_collect_local_facts(
        const SZrExecIrFunction *function,
        SZrPlacePromotion *promotion) {
    TZrUInt32 blockIndex;
    TZrBool anyActive = ZR_FALSE;

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt8 *defined = (TZrUInt8 *)calloc(
                promotion->placeCount, sizeof(*defined));
        TZrUInt32 instructionIndex;
        if (defined == ZR_NULL) return ZR_FALSE;
        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                        block->instructionRange.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            if ((instruction->opcode == ZR_EXEC_IR_OPCODE_LOAD ||
                 instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) &&
                instruction->operandRange.count != 0u) {
                TZrExecIrValueId address = function->operands[
                        instruction->operandRange.start];
                TZrUInt32 placeIndex =
                        address <= promotion->originalValueCount
                                ? promotion->placeByValue[address]
                                : UINT32_MAX;
                if (placeIndex != UINT32_MAX &&
                    promotion->active[placeIndex] != 0u) {
                    TZrSize slot = (TZrSize)placeIndex *
                                           promotion->blockCount + blockIndex;
                    anyActive = ZR_TRUE;
                    if (block->immediateDominator ==
                        ZR_EXEC_IR_BLOCK_ID_INVALID) {
                        promotion->active[placeIndex] = 0u;
                    } else if (instruction->opcode ==
                               ZR_EXEC_IR_OPCODE_LOAD) {
                        if (defined[placeIndex] == 0u) {
                            promotion->uses[slot] = 1u;
                        }
                    } else {
                        defined[placeIndex] = 1u;
                        promotion->definitions[slot] = 1u;
                    }
                }
            }
        }
        free(defined);
    }
    if (!anyActive) return ZR_TRUE;
    for (blockIndex = 0u; blockIndex < promotion->placeCount; ++blockIndex) {
        if (promotion->active[blockIndex] != 0u) return ZR_TRUE;
    }
    return ZR_TRUE;
}

static void promotion_compute_liveness(const SZrExecIrFunction *function,
                                       SZrPlacePromotion *promotion) {
    TZrBool changed;
    do {
        TZrUInt32 placeIndex;
        changed = ZR_FALSE;
        for (placeIndex = 0u; placeIndex < promotion->placeCount;
             ++placeIndex) {
            TZrUInt32 blockIndex;
            if (promotion->active[placeIndex] == 0u) continue;
            for (blockIndex = promotion->blockCount; blockIndex-- != 0u;) {
                const SZrExecIrBlock *block = &function->blocks[blockIndex];
                TZrSize slot = (TZrSize)placeIndex *
                                       promotion->blockCount + blockIndex;
                TZrBool liveOut = ZR_FALSE;
                TZrUInt32 successorOffset;
                TZrUInt8 newLiveIn;
                for (successorOffset = 0u;
                     successorOffset < block->successorRange.count;
                     ++successorOffset) {
                    TZrExecIrBlockId successor = function->successors[
                            block->successorRange.start + successorOffset];
                    TZrSize successorSlot = (TZrSize)placeIndex *
                                                   promotion->blockCount +
                                           successor - 1u;
                    if (promotion->liveIn[successorSlot] != 0u) {
                        liveOut = ZR_TRUE;
                        break;
                    }
                }
                newLiveIn = (TZrUInt8)(
                        promotion->uses[slot] != 0u ||
                        (liveOut && promotion->definitions[slot] == 0u));
                if (promotion->liveIn[slot] != newLiveIn) {
                    promotion->liveIn[slot] = newLiveIn;
                    changed = ZR_TRUE;
                }
            }
        }
    } while (changed);
}

static void promotion_frontier_set(SZrPlacePromotion *promotion,
                                   TZrUInt32 row,
                                   TZrUInt32 blockIndex) {
    TZrUInt8 *bytes = promotion->frontiers +
                      (TZrSize)row * promotion->frontierRowBytes;
    bytes[blockIndex / 8u] = (TZrUInt8)(
            bytes[blockIndex / 8u] |
            (TZrUInt8)(1u << (blockIndex % 8u)));
}

static TZrBool promotion_frontier_has(const SZrPlacePromotion *promotion,
                                      TZrUInt32 row,
                                      TZrUInt32 blockIndex) {
    const TZrUInt8 *bytes = promotion->frontiers +
                            (TZrSize)row * promotion->frontierRowBytes;
    return (TZrBool)((bytes[blockIndex / 8u] &
                      (TZrUInt8)(1u << (blockIndex % 8u))) != 0u);
}

static TZrBool promotion_compute_frontiers(
        const SZrExecIrFunction *function,
        SZrPlacePromotion *promotion,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrExecIrBlockId stop = block->immediateDominator;
        TZrUInt32 predecessorOffset;
        if (block->predecessorRange.count < 2u ||
            stop == ZR_EXEC_IR_BLOCK_ID_INVALID) {
            continue;
        }
        for (predecessorOffset = 0u;
             predecessorOffset < block->predecessorRange.count;
             ++predecessorOffset) {
            TZrExecIrBlockId runner = function->predecessors[
                    block->predecessorRange.start + predecessorOffset];
            TZrUInt32 steps = 0u;
            while (runner != stop) {
                if (runner == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                    runner > function->blockCount ||
                    ++steps > function->blockCount) {
                    return promotion_fail(
                            function, diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                            block->terminatorInstructionId, block->id);
                }
                promotion_frontier_set(promotion, runner - 1u, blockIndex);
                runner = function->blocks[runner - 1u].immediateDominator;
            }
        }
    }
    return ZR_TRUE;
}

static void promotion_place_phis(SZrPlacePromotion *promotion) {
    TZrBool changed;
    do {
        TZrUInt32 placeIndex;
        changed = ZR_FALSE;
        for (placeIndex = 0u; placeIndex < promotion->placeCount;
             ++placeIndex) {
            TZrUInt32 definitionBlock;
            if (promotion->active[placeIndex] == 0u) continue;
            for (definitionBlock = 0u;
                 definitionBlock < promotion->blockCount;
                 ++definitionBlock) {
                TZrSize definitionSlot = (TZrSize)placeIndex *
                                                promotion->blockCount +
                                        definitionBlock;
                TZrUInt32 frontierBlock;
                if (promotion->definitions[definitionSlot] == 0u &&
                    promotion->hasPhi[definitionSlot] == 0u) {
                    continue;
                }
                for (frontierBlock = 0u;
                     frontierBlock < promotion->blockCount;
                     ++frontierBlock) {
                    TZrSize frontierSlot = (TZrSize)placeIndex *
                                                  promotion->blockCount +
                                          frontierBlock;
                    if (promotion_frontier_has(
                                promotion, definitionBlock, frontierBlock) &&
                        promotion->liveIn[frontierSlot] != 0u &&
                        promotion->hasPhi[frontierSlot] == 0u) {
                        promotion->hasPhi[frontierSlot] = 1u;
                        changed = ZR_TRUE;
                    }
                }
            }
        }
    } while (changed);
}

static TZrBool promotion_append_phis(SZrExecIrFunction *function,
                                     SZrPlacePromotion *promotion,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 newPhiCount = 0u;
    TZrUInt32 blockIndex;
    TZrUInt32 totalPhiCount;
    TZrUInt32 cursor = 0u;
    SZrExecIrPhi *oldPhis = function->phiPool;
    SZrExecIrPhi *combinedPhis;

    for (blockIndex = 0u; blockIndex < promotion->placeCount;
         ++blockIndex) {
        TZrUInt32 candidateBlock;
        for (candidateBlock = 0u;
             candidateBlock < promotion->blockCount;
             ++candidateBlock) {
            if (promotion->hasPhi[(TZrSize)blockIndex *
                                      promotion->blockCount + candidateBlock] !=
                0u) {
                ++newPhiCount;
            }
        }
    }
    if (newPhiCount == 0u) return ZR_TRUE;
    if (newPhiCount > UINT32_MAX - function->phiCount) {
        return promotion_fail(function, diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              0u, 0u);
    }
    totalPhiCount = function->phiCount + newPhiCount;
    combinedPhis = (SZrExecIrPhi *)promotion_calloc(
            function, totalPhiCount, sizeof(*combinedPhis), diagnostic);
    if (combinedPhis == ZR_NULL) return ZR_FALSE;

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 oldOffset;
        TZrUInt32 placeIndex;
        TZrUInt32 blockStart = cursor;
        for (oldOffset = 0u; oldOffset < block->phis.count; ++oldOffset) {
            combinedPhis[cursor++] = oldPhis[block->phis.start + oldOffset];
        }
        for (placeIndex = 0u; placeIndex < promotion->placeCount;
             ++placeIndex) {
            TZrSize slot = (TZrSize)placeIndex * promotion->blockCount +
                           blockIndex;
            const SZrExecIrValue *placeValue;
            TZrExecIrValueId result;
            SZrExecIrPhi phi;
            TZrUInt32 predecessorOffset;
            if (promotion->hasPhi[slot] == 0u) continue;
            placeValue = &function->values[
                    promotion->places[placeIndex] - 1u];
            result = ZrCore_ExecIr_FunctionAddValue(
                    function, placeValue->typeToken, placeValue->ownership,
                    placeValue->nullability);
            if (result == ZR_EXEC_IR_VALUE_ID_INVALID) {
                free(combinedPhis);
                return promotion_fail(
                        function, diagnostic,
                        ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u, block->id);
            }
            memset(&phi, 0, sizeof(phi));
            phi.result = result;
            phi.incomings.start = function->phiIncomingCount;
            for (predecessorOffset = 0u;
                 predecessorOffset < block->predecessorRange.count;
                 ++predecessorOffset) {
                SZrExecIrPhiIncoming incoming;
                incoming.predecessor = function->predecessors[
                        block->predecessorRange.start + predecessorOffset];
                incoming.value = ZR_EXEC_IR_VALUE_ID_INVALID;
                if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                            function, &incoming, 1u, ZR_NULL)) {
                    free(combinedPhis);
                    return promotion_fail(
                            function, diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                            block->terminatorInstructionId, block->id);
                }
                ++phi.incomings.count;
            }
            combinedPhis[cursor] = phi;
            promotion->phiSlots[slot] = cursor + 1u;
            ++cursor;
        }
        block->phis.start = blockStart;
        block->phis.count = cursor - blockStart;
    }
    free(oldPhis);
    function->phiPool = combinedPhis;
    function->phiCount = totalPhiCount;
    function->phiCapacity = totalPhiCount;
    return ZR_TRUE;
}

static void promotion_rewrite_store(SZrExecIrInstruction *instruction) {
    TZrExecIrSourceId sourceId = instruction->sourceId;
    memset(instruction, 0, sizeof(*instruction));
    instruction->opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction->sourceId = sourceId;
}

static void promotion_rewrite_load(SZrExecIrFunction *function,
                                   SZrExecIrInstruction *instruction,
                                   TZrExecIrValueId value) {
    instruction->opcode = ZR_EXEC_IR_OPCODE_COPY;
    instruction->flags = 0u;
    function->operands[instruction->operandRange.start] = value;
    memset(&instruction->phiRange, 0, sizeof(instruction->phiRange));
    memset(&instruction->successorRange, 0,
           sizeof(instruction->successorRange));
    memset(&instruction->memoryIn, 0, sizeof(instruction->memoryIn));
    memset(&instruction->memoryOut, 0, sizeof(instruction->memoryOut));
    instruction->effectIn = 0u;
    instruction->effectOut = 0u;
    instruction->layoutId = 0u;
    instruction->deoptId = 0u;
    instruction->bindingRow = 0u;
}

static TZrBool promotion_rename(SZrExecIrFunction *function,
                                SZrPlacePromotion *promotion,
                                SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrBlockId *stack = (TZrExecIrBlockId *)promotion_calloc(
            function, function->blockCount, sizeof(*stack), diagnostic);
    TZrUInt32 stackCount = 0u;
    TZrBool result = ZR_FALSE;
    if (stack == ZR_NULL) return ZR_FALSE;
    stack[stackCount++] = function->entryBlockId;

    while (stackCount != 0u) {
        TZrExecIrBlockId blockId = stack[--stackCount];
        SZrExecIrBlock *block = &function->blocks[blockId - 1u];
        TZrExecIrBlockId parent = block->immediateDominator;
        TZrUInt32 placeIndex;
        TZrUInt32 instructionIndex;
        TZrUInt32 childIndex;

        for (placeIndex = 0u; placeIndex < promotion->placeCount;
             ++placeIndex) {
            TZrSize outSlot = (TZrSize)(blockId - 1u) *
                                     promotion->placeCount + placeIndex;
            TZrSize phiSlot = (TZrSize)placeIndex * promotion->blockCount +
                              blockId - 1u;
            if (promotion->active[placeIndex] == 0u) continue;
            if (blockId != function->entryBlockId) {
                promotion->outDefinitions[outSlot] =
                        promotion->outDefinitions[
                                (TZrSize)(parent - 1u) *
                                        promotion->placeCount + placeIndex];
            }
            if (promotion->phiSlots[phiSlot] != 0u) {
                promotion->outDefinitions[outSlot] =
                        function->phiPool[
                                promotion->phiSlots[phiSlot] - 1u].result;
            }
        }

        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                        block->instructionRange.count;
             ++instructionIndex) {
            SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            if ((instruction->opcode == ZR_EXEC_IR_OPCODE_LOAD ||
                 instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) &&
                instruction->operandRange.count != 0u) {
                TZrExecIrValueId address = function->operands[
                        instruction->operandRange.start];
                TZrUInt32 candidate =
                        address <= promotion->originalValueCount
                                ? promotion->placeByValue[address]
                                : UINT32_MAX;
                if (candidate != UINT32_MAX &&
                    promotion->active[candidate] != 0u) {
                    TZrSize outSlot = (TZrSize)(blockId - 1u) *
                                             promotion->placeCount + candidate;
                    if (instruction->opcode == ZR_EXEC_IR_OPCODE_STORE) {
                        promotion->outDefinitions[outSlot] =
                                function->operands[
                                        instruction->operandRange.start + 1u];
                        promotion_rewrite_store(instruction);
                    } else {
                        TZrExecIrValueId current =
                                promotion->outDefinitions[outSlot];
                        if (current == ZR_EXEC_IR_VALUE_ID_INVALID) {
                            promotion_fail(
                                    function, diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                    instructionIndex + 1u, blockId);
                            goto cleanup;
                        }
                        promotion_rewrite_load(function, instruction, current);
                    }
                }
            }
        }

        for (childIndex = function->blockCount; childIndex-- != 0u;) {
            if (childIndex + 1u != function->entryBlockId &&
                function->blocks[childIndex].immediateDominator == blockId) {
                stack[stackCount++] = childIndex + 1u;
            }
        }
    }

    {
        TZrUInt32 placeIndex;
        for (placeIndex = 0u; placeIndex < promotion->placeCount;
             ++placeIndex) {
            TZrUInt32 blockIndex;
            if (promotion->active[placeIndex] == 0u) continue;
            for (blockIndex = 0u; blockIndex < promotion->blockCount;
                 ++blockIndex) {
                TZrSize slot = (TZrSize)placeIndex * promotion->blockCount +
                               blockIndex;
                SZrExecIrPhi *phi;
                const SZrExecIrBlock *block;
                TZrUInt32 predecessorOffset;
                if (promotion->phiSlots[slot] == 0u) continue;
                phi = &function->phiPool[promotion->phiSlots[slot] - 1u];
                block = &function->blocks[blockIndex];
                for (predecessorOffset = 0u;
                     predecessorOffset < block->predecessorRange.count;
                     ++predecessorOffset) {
                    TZrExecIrBlockId predecessor = function->predecessors[
                            block->predecessorRange.start + predecessorOffset];
                    TZrExecIrValueId incoming = promotion->outDefinitions[
                            (TZrSize)(predecessor - 1u) *
                                    promotion->placeCount + placeIndex];
                    if (incoming == ZR_EXEC_IR_VALUE_ID_INVALID) {
                        promotion_fail(
                                function, diagnostic,
                                ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                block->terminatorInstructionId, block->id);
                        goto cleanup;
                    }
                    function->phiIncoming[
                            phi->incomings.start + predecessorOffset].value =
                            incoming;
                }
            }
        }
    }
    result = ZR_TRUE;

cleanup:
    free(stack);
    return result;
}

static TZrBool promotion_transform(SZrExecIrFunction *function,
                                   TZrBool *changed,
                                   SZrExecIrDiagnostic *diagnostic) {
    SZrPlacePromotion promotion;
    TZrUInt32 placeIndex;
    TZrBool result = ZR_FALSE;
    TZrBool anyActive = ZR_FALSE;

    if (!promotion_allocate_analysis(function, &promotion, diagnostic)) {
        promotion_free(&promotion);
        return ZR_FALSE;
    }
    if (!promotion_build_instruction_blocks(
                function, &promotion, diagnostic)) {
        goto cleanup;
    }
    promotion_screen_uses(function, &promotion);
    if (!promotion_collect_local_facts(function, &promotion)) {
        promotion_fail(function, diagnostic,
                       ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u, 0u);
        goto cleanup;
    }
    for (placeIndex = 0u; placeIndex < promotion.placeCount; ++placeIndex) {
        if (promotion.active[placeIndex] != 0u) {
            anyActive = ZR_TRUE;
            break;
        }
    }
    if (!anyActive) {
        result = ZR_TRUE;
        goto cleanup;
    }
    promotion_compute_liveness(function, &promotion);
    if (!promotion_compute_frontiers(function, &promotion, diagnostic)) {
        goto cleanup;
    }
    promotion_place_phis(&promotion);
    if (!promotion_append_phis(function, &promotion, diagnostic) ||
        !promotion_rename(function, &promotion, diagnostic)) {
        goto cleanup;
    }
    *changed = ZR_TRUE;
    result = ZR_TRUE;

cleanup:
    promotion_free(&promotion);
    return result;
}

TZrBool zr_parser_exec_ir_promote_places(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction candidate;
    TZrBool changed = ZR_FALSE;

    if (!promotion_has_memory_access(function)) return ZR_TRUE;
    if (function->sealed) {
        return promotion_fail(function, diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_SEALED, 0u, 0u);
    }
    if (!promotion_verify_candidate(function, diagnostic)) {
        return ZR_FALSE;
    }

    ZrCore_ExecIr_FunctionInit(&candidate);
    if (!ZrCore_ExecIr_CloneFunction(function, &candidate, diagnostic)) {
        return ZR_FALSE;
    }
    if (!ZrParser_ExecIr_ComputeDominators(&candidate, diagnostic) ||
        !promotion_transform(&candidate, &changed, diagnostic) ||
        (changed && !promotion_verify_candidate(&candidate, diagnostic))) {
        ZrCore_ExecIr_FreeFunction(&candidate);
        return ZR_FALSE;
    }
    if (!changed) {
        ZrCore_ExecIr_FreeFunction(&candidate);
        return ZR_TRUE;
    }
    ZrCore_ExecIr_FreeFunction(function);
    *function = candidate;
    return ZR_TRUE;
}
