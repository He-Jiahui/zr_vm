#include "exec_ir_copy_aliases.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

static TZrBool zr_copy_size_mul_overflow(TZrUInt64 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     count > (TZrUInt64)(SIZE_MAX / elementSize));
}
static TZrExecIrBlockId zr_copy_containing_block(const SZrExecIrFunction *function,
                                                 TZrUInt32 instructionIndex) {
    TZrUInt32 index;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (instructionIndex >= block->instructions.start &&
            instructionIndex - block->instructions.start < block->instructions.count)
            return block->id;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool zr_copy_block_dominates(const SZrExecIrFunction *function,
                                       TZrExecIrBlockId definition,
                                       TZrExecIrBlockId use) {
    TZrUInt32 steps = 0u;
    if (definition == ZR_EXEC_IR_BLOCK_ID_INVALID || use == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        definition > function->blockCount || use > function->blockCount) return ZR_FALSE;
    while (use != ZR_EXEC_IR_BLOCK_ID_INVALID && steps++ <= function->blockCount) {
        if (use == definition) return ZR_TRUE;
        use = function->blocks[use - 1u].immediateDominator;
    }
    return ZR_FALSE;
}

static TZrExecIrValueId zr_copy_alias_for_use(const SZrExecIrFunction *function,
                                              const TZrExecIrValueId *aliases,
                                              const TZrUInt32 *aliasDefinitions,
                                              const TZrExecIrBlockId *aliasBlocks,
                                              TZrExecIrValueId value,
                                              TZrUInt32 useInstructionIndex) {
    TZrUInt32 steps = 0u;
    TZrExecIrBlockId useBlock = zr_copy_containing_block(function, useInstructionIndex);
    while (value != ZR_EXEC_IR_VALUE_ID_INVALID && value <= function->valueCount &&
           aliases[value] != ZR_EXEC_IR_VALUE_ID_INVALID && aliases[value] != value &&
           steps++ <= function->valueCount) {
        TZrUInt32 definition = aliasDefinitions[value];
        if (definition == 0u) break;
        if (function->blockCount == 0u) {
            if (definition >= useInstructionIndex + 1u) break;
        } else {
            TZrExecIrBlockId definitionBlock = aliasBlocks[value];
            if (definitionBlock == useBlock) {
                if (definition >= useInstructionIndex + 1u) break;
            } else if (!zr_copy_block_dominates(function, definitionBlock, useBlock)) break;
        }
        value = aliases[value];
    }
    return value;
}

static void zr_copy_set_changed(SZrExecIrPassContext *context,
                                TZrBool *changed,
                                TZrExecIrSourceId sourceId) {
    if (changed != ZR_NULL && !*changed) *changed = ZR_TRUE;
    if (context != ZR_NULL && context->lastSourceId == 0u) context->lastSourceId = sourceId;
}

static TZrBool zr_copy_has_boundary(const SZrExecIrInstruction *instruction) {
    return (TZrBool)(instruction->flags != 0u || instruction->bindingRow != 0u ||
                     instruction->deoptId != 0u ||
                     instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u);
}

/* Match this ordered predecessor slot to its actual successor occurrence.
 * Duplicate predecessor IDs are separate uses, even for the same target. */
static TZrBool zr_copy_phi_predecessor(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        const SZrExecIrBlock *target, TZrUInt32 slot,
        TZrExecIrBlockId predecessor, TZrUInt32 *terminator) {
    const SZrExecIrBlock *block;
    const SZrExecIrInstruction *instruction;
    TZrUInt32 occurrence = 0u;
    TZrUInt32 matched = 0u;
    TZrUInt32 at;
    if (predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID || predecessor > function->blockCount ||
        slot >= target->predecessors.count ||
        function->predecessors[target->predecessors.start + slot] != predecessor)
        return ZR_FALSE;
    block = &function->blocks[predecessor - 1u];
    if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u ||
        block->instructions.count == 0u ||
        block->terminatorInstructionId != block->instructions.start + block->instructions.count)
        return ZR_FALSE;
    instruction = &function->instructions[block->terminatorInstructionId - 1u];
    if ((instruction->opcode != ZR_EXEC_IR_OPCODE_BRANCH &&
         instruction->opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
         instruction->opcode != ZR_EXEC_IR_OPCODE_SWITCH) ||
        zr_copy_has_boundary(instruction) ||
        instruction->successorRange.start != block->successors.start ||
        instruction->successorRange.count != block->successors.count)
        return ZR_FALSE;
    /* A pure final branch does not erase an interior observable boundary.
     * Consult intrinsic opcode effects too: zero encoded CALL flags do not
     * establish provider purity. This scan only gates the new PHI path. */
    for (at = block->instructions.start; at < block->terminatorInstructionId - 1u; ++at) {
        const SZrExecIrInstruction *body = &function->instructions[at];
        const SZrExecIrOpcodeInfo *info;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        info = ZrCore_ExecIr_OpcodeInfo(body->opcode);
        if (info == ZR_NULL || zr_copy_has_boundary(body) ||
            info->memoryReads != 0u || info->memoryWrites != 0u ||
            (info->flags & ~ZR_EXEC_IR_SCHEMA_FLAG_PRODUCES_VALUE) != 0u)
            return ZR_FALSE;
    }
    for (at = 0u; at < slot; ++at) {
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        if (function->predecessors[target->predecessors.start + at] == predecessor)
            ++occurrence;
    }
    for (at = 0u; at < block->successors.count; ++at) {
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        if (function->successors[block->successors.start + at] != target->id) continue;
        if (matched++ == occurrence) {
            *terminator = block->terminatorInstructionId;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrExecIrValueId zr_copy_alias_for_phi(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        const TZrExecIrValueId *aliases, const TZrUInt32 *definitions,
        const TZrExecIrBlockId *blocks, TZrExecIrValueId original,
        TZrExecIrBlockId predecessor, TZrUInt32 terminator) {
    TZrExecIrValueId value = original;
    TZrUInt32 before = terminator;
    TZrUInt32 steps = 0u;
    while (value != ZR_EXEC_IR_VALUE_ID_INVALID && value <= function->valueCount &&
           aliases[value] != ZR_EXEC_IR_VALUE_ID_INVALID && aliases[value] != value) {
        const SZrExecIrInstruction *copy;
        TZrUInt32 definition = definitions[value];
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return original;
        if (steps++ >= function->valueCount) return original;
        if (blocks[value] != predecessor || definition == 0u || definition >= before)
            break;
        copy = &function->instructions[definition - 1u];
        if (copy->opcode != ZR_EXEC_IR_OPCODE_COPY || zr_copy_has_boundary(copy) ||
            copy->phiRange.count != 0u || copy->successorRange.count != 0u)
            break;
        /* Strictly decreasing local definitions prove order and exclude cycles.
         * The alias table already checked UNKNOWN ownership and tombstones. */
        before = definition;
        value = aliases[value];
    }
    return value;
}

static void zr_copy_rewrite_phis(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        const TZrExecIrValueId *aliases, const TZrUInt32 *definitions,
        const TZrExecIrBlockId *blocks, TZrBool *changed) {
    TZrUInt32 blockIndex;
    /* Logical maps and other identity-bearing maps need a separate liveness
     * proof. The finite PHI rewrite never repairs those tables. */
    if (function->stateMap != ZR_NULL || function->gcMapCount != 0u ||
        function->deoptStateCount != 0u || function->deoptValueCount != 0u ||
        function->deoptAggregateCount != 0u) return;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u) continue;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 slot;
            if (phi->incomings.count != block->predecessors.count) continue;
            for (slot = 0u; slot < phi->incomings.count; ++slot) {
                SZrExecIrPhiIncoming *incoming =
                        &function->phiIncoming[phi->incomings.start + slot];
                TZrUInt32 terminator;
                TZrExecIrValueId value;
                if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return;
                if (!zr_copy_phi_predecessor(function, context, block, slot,
                                              incoming->predecessor, &terminator)) {
                    if (context != ZR_NULL && context->budgetExhausted) return;
                    continue;
                }
                value = zr_copy_alias_for_phi(function, context, aliases, definitions,
                                               blocks, incoming->value,
                                               incoming->predecessor, terminator);
                if (context != ZR_NULL && context->budgetExhausted) return;
                if (value != incoming->value) {
                    incoming->value = value;
                    zr_copy_set_changed(context, changed,
                            function->instructions[terminator - 1u].sourceId);
                }
            }
        }
    }
}

TZrBool ZrParser_ExecIr_RewriteCopyAliases(SZrExecIrFunction *function,
                               SZrExecIrPassContext *context,
                               TZrBool *changed,
                               SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *aliases;
    TZrUInt32 *aliasDefinitions;
    TZrExecIrBlockId *aliasBlocks;
    TZrUInt32 index;
    size_t bytes;
    if (function->valueCount == UINT32_MAX ||
        zr_copy_size_mul_overflow((TZrUInt64)function->valueCount + 1u,
                                  sizeof(*aliases))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                       function, 0u, 0u, UINT32_MAX, function->valueCount);
        return ZR_FALSE;
    }
    bytes = (size_t)((TZrUInt64)function->valueCount + 1u) * sizeof(*aliases);
    aliases = (TZrExecIrValueId *)calloc(1u, bytes);
    aliasDefinitions = (TZrUInt32 *)calloc(1u, bytes);
    aliasBlocks = (TZrExecIrBlockId *)calloc(1u, bytes);
    if (aliases == ZR_NULL || aliasDefinitions == ZR_NULL || aliasBlocks == ZR_NULL) {
        free(aliases);
        free(aliasDefinitions);
        free(aliasBlocks);
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, 0u, 0u, function->valueCount, 0u);
        return ZR_FALSE;
    }
    for (index = 1u; index <= function->valueCount; ++index) aliases[index] = index;
    /* COPY payload equality does not prove availability after consumption.
     * Mark every consumed identity, including UNKNOWN values and consumers
     * after a potential reuse. INVALID aliases must never be overwritten. */
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 at;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) {
            free(aliases);
            free(aliasDefinitions);
            free(aliasBlocks);
            return ZR_TRUE;
        }
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_MOVE &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_DROP &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) continue;
        for (at = instruction->operands.start;
             at < instruction->operands.start + instruction->operands.count; ++at)
            aliases[function->operands[at]] = ZR_EXEC_IR_VALUE_ID_INVALID;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *instruction = &function->instructions[index];
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_COPY &&
            instruction->operands.count == 1u && instruction->results.count == 1u) {
            TZrExecIrValueId source = function->operands[instruction->operands.start];
            TZrExecIrValueId destination = function->results[instruction->results.start];
            if (source != ZR_EXEC_IR_VALUE_ID_INVALID && source <= function->valueCount &&
                destination != ZR_EXEC_IR_VALUE_ID_INVALID && destination <= function->valueCount &&
                aliases[source] != ZR_EXEC_IR_VALUE_ID_INVALID &&
                aliases[destination] != ZR_EXEC_IR_VALUE_ID_INVALID &&
                function->values[source - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                function->values[destination - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN)
                if (instruction->flags == 0u &&
                    instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                    instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                    instruction->memoryIn.count == 0u && instruction->memoryOut.count == 0u) {
                    aliases[destination] = source;
                    aliasDefinitions[destination] = index + 1u;
                    aliasBlocks[destination] = zr_copy_containing_block(function, index);
                }
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 operandIndex;
        for (operandIndex = instruction->operands.start;
             operandIndex < instruction->operands.start + instruction->operands.count;
             ++operandIndex) {
            TZrExecIrValueId oldValue = function->operands[operandIndex];
            TZrExecIrValueId newValue = zr_copy_alias_for_use(function, aliases,
                                                              aliasDefinitions, aliasBlocks,
                                                              oldValue, index);
            if (newValue != oldValue) {
                function->operands[operandIndex] = newValue;
                zr_copy_set_changed(context, changed, instruction->sourceId);
            }
        }
    }
    zr_copy_rewrite_phis(function, context, aliases, aliasDefinitions, aliasBlocks, changed);
    free(aliases);
    free(aliasDefinitions);
    free(aliasBlocks);
    return ZR_TRUE;
}
