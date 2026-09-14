#include "zr_vm_parser/exec_ir_loops.h"

#include "zr_vm_parser/exec_ir_pass_manager.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void zr_licm_diagnostic(SZrExecIrDiagnostic *diagnostic,
                               EZrExecutionDiagnosticCode code,
                               const SZrExecIrFunction *function,
                               TZrExecIrBlockId blockId,
                               TZrExecIrInstructionId instructionId) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (function != ZR_NULL) diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
}

static TZrBool zr_licm_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_licm_function_storage_valid(const SZrExecIrFunction *function,
                                              SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL ||
        function->instructionCount > function->instructionCapacity ||
        function->valueCount > function->valueCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL)) {
        zr_licm_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrExecIrBlockId zr_licm_instruction_block(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId) {
    TZrUInt32 index;
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount) return 0u;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (!zr_licm_range_valid(block->instructionRange,
                                 function->instructionCount)) return 0u;
        if (instructionId - 1u >= block->instructionRange.start &&
            instructionId - 1u < block->instructionRange.start +
                                      block->instructionRange.count)
            return block->id;
    }
    return 0u;
}

static TZrBool zr_licm_loop_contains(const SZrExecIrLoopInfo *info,
                                     const SZrExecIrLoop *loop,
                                     TZrExecIrBlockId blockId) {
    const TZrExecIrBlockId *members;
    TZrUInt32 index;
    members = ZrParser_ExecIr_LoopMembers(info, loop);
    if (members == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < loop->memberCount; ++index)
        if (members[index] == blockId) return ZR_TRUE;
    return ZR_FALSE;
}

static TZrBool zr_licm_value_is_constant(const SZrExecIrFunction *function,
                                         TZrExecIrValueId valueId,
                                         TZrUInt64 *bits) {
    TZrExecIrInstructionId definition;
    if (function == ZR_NULL || bits == ZR_NULL || valueId == 0u ||
        valueId > function->valueCount || function->values == ZR_NULL)
        return ZR_FALSE;
    definition = function->values[valueId - 1u].definition;
    if (definition == 0u || definition > function->instructionCount ||
        function->instructions == ZR_NULL)
        return ZR_FALSE;
    if (function->instructions[definition - 1u].opcode !=
            (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT ||
        function->instructions[definition - 1u].results.count != 1u)
        return ZR_FALSE;
    *bits = function->instructions[definition - 1u].layoutId;
    return ZR_TRUE;
}

static TZrBool zr_licm_instruction_is_pure(const SZrExecIrInstruction *instruction) {
    if (instruction == ZR_NULL || instruction->flags != 0u ||
        instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
        instruction->effectIn != 0u || instruction->effectOut != 0u ||
        instruction->deoptId != 0u || instruction->bindingRow != 0u ||
        instruction->phiRange.count != 0u || instruction->successorRange.count != 0u)
        return ZR_FALSE;
    switch ((EZrExecIrOpcode)instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_CONVERT:
            return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
            /* Arithmetic without a range proof is considered safe only when
             * both operands are constants and the uint32 representation does
             * not overflow.  A later range-aware pass can widen this set. */
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_licm_arithmetic_is_safe(const SZrExecIrFunction *function,
                                           const SZrExecIrInstruction *instruction) {
    TZrUInt64 left;
    TZrUInt64 right;
    TZrUInt64 result;
    if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT ||
        instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_COPY ||
        instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONVERT)
        return ZR_TRUE;
    if (instruction->operands.count != 2u || function->operands == ZR_NULL ||
        !zr_licm_value_is_constant(function,
                                    function->operands[instruction->operands.start],
                                    &left) ||
        !zr_licm_value_is_constant(function,
                                    function->operands[instruction->operands.start + 1u],
                                    &right))
        return ZR_FALSE;
    if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD) {
        if (left > UINT64_MAX - right) return ZR_FALSE;
        result = left + right;
    } else if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB) {
        if (left < right) return ZR_FALSE;
        result = left - right;
    } else {
        if (right != 0u && left > UINT64_MAX / right) return ZR_FALSE;
        result = left * right;
    }
    return (TZrBool)(result <= UINT32_MAX);
}

static TZrBool zr_licm_operand_invariant(const SZrExecIrFunction *function,
                                          const SZrExecIrLoopInfo *info,
                                          const SZrExecIrLoop *loop,
                                          const TZrUInt8 *invariant,
                                          const SZrExecIrInstruction *instruction) {
    TZrUInt32 operandIndex;
    if (instruction->operands.count == 0u) return ZR_TRUE;
    if (function->operands == ZR_NULL) return ZR_FALSE;
    for (operandIndex = 0u; operandIndex < instruction->operands.count;
         ++operandIndex) {
        TZrExecIrValueId valueId = function->operands[
                instruction->operands.start + operandIndex];
        TZrExecIrInstructionId definition;
        TZrExecIrBlockId definitionBlock;
        if (valueId == 0u || valueId > function->valueCount ||
            function->values == ZR_NULL) return ZR_FALSE;
        definition = function->values[valueId - 1u].definition;
        if (definition == 0u) continue; /* parameter/input */
        definitionBlock = zr_licm_instruction_block(function, definition);
        if (!zr_licm_loop_contains(info, loop, definitionBlock)) continue;
        if (definition > function->instructionCount || invariant == ZR_NULL ||
            invariant[definition - 1u] == 0u) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_licm_result_uses_after(const SZrExecIrFunction *function,
                                         TZrExecIrValueId resultValue,
                                         TZrExecIrInstructionId definitionId) {
    TZrUInt32 instructionIndex;
    TZrBool used = ZR_FALSE;
    if (function == ZR_NULL || resultValue == 0u ||
        function->operands == ZR_NULL) return ZR_FALSE;
    for (instructionIndex = 0u; instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
        TZrUInt32 operandIndex;
        if (!zr_licm_range_valid(instruction->operands, function->operandCount))
            return ZR_FALSE;
        for (operandIndex = 0u; operandIndex < instruction->operands.count;
             ++operandIndex) {
            if (function->operands[instruction->operands.start + operandIndex] !=
                resultValue) continue;
            /* A moved definition must remain before every use in the CFG.
             * The linear-id check is deliberately stricter than a full
             * dominance query and therefore rejects uncertain shapes. */
            if (instructionIndex + 1u <= definitionId) return ZR_FALSE;
            used = ZR_TRUE;
        }
    }
    return used;
}

static TZrBool zr_licm_candidate(const SZrExecIrFunction *function,
                                 const SZrExecIrLoopInfo *info,
                                 const SZrExecIrLoop *loop,
                                 const TZrUInt8 *invariant,
                                 TZrExecIrInstructionId *instructionId,
                                 EZrExecIrLoopReason *reason) {
    const TZrExecIrBlockId *members;
    TZrUInt32 memberIndex;
    if (instructionId == ZR_NULL || reason == ZR_NULL ||
        function == ZR_NULL || info == ZR_NULL || loop == ZR_NULL) return ZR_FALSE;
    *instructionId = 0u;
    *reason = ZR_EXEC_IR_LOOP_REASON_NONE;
    if (!loop->reducible || loop->multipleEntry) {
        *reason = loop->multipleEntry ? ZR_EXEC_IR_LOOP_REASON_MULTIPLE_ENTRY
                                      : ZR_EXEC_IR_LOOP_REASON_IRREDUCIBLE;
        return ZR_FALSE;
    }
    if (!loop->hasPreheader) {
        *reason = ZR_EXEC_IR_LOOP_REASON_NO_PREHEADER;
        return ZR_FALSE;
    }
    if (loop->zeroTrip) {
        *reason = ZR_EXEC_IR_LOOP_REASON_ZERO_TRIP;
        return ZR_FALSE;
    }
    members = ZrParser_ExecIr_LoopMembers(info, loop);
    if (members == ZR_NULL) {
        *reason = ZR_EXEC_IR_LOOP_REASON_INVALID;
        return ZR_FALSE;
    }
    for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
        const SZrExecIrBlock *block = &function->blocks[members[memberIndex] - 1u];
        TZrUInt32 instructionIndex;
        for (instructionIndex = block->instructionRange.start;
             instructionIndex < block->instructionRange.start +
                                 block->instructionRange.count;
             ++instructionIndex) {
            const SZrExecIrInstruction *instruction =
                    &function->instructions[instructionIndex];
            const SZrExecIrOpcodeInfo *opcodeInfo;
            if (instructionIndex + 1u == block->terminatorInstructionId ||
                instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_BRANCH ||
                instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH)
                continue;
            if (!zr_licm_instruction_is_pure(instruction)) {
                *reason = (instruction->flags != 0u || instruction->memoryIn.count != 0u ||
                           instruction->memoryOut.count != 0u || instruction->effectIn != 0u ||
                           instruction->effectOut != 0u)
                              ? ZR_EXEC_IR_LOOP_REASON_EFFECT
                              : ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT;
                continue;
            }
            opcodeInfo = ZrCore_ExecIr_OpcodeInfo(
                    (EZrExecIrOpcode)instruction->opcode);
            if (opcodeInfo == ZR_NULL || instruction->results.count != 1u ||
                !zr_licm_operand_invariant(function, info, loop, invariant,
                                            instruction)) {
                *reason = ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT;
                continue;
            }
            if (!zr_licm_arithmetic_is_safe(function, instruction)) {
                *reason = ZR_EXEC_IR_LOOP_REASON_OVERFLOW;
                continue;
            }
            if (instruction->results.count != 1u || function->results == ZR_NULL ||
                !zr_licm_result_uses_after(
                        function,
                        function->results[instruction->results.start],
                        instructionIndex + 1u)) {
                *reason = ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT;
                continue;
            }
            *instructionId = instructionIndex + 1u;
            *reason = ZR_EXEC_IR_LOOP_REASON_NONE;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt32 zr_licm_map_instruction_id(TZrUInt32 oldId,
                                             TZrUInt32 sourceIndex,
                                             TZrUInt32 destinationIndex) {
    TZrUInt32 oldIndex;
    if (oldId == 0u) return 0u;
    oldIndex = oldId - 1u;
    if (oldIndex == sourceIndex) return destinationIndex + 1u;
    if (destinationIndex <= oldIndex && oldIndex < sourceIndex)
        return oldIndex + 2u;
    return oldId;
}

static TZrBool zr_licm_move_instruction(SZrExecIrFunction *function,
                                        TZrExecIrInstructionId instructionId,
                                        TZrExecIrBlockId preheaderId,
                                        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrBlockId sourceBlockId;
    SZrExecIrBlock *preheader;
    SZrExecIrBlock *sourceBlock;
    TZrUInt32 sourceIndex;
    TZrUInt32 destinationIndex;
    SZrExecIrInstruction saved;
    TZrUInt32 blockIndex;
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount || preheaderId == 0u ||
        preheaderId > function->blockCount) {
        zr_licm_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, preheaderId, instructionId);
        return ZR_FALSE;
    }
    sourceBlockId = zr_licm_instruction_block(function, instructionId);
    if (sourceBlockId == 0u || sourceBlockId == preheaderId) return ZR_FALSE;
    preheader = &function->blocks[preheaderId - 1u];
    sourceBlock = &function->blocks[sourceBlockId - 1u];
    if (preheader->instructionRange.count == 0u ||
        sourceBlock->instructionRange.count < 2u) {
        zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                           function, sourceBlockId, instructionId);
        return ZR_FALSE;
    }
    sourceIndex = instructionId - 1u;
    destinationIndex = preheader->instructionRange.start +
                       preheader->instructionRange.count - 1u;
    if (sourceIndex <= destinationIndex ||
        destinationIndex >= function->instructionCount) {
        /* A preheader must precede its natural loop.  Refuse unusual block
         * ordering instead of changing a terminator's control-flow position. */
        return ZR_FALSE;
    }
    saved = function->instructions[sourceIndex];
    memmove(&function->instructions[destinationIndex + 1u],
            &function->instructions[destinationIndex],
            (size_t)(sourceIndex - destinationIndex) *
                    sizeof(*function->instructions));
    function->instructions[destinationIndex] = saved;

    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 oldStart = block->instructionRange.start;
        TZrUInt32 oldCount = block->instructionRange.count;
        TZrUInt32 oldEnd = oldStart + oldCount;
        TZrUInt32 newStart = UINT32_MAX;
        TZrUInt32 newEnd = 0u;
        TZrUInt32 oldIndex;
        if (!zr_licm_range_valid(block->instructionRange,
                                 function->instructionCount)) {
            zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                               function, block->id, 0u);
            return ZR_FALSE;
        }
        for (oldIndex = oldStart; oldIndex < oldEnd; ++oldIndex) {
            TZrUInt32 mapped;
            if (oldIndex == sourceIndex) continue;
            mapped = zr_licm_map_instruction_id(oldIndex + 1u, sourceIndex,
                                                destinationIndex) - 1u;
            if (mapped < newStart) newStart = mapped;
            if (mapped + 1u > newEnd) newEnd = mapped + 1u;
        }
        if (block->id == preheaderId) {
            if (destinationIndex < newStart) newStart = destinationIndex;
            if (destinationIndex + 1u > newEnd) newEnd = destinationIndex + 1u;
        }
        if (newStart == UINT32_MAX) {
            block->instructionRange.start = 0u;
            block->instructionRange.count = 0u;
        } else {
            block->instructionRange.start = newStart;
            block->instructionRange.count = newEnd - newStart;
        }
        block->terminatorInstructionId = zr_licm_map_instruction_id(
                block->terminatorInstructionId, sourceIndex, destinationIndex);
    }
    if (function->values != ZR_NULL) {
        TZrUInt32 valueIndex;
        for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
            function->values[valueIndex].definition = zr_licm_map_instruction_id(
                    function->values[valueIndex].definition,
                    sourceIndex, destinationIndex);
        }
    }
    if (function->sourceMaps != ZR_NULL) {
        TZrUInt32 mapIndex;
        for (mapIndex = 0u; mapIndex < function->sourceMapCount; ++mapIndex) {
            function->sourceMaps[mapIndex].instructionId = zr_licm_map_instruction_id(
                    function->sourceMaps[mapIndex].instructionId,
                    sourceIndex, destinationIndex);
        }
    }
    if (function->gcMap != ZR_NULL && function->gcMapCount != 0u) {
        TZrUInt32 mapIndex;
        for (mapIndex = 0u; mapIndex < function->gcMap->entryCount; ++mapIndex) {
            function->gcMap->entries[mapIndex].site = zr_licm_map_instruction_id(
                    function->gcMap->entries[mapIndex].site,
                    sourceIndex, destinationIndex);
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_licm_emit_remark(struct SZrExecIrRemarkSink *remarks,
                                   const SZrExecIrFunction *function,
                                   TZrExecIrInstructionId instructionId,
                                   EZrExecIrRemarkOutcome outcome,
                                   EZrExecIrLoopReason reason,
                                   SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOptimizationRemark remark;
    SZrExecIrOptimizationRemark *replacement;
    TZrUInt32 capacity;
    if (remarks == ZR_NULL) return ZR_TRUE;
    memset(&remark, 0, sizeof(remark));
    remark.pass = "licm";
    remark.sourceId = (function != ZR_NULL && instructionId != 0u &&
                       instructionId <= function->instructionCount)
                          ? function->instructions[instructionId - 1u].sourceId
                          : 0u;
    remark.outcome = outcome;
    remark.reasonCode = (TZrUInt32)reason + 100u;
    remark.beforeHash = 0u;
    remark.afterHash = 0u;
    if (remarks->count > remarks->capacity) {
        zr_licm_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, instructionId);
        return ZR_FALSE;
    }
    if (remarks->count == remarks->capacity) {
        capacity = remarks->capacity == 0u ? 8u : remarks->capacity;
        if (capacity > UINT32_MAX / 2u) {
            capacity = remarks->count + 1u;
        } else {
            capacity *= 2u;
        }
        if (capacity < remarks->count + 1u
#if SIZE_MAX < UINT32_MAX
            || capacity > (TZrUInt32)(SIZE_MAX / sizeof(*replacement))
#endif
            ) {
            zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               function, 0u, instructionId);
            return ZR_FALSE;
        }
        replacement = (SZrExecIrOptimizationRemark *)realloc(
                remarks->items, (size_t)capacity * sizeof(*replacement));
        if (replacement == ZR_NULL) {
            zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, 0u, instructionId);
            return ZR_FALSE;
        }
        remarks->items = replacement;
        remarks->capacity = capacity;
    }
    remarks->items[remarks->count++] = remark;
    return ZR_TRUE;
}

static TZrBool zr_licm_strength_identity(const SZrExecIrFunction *function,
                                         const SZrExecIrInstruction *instruction,
                                         TZrExecIrValueId *baseValue) {
    TZrUInt64 left;
    TZrUInt64 right;
    if (function == ZR_NULL || instruction == ZR_NULL || baseValue == ZR_NULL ||
        function->operands == ZR_NULL || instruction->operands.count != 2u ||
        instruction->results.count != 1u || instruction->flags != 0u ||
        instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
        instruction->effectIn != 0u || instruction->effectOut != 0u ||
        instruction->phiRange.count != 0u || instruction->successorRange.count != 0u ||
        instruction->deoptId != 0u || instruction->bindingRow != 0u) {
        return ZR_FALSE;
    }
    if (instruction->operands.start > function->operandCount ||
        instruction->operands.count > function->operandCount - instruction->operands.start)
        return ZR_FALSE;
    *baseValue = function->operands[instruction->operands.start];
    if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_MUL) {
        if (!zr_licm_value_is_constant(function,
                                       function->operands[instruction->operands.start + 1u],
                                       &right) || right != 1u)
            return ZR_FALSE;
        return ZR_TRUE;
    }
    if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD ||
        instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_SUB) {
        if (zr_licm_value_is_constant(function,
                                       function->operands[instruction->operands.start + 1u],
                                       &right) && right == 0u)
            return ZR_TRUE;
        if (instruction->opcode == (TZrUInt16)ZR_EXEC_IR_OPCODE_ADD &&
            zr_licm_value_is_constant(function,
                                       function->operands[instruction->operands.start],
                                       &left) && left == 0u) {
            *baseValue = function->operands[instruction->operands.start + 1u];
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_licm_apply_strength_identity(
        SZrExecIrFunction *function, const SZrExecIrLoopInfo *info,
        TZrUInt32 *reduced, struct SZrExecIrRemarkSink *remarks,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 loopIndex;
    if (reduced == ZR_NULL) return ZR_FALSE;
    *reduced = 0u;
    for (loopIndex = 0u; loopIndex < info->loopCount; ++loopIndex) {
        const SZrExecIrLoop *loop = &info->loops[loopIndex];
        const TZrExecIrBlockId *members = ZrParser_ExecIr_LoopMembers(info, loop);
        TZrUInt32 memberIndex;
        if (members == ZR_NULL || !loop->reducible || loop->zeroTrip) continue;
        for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
            const SZrExecIrBlock *block = &function->blocks[members[memberIndex] - 1u];
            TZrUInt32 instructionIndex;
            for (instructionIndex = block->instructionRange.start;
                 instructionIndex < block->instructionRange.start +
                                     block->instructionRange.count;
                 ++instructionIndex) {
                SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
                TZrExecIrValueId baseValue;
                SZrExecIrRange baseRange;
                if (!zr_licm_strength_identity(function, instruction, &baseValue)) continue;
                if (!ZrCore_ExecIr_FunctionAppendOperands(function, &baseValue, 1u,
                                                          &baseRange)) {
                    zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, block->id, instructionIndex + 1u);
                    return ZR_FALSE;
                }
                instruction->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_COPY;
                instruction->operands = baseRange;
                (*reduced)++;
                if (!zr_licm_emit_remark(remarks, function, instructionIndex + 1u,
                                         ZR_EXEC_IR_REMARK_SUCCESS,
                                         ZR_EXEC_IR_LOOP_REASON_NONE, diagnostic))
                    return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_licm_find_and_mark(const SZrExecIrFunction *function,
                                     const SZrExecIrLoopInfo *info,
                                     TZrExecIrInstructionId *instructionId,
                                     EZrExecIrLoopReason *reason,
                                     TZrUInt8 **invariantOut) {
    TZrUInt8 *invariant;
    TZrUInt32 round;
    TZrUInt32 loopIndex;
    if (instructionId == ZR_NULL || reason == ZR_NULL || invariantOut == ZR_NULL ||
        function == ZR_NULL || info == ZR_NULL) return ZR_FALSE;
    *instructionId = 0u;
    *reason = ZR_EXEC_IR_LOOP_REASON_NONE;
    *invariantOut = ZR_NULL;
    if (function->instructionCount == 0u) return ZR_FALSE;
    invariant = (TZrUInt8 *)calloc(function->instructionCount, sizeof(*invariant));
    if (invariant == ZR_NULL) return ZR_FALSE;
    /* Fixed point over pure instructions allows a chain of invariant values. */
    for (round = 0u; round < function->instructionCount + 1u; ++round) {
        TZrBool changed = ZR_FALSE;
        for (loopIndex = 0u; loopIndex < info->loopCount; ++loopIndex) {
            const SZrExecIrLoop *loop = &info->loops[loopIndex];
            const TZrExecIrBlockId *members = ZrParser_ExecIr_LoopMembers(info, loop);
            TZrUInt32 memberIndex;
            if (members == ZR_NULL) continue;
            for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
                const SZrExecIrBlock *block = &function->blocks[members[memberIndex] - 1u];
                TZrUInt32 instructionIndex;
                for (instructionIndex = block->instructionRange.start;
                     instructionIndex < block->instructionRange.start +
                                         block->instructionRange.count;
                     ++instructionIndex) {
                    const SZrExecIrInstruction *instruction =
                            &function->instructions[instructionIndex];
                    EZrExecIrLoopReason localReason;
                    TZrExecIrInstructionId ignored;
                    if (invariant[instructionIndex] != 0u) continue;
                    if (zr_licm_candidate(function, info, loop, invariant,
                                          &ignored, &localReason) &&
                        ignored == instructionIndex + 1u) {
                        invariant[instructionIndex] = 1u;
                        changed = ZR_TRUE;
                    }
                    (void)instruction;
                }
            }
        }
        if (!changed) break;
    }
    for (loopIndex = 0u; loopIndex < info->loopCount; ++loopIndex) {
        EZrExecIrLoopReason localReason;
        if (zr_licm_candidate(function, info, &info->loops[loopIndex], invariant,
                              instructionId, &localReason)) {
            *reason = localReason;
            *invariantOut = invariant;
            return ZR_TRUE;
        }
        if (*reason == ZR_EXEC_IR_LOOP_REASON_NONE) *reason = localReason;
    }
    free(invariant);
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_OptimizeLoopsEx(SZrExecIrFunction *function,
                                        SZrExecIrLoopInfo *info,
                                        struct SZrExecIrRemarkSink *remarks,
                                        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrLoopInfo working;
    TZrUInt32 rounds = 0u;
    TZrUInt32 hoisted = 0u;
    TZrUInt32 blocked = 0u;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || info == ZR_NULL ||
        !zr_licm_function_storage_valid(function, diagnostic)) return ZR_FALSE;
    if (function->sealed) {
        zr_licm_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_VerifyFunction(function,
                                      (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                              ZR_EXEC_IR_VERIFY_SSA),
                                      diagnostic)) return ZR_FALSE;
    ZrParser_ExecIr_LoopInfoInit(&working);
    if (!ZrParser_ExecIr_AnalyzeLoops(function, &working, diagnostic)) {
        ZrParser_ExecIr_LoopInfoFree(&working);
        return ZR_FALSE;
    }
    while (rounds++ < function->instructionCount + 1u) {
        TZrExecIrInstructionId instructionId = 0u;
        EZrExecIrLoopReason reason = ZR_EXEC_IR_LOOP_REASON_NONE;
        TZrUInt8 *invariant = ZR_NULL;
        TZrExecIrBlockId preheaderId = 0u;
        TZrUInt32 loopIndex;
        if (!zr_licm_find_and_mark(function, &working, &instructionId, &reason,
                                   &invariant)) {
            if (working.loopCount != 0u) blocked++;
            if (!zr_licm_emit_remark(remarks, function, 0u,
                                     ZR_EXEC_IR_REMARK_MISSED, reason,
                                     diagnostic)) {
                ZrParser_ExecIr_LoopInfoFree(&working);
                return ZR_FALSE;
            }
            break;
        }
        free(invariant);
        for (loopIndex = 0u; loopIndex < working.loopCount; ++loopIndex) {
            const SZrExecIrLoop *loop = &working.loops[loopIndex];
            const TZrExecIrBlockId *members = ZrParser_ExecIr_LoopMembers(&working, loop);
            TZrUInt32 memberIndex;
            if (members == ZR_NULL) continue;
            for (memberIndex = 0u; memberIndex < loop->memberCount; ++memberIndex) {
                const SZrExecIrBlock *block = &function->blocks[members[memberIndex] - 1u];
                if (instructionId - 1u >= block->instructionRange.start &&
                    instructionId - 1u < block->instructionRange.start +
                                              block->instructionRange.count) {
                    preheaderId = loop->preheaderBlockId;
                    break;
                }
            }
            if (preheaderId != 0u) break;
        }
        if (preheaderId == 0u ||
            !zr_licm_move_instruction(function, instructionId, preheaderId,
                                       diagnostic)) {
            blocked++;
            if (!zr_licm_emit_remark(remarks, function, instructionId,
                                     ZR_EXEC_IR_REMARK_MISSED,
                                     ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT,
                                     diagnostic)) {
                ZrParser_ExecIr_LoopInfoFree(&working);
                return ZR_FALSE;
            }
            break;
        }
        hoisted++;
        if (!zr_licm_emit_remark(remarks, function, instructionId,
                                 ZR_EXEC_IR_REMARK_SUCCESS,
                                 ZR_EXEC_IR_LOOP_REASON_NONE, diagnostic)) {
            ZrParser_ExecIr_LoopInfoFree(&working);
            return ZR_FALSE;
        }
        ZrParser_ExecIr_LoopInfoFree(&working);
        ZrParser_ExecIr_LoopInfoInit(&working);
        if (!ZrParser_ExecIr_AnalyzeLoops(function, &working, diagnostic)) {
            ZrParser_ExecIr_LoopInfoFree(&working);
            return ZR_FALSE;
        }
    }
    working.hoistedInstructionCount = hoisted;
    working.blockedInstructionCount = blocked;
    working.changed = (TZrBool)(hoisted != 0u);
    ZrParser_ExecIr_LoopInfoFree(info);
    *info = working;
    memset(&working, 0, sizeof(working));
    if (hoisted != 0u &&
        !ZrCore_ExecIr_VerifyFunction(function,
                                      (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                              ZR_EXEC_IR_VERIFY_SSA),
                                      diagnostic)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_OptimizeLoops(SZrExecIrFunction *function,
                                       SZrExecIrLoopInfo *info,
                                       SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_OptimizeLoopsEx(function, info, ZR_NULL, diagnostic);
}

TZrBool ZrParser_ExecIr_StrengthReduceEx(SZrExecIrFunction *function,
                                         SZrExecIrLoopInfo *info,
                                         struct SZrExecIrRemarkSink *remarks,
                                         SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrLoopInfo working;
    TZrUInt32 reduced = 0u;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || info == ZR_NULL ||
        !zr_licm_function_storage_valid(function, diagnostic) ||
        function->sealed) {
        zr_licm_diagnostic(diagnostic,
                           function != ZR_NULL && function->sealed
                               ? ZR_EXEC_IR_DIAGNOSTIC_SEALED
                               : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_VerifyFunction(function,
                                      (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                              ZR_EXEC_IR_VERIFY_SSA),
                                      diagnostic)) return ZR_FALSE;
    ZrParser_ExecIr_LoopInfoInit(&working);
    if (!ZrParser_ExecIr_AnalyzeLoops(function, &working, diagnostic) ||
        !zr_licm_apply_strength_identity(function, &working, &reduced,
                                          remarks, diagnostic)) {
        ZrParser_ExecIr_LoopInfoFree(&working);
        return ZR_FALSE;
    }
    working.strengthReducedCount = reduced;
    working.changed = (TZrBool)(reduced != 0u);
    ZrParser_ExecIr_LoopInfoFree(info);
    *info = working;
    memset(&working, 0, sizeof(working));
    if (reduced != 0u &&
        !ZrCore_ExecIr_VerifyFunction(function,
                                      (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                                              ZR_EXEC_IR_VERIFY_SSA),
                                      diagnostic)) return ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_StrengthReduce(SZrExecIrFunction *function,
                                       SZrExecIrLoopInfo *info,
                                       SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_StrengthReduceEx(function, info, ZR_NULL, diagnostic);
}

TZrBool ZrParser_ExecIr_RunLicm(SZrExecIrFunction *function,
                                SZrExecIrLoopInfo *info,
                                SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_OptimizeLoops(function, info, diagnostic);
}
