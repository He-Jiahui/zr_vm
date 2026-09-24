#include "exec_ir_state_map_liveness.h"

#include <stdlib.h>
#include <string.h>

static void zr_live_add(TZrUInt8 *row, TZrExecIrValueId value) {
    TZrUInt32 bit = value - 1u;
    row[bit / 8u] |= (TZrUInt8)(1u << (bit % 8u));
}

static void zr_live_remove(TZrUInt8 *row, TZrExecIrValueId value) {
    TZrUInt32 bit = value - 1u;
    row[bit / 8u] &= (TZrUInt8)~(1u << (bit % 8u));
}

void zr_state_map_liveness_free(SZrStateMapLiveness *liveness) {
    free(liveness->before);
    free(liveness->after);
    memset(liveness, 0, sizeof(*liveness));
}

TZrBool zr_state_map_liveness_contains(const SZrStateMapLiveness *liveness,
                                        TZrExecIrInstructionId instruction,
                                        TZrExecIrValueId value,
                                        TZrBool after) {
    const TZrUInt8 *rows = after ? liveness->after : liveness->before;
    TZrUInt32 bit = value - 1u;
    if (rows == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((rows[(size_t)(instruction - 1u) * liveness->rowBytes +
                          bit / 8u] & (1u << (bit % 8u))) != 0u);
}

static void zr_live_add_deopt(const SZrExecIrFunction *function,
                              const SZrExecIrInstruction *instruction,
                              TZrUInt8 *row) {
    TZrUInt32 stateIndex;
    if (instruction->deoptId == 0u) {
        return;
    }
    for (stateIndex = 0u; stateIndex < function->deoptStateCount; ++stateIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[stateIndex];
        TZrUInt32 index;
        if (state->id != instruction->deoptId) {
            continue;
        }
        for (index = state->valueRange.start;
             index < state->valueRange.start + state->valueRange.count;
             ++index) {
            zr_live_add(row, function->deoptValues[index]);
        }
    }
}

/* PHI operands belong to predecessor edges. The PHI result is killed at the
 * successor's entry, so neither it nor another edge's input leaks backwards. */
static void zr_live_add_successors(const SZrExecIrFunction *function,
                                   const SZrExecIrBlock *block,
                                   const TZrUInt8 *liveIn,
                                   size_t rowBytes,
                                   TZrUInt8 *row) {
    TZrUInt32 edge;
    for (edge = block->successorRange.start;
         edge < block->successorRange.start + block->successorRange.count;
         ++edge) {
        const SZrExecIrBlock *successor =
                &function->blocks[function->successors[edge] - 1u];
        const TZrUInt8 *input = liveIn + (size_t)(successor->id - 1u) * rowBytes;
        size_t byte;
        TZrUInt32 phiIndex;
        for (byte = 0u; byte < rowBytes; ++byte) {
            row[byte] |= input[byte];
        }
        for (phiIndex = successor->phis.start;
             phiIndex < successor->phis.start + successor->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incoming;
            for (incoming = phi->incomings.start;
                 incoming < phi->incomings.start + phi->incomings.count;
                 ++incoming) {
                if (function->phiIncoming[incoming].predecessor == block->id) {
                    zr_live_add(row, function->phiIncoming[incoming].value);
                }
            }
        }
    }
}

static void zr_live_scan_instructions(const SZrExecIrFunction *function,
                                      SZrExecIrRange instructions,
                                      SZrStateMapLiveness *liveness,
                                      TZrUInt8 *row) {
    TZrUInt32 end = instructions.start + instructions.count;
    while (end > instructions.start) {
        TZrUInt32 index = --end;
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 valueIndex;
        size_t offset = (size_t)index * liveness->rowBytes;
        /* Recovery may use the values at either phase of this checkpoint,
         * even when the optimized instruction itself has no ordinary uses. */
        zr_live_add_deopt(function, instruction, row);
        memcpy(liveness->after + offset, row, liveness->rowBytes);
        for (valueIndex = instruction->resultRange.start;
             valueIndex < instruction->resultRange.start + instruction->resultRange.count;
             ++valueIndex) {
            zr_live_remove(row, function->results[valueIndex]);
        }
        for (valueIndex = instruction->operandRange.start;
             valueIndex < instruction->operandRange.start + instruction->operandRange.count;
             ++valueIndex) {
            zr_live_add(row, function->operands[valueIndex]);
        }
        memcpy(liveness->before + offset, row, liveness->rowBytes);
    }
}

EZrExecutionDiagnosticCode zr_state_map_liveness_build(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness) {
    TZrUInt8 *liveIn;
    TZrUInt8 *row;
    TZrUInt32 blockCount = function->blockCount != 0u ? function->blockCount : 1u;
    TZrBool changed;
    memset(liveness, 0, sizeof(*liveness));
    if (function->valueCount == 0u || function->instructionCount == 0u) {
        return ZR_EXECUTION_DIAGNOSTIC_NONE;
    }
    liveness->rowBytes = (size_t)(function->valueCount / 8u) +
                         (function->valueCount % 8u != 0u ? 1u : 0u);
    if ((size_t)function->instructionCount > SIZE_MAX / liveness->rowBytes ||
        (size_t)blockCount > SIZE_MAX / liveness->rowBytes) {
        return ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
    }
    liveness->before = (TZrUInt8 *)calloc(function->instructionCount, liveness->rowBytes);
    liveness->after = (TZrUInt8 *)calloc(function->instructionCount, liveness->rowBytes);
    liveIn = (TZrUInt8 *)calloc(blockCount, liveness->rowBytes);
    row = (TZrUInt8 *)calloc(1u, liveness->rowBytes);
    if (liveness->before == ZR_NULL || liveness->after == ZR_NULL ||
        liveIn == ZR_NULL || row == ZR_NULL) {
        free(liveIn);
        free(row);
        zr_state_map_liveness_free(liveness);
        return ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    }
    do {
        TZrUInt32 blockIndex = blockCount;
        changed = ZR_FALSE;
        while (blockIndex != 0u) {
            const SZrExecIrBlock *block;
            SZrExecIrRange instructions = {0};
            TZrUInt8 *input;
            --blockIndex;
            block = function->blockCount != 0u ? &function->blocks[blockIndex] : ZR_NULL;
            memset(row, 0, liveness->rowBytes);
            if (block != ZR_NULL) {
                instructions = block->instructionRange;
                zr_live_add_successors(function, block, liveIn, liveness->rowBytes, row);
            } else {
                /* The verified blockless form is one straight-line region. */
                instructions.count = function->instructionCount;
            }
            zr_live_scan_instructions(function, instructions, liveness, row);
            if (block != ZR_NULL) {
                TZrUInt32 phiIndex;
                for (phiIndex = block->phis.start;
                     phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
                    zr_live_remove(row, function->phiPool[phiIndex].result);
                }
            }
            input = liveIn + (size_t)blockIndex * liveness->rowBytes;
            if (memcmp(input, row, liveness->rowBytes) != 0) {
                memcpy(input, row, liveness->rowBytes);
                changed = ZR_TRUE;
            }
        }
    } while (changed);
    free(liveIn);
    free(row);
    return ZR_EXECUTION_DIAGNOSTIC_NONE;
}
