#include "zr_vm_core/exec_ir_state_map_liveness.h"

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

void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness) {
    free(liveness->before);
    free(liveness->after);
    free(liveness->semanticBefore);
    free(liveness->semanticAfter);
    memset(liveness, 0, sizeof(*liveness));
}

TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness,
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
                              TZrUInt8 *row, TZrBool fieldsOnly) {
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
             !fieldsOnly && index < state->valueRange.start + state->valueRange.count;
             ++index) {
            zr_live_add(row, function->deoptValues[index]);
        }
        /* A scalarized field is a recovery use even when optimized code no
         * longer reads it. Object references use stable IDs, not SSA values;
         * scan definitions once so aliases and cycles need no recursion. */
        for (index = state->aggregates.start;
             index < state->aggregates.start + state->aggregates.count;
             ++index) {
            const SZrExecIrDeoptAggregate *aggregate = &function->deoptAggregates[index];
            TZrUInt32 field;
            for (field = aggregate->fields.start;
                 field < aggregate->fields.start + aggregate->fields.count; ++field) {
                const SZrExecIrDeoptAggregateField *binding =
                        &function->deoptAggregateFields[field];
                if (binding->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE) {
                    zr_live_add(row, binding->valueId);
                }
            }
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
                                      TZrUInt8 *row, TZrBool semanticOnly) {
    TZrUInt32 end = instructions.start + instructions.count;
    while (end > instructions.start) {
        TZrUInt32 index = --end;
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 valueIndex;
        size_t offset = (size_t)index * liveness->rowBytes;
        /* Recovery may use the values at either phase of this checkpoint,
         * even when the optimized instruction itself has no ordinary uses. */
        zr_live_add_deopt(function, instruction, row, ZR_FALSE);
        memcpy(liveness->after + offset, row, liveness->rowBytes);
        for (valueIndex = instruction->resultRange.start;
             valueIndex < instruction->resultRange.start + instruction->resultRange.count;
             ++valueIndex) {
            zr_live_remove(row, function->results[valueIndex]);
        }
        /* One recipe is shared by all emitted phases. Preserve its demanded
         * fields before the effect too, so ownership analysis rejects a field
         * that only becomes available as this instruction's result. */
        zr_live_add_deopt(function, instruction, row, ZR_TRUE);
        for (valueIndex = instruction->operandRange.start;
             (!semanticOnly || instruction->opcode != ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) &&
             valueIndex < instruction->operandRange.start + instruction->operandRange.count;
             ++valueIndex) {
            zr_live_add(row, function->operands[valueIndex]);
        }
        memcpy(liveness->before + offset, row, liveness->rowBytes);
    }
}

static EZrExecutionDiagnosticCode zr_live_build(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness,
        TZrBool semanticOnly) {
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
        ZrCore_ExecIr_StateMapLivenessFree(liveness);
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
            zr_live_scan_instructions(function, instructions, liveness, row, semanticOnly);
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

EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness) {
    SZrStateMapLiveness semantic = {0};
    TZrUInt32 index, at;
    EZrExecutionDiagnosticCode code;
    memset(liveness, 0, sizeof(*liveness));
    /* OwnerAnalysis validates CFG and SSA pools before this helper is used.
     * Recovery-only uses have their own pools and need independent checks. */
    if (function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL))
        return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
    for (index = 0u; index < function->deoptStateCount; ++index) {
        SZrExecIrRange range = function->deoptStates[index].valueRange;
        if (range.start > function->deoptValueCount ||
            range.count > function->deoptValueCount - range.start)
            return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        for (at = range.start; at < range.start + range.count; ++at)
            if (function->deoptValues[at] == 0u || function->deoptValues[at] > function->valueCount)
                return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
    }
    code = zr_live_build(function, liveness, ZR_FALSE);
    if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) return code;
    code = zr_live_build(function, &semantic, ZR_TRUE);
    if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        ZrCore_ExecIr_StateMapLivenessFree(liveness);
        return code;
    }
    liveness->semanticBefore = semantic.before;
    liveness->semanticAfter = semantic.after;
    return ZR_EXECUTION_DIAGNOSTIC_NONE;
}

static TZrBool zr_live_semantic(const SZrStateMapLiveness *liveness,
                                TZrExecIrInstructionId instruction,
                                TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    SZrStateMapLiveness semantic = *liveness;
    semantic.before = liveness->semanticBefore;
    semantic.after = liveness->semanticAfter;
    return ZrCore_ExecIr_StateMapLivenessContains(&semantic, instruction, value,
            (TZrBool)(phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT));
}

TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    TZrUInt8 mask = ZrCore_ExecIr_OwnerStateMaskAt(ownership, instruction, value, phase);
    const SZrExecIrInstruction *ins;
    const SZrExecIrOpcodeInfo *info;
    TZrUInt32 index;
    if (mask == 0u || phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT ||
        zr_live_semantic(liveness, instruction, value, phase)) return mask;
    ins = &function->instructions[instruction - 1u];
    info = ZrCore_ExecIr_OpcodeInfo(ins->opcode);
    /* An after-effect checkpoint precedes edge selection. Cleanup-only
     * results may be absent on the exceptional successor. Ordinary result
     * uses retain the established normal-edge availability contract. */
    if ((info->flags & (ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR | ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW)) ==
        (ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR | ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW)) {
        for (index = ins->resultRange.start;
             index < ins->resultRange.start + ins->resultRange.count; ++index) {
            if (function->results[index] == value)
                mask |= ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED);
        }
    }
    return mask;
}

EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(
            function, ownership, liveness, instruction, value, phase);
    TZrUInt32 state;
    EZrExecIrOwnership kind;
    if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED))
        return ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN))
        return ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN;
    if (mask == 0u || zr_live_semantic(liveness, instruction, value, phase))
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    kind = function->values[value - 1u].ownership;
    if ((kind != ZR_EXEC_IR_OWNERSHIP_UNIQUE && kind != ZR_EXEC_IR_OWNERSHIP_SHARED) ||
        (mask & ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN)) != 0u)
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    for (state = ZR_EXEC_IR_STATE_MAP_OWNER_MOVED;
         state < ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL; ++state) {
        if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(state)) return (EZrExecIrStateMapOwnerState)state;
    }
    return ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL;
}
