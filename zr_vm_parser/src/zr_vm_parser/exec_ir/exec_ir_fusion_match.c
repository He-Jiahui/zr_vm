#include "exec_ir_fusion_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/exec_ir_binding_facts.h"

const SZrExecIrInstruction *zr_fusion_instruction(
        const SZrExecIrFunction *function, TZrUInt32 instructionId) {
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount ||
        function->instructions == ZR_NULL) {
        return ZR_NULL;
    }
    return &function->instructions[instructionId - 1u];
}

static TZrBool zr_fusion_instruction_in_block(
        const SZrExecIrBlock *block, TZrUInt32 instructionId) {
    return (TZrBool)(block != ZR_NULL && instructionId != 0u &&
                     instructionId - 1u >= block->instructionRange.start &&
                     instructionId - 1u - block->instructionRange.start <
                             block->instructionRange.count);
}

static const SZrExecIrBlock *zr_fusion_block_for_instruction(
        const SZrExecIrFunction *function, TZrUInt32 instructionId) {
    TZrUInt32 index;
    if (function == ZR_NULL || function->blockCount == 0u) {
        return ZR_NULL;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        if (zr_fusion_instruction_in_block(&function->blocks[index],
                                           instructionId)) {
            return &function->blocks[index];
        }
    }
    return ZR_NULL;
}

static TZrUInt32 zr_fusion_state_boundary_mask(
        const SZrExecIrFunction *function, TZrUInt32 instructionId) {
    TZrUInt32 mask = ZR_EXEC_BC_FUSION_BOUNDARY_NONE;
    TZrUInt32 index;
    if (function == ZR_NULL || instructionId == 0u) {
        return mask;
    }
    if (function->stateMap != ZR_NULL &&
        function->stateMap->entryCount <= function->stateMap->entryCapacity &&
        (function->stateMap->entryCount == 0u ||
         function->stateMap->entries != ZR_NULL)) {
        for (index = 0u; index < function->stateMap->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry =
                    &function->stateMap->entries[index];
            TZrUInt32 flags;
            if (entry->instructionId != instructionId) {
                continue;
            }
            flags = entry->boundaryFlags;
            if ((flags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL) != 0u) {
                mask |= ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG;
            }
            if ((flags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC |
                          ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND |
                          ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE)) != 0u) {
                mask |= ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT;
            }
            if ((flags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW |
                          ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)) != 0u ||
                entry->exceptionState != 0u) {
                mask |= ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION;
            }
            if ((flags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT |
                          ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT)) != 0u) {
                mask |= ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT;
            }
        }
    }
    if (function->gcMap != ZR_NULL &&
        function->gcMap->entryCount <= function->gcMap->entryCapacity &&
        (function->gcMap->entryCount == 0u ||
         function->gcMap->entries != ZR_NULL)) {
        for (index = 0u; index < function->gcMap->entryCount; ++index) {
            if (function->gcMap->entries[index].site == instructionId) {
                mask |= ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT;
                break;
            }
        }
    }
    return mask;
}

static TZrUInt32 zr_fusion_instruction_boundary_mask(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrBlock *block,
        TZrUInt32 instructionId) {
    const SZrExecIrOpcodeInfo *info;
    TZrUInt32 mask = ZR_EXEC_BC_FUSION_BOUNDARY_NONE;
    if (instruction == ZR_NULL) {
        return ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT;
    }
    info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    if ((instruction->flags & ZR_EXEC_IR_FLAG_DEBUG_POLL) != 0u) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG;
    }
    if ((instruction->flags & (ZR_EXEC_IR_FLAG_MAY_GC |
                               ZR_EXEC_IR_FLAG_MAY_SUSPEND)) != 0u) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u ||
        (info != ZR_NULL &&
         (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u)) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_ALLOCATE) != 0u ||
        (info != ZR_NULL &&
         (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u)) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT;
    }
    /* A guard exit is an observable hand-off to a slow path.  Treat it as a
     * reentrant boundary even when the opcode schema itself is otherwise
     * pure; folding it away would make the deopt/resume point unreachable. */
    if ((instruction->flags & ZR_EXEC_IR_FLAG_GUARD_EXIT) != 0u) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT;
    }
    if (instruction->deoptId != 0u) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT;
    }
    if (block != ZR_NULL &&
        (block->flags & (ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION |
                         ZR_EXEC_IR_BLOCK_FLAG_CLEANUP)) != 0u) {
        mask |= ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION;
    }
    return mask | zr_fusion_state_boundary_mask(function, instructionId);
}

static TZrBool zr_fusion_operand_at(const SZrExecIrFunction *function,
                                    const SZrExecIrInstruction *instruction,
                                    TZrUInt32 index,
                                    TZrExecIrValueId *value) {
    if (value == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instruction->operands.count == ZR_EXEC_IR_VARIADIC ||
        index >= instruction->operands.count ||
        instruction->operands.start > function->operandCount ||
        index > function->operandCount - instruction->operands.start) {
        return ZR_FALSE;
    }
    *value = function->operands[instruction->operands.start + index];
    return ZR_TRUE;
}

static TZrBool zr_fusion_result_at(const SZrExecIrFunction *function,
                                   const SZrExecIrInstruction *instruction,
                                   TZrUInt32 index,
                                   TZrExecIrValueId *value) {
    if (value == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instruction->results.count == ZR_EXEC_IR_VARIADIC ||
        index >= instruction->results.count ||
        instruction->results.start > function->resultCount ||
        index > function->resultCount - instruction->results.start) {
        return ZR_FALSE;
    }
    *value = function->results[instruction->results.start + index];
    return ZR_TRUE;
}

static TZrUInt32 zr_fusion_use_count(const SZrExecIrFunction *function,
                                     TZrExecIrValueId value) {
    TZrUInt32 count = 0u;
    TZrUInt32 instructionIndex;
    if (function == ZR_NULL || value == ZR_EXEC_IR_VALUE_ID_INVALID) {
        return 0u;
    }
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        if (instruction->operands.count == ZR_EXEC_IR_VARIADIC) {
            /* A variadic use set is intentionally unknown to this bounded
             * analysis; treating it as non-single-use is conservative. */
            return UINT32_MAX;
        }
        TZrUInt32 operandIndex;
        for (operandIndex = 0u; operandIndex < instruction->operands.count;
             ++operandIndex) {
            TZrExecIrValueId operand;
            if (zr_fusion_operand_at(function, instruction, operandIndex,
                                     &operand) && operand == value) {
                if (count != UINT32_MAX) {
                    ++count;
                }
            }
        }
    }
    /* Def-use observations outside instruction operands also keep the
     * intermediate value live.  Treat each occurrence as a use so a fused
     * window cannot erase a phi input, GC root, or deopt reconstruction value
     * that a resume/collector path expects to see. */
    for (instructionIndex = 0u;
         instructionIndex < function->phiIncomingCount;
         ++instructionIndex) {
        if (function->phiIncoming[instructionIndex].value == value) {
            if (count != UINT32_MAX) {
                ++count;
            }
        }
    }
    for (instructionIndex = 0u;
         instructionIndex < function->gcRootCount;
         ++instructionIndex) {
        if (function->gcRoots[instructionIndex] == value) {
            if (count != UINT32_MAX) {
                ++count;
            }
        }
    }
    for (instructionIndex = 0u;
         instructionIndex < function->deoptValueCount;
         ++instructionIndex) {
        if (function->deoptValues[instructionIndex] == value) {
            if (count != UINT32_MAX) {
                ++count;
            }
        }
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        for (instructionIndex = 0u; instructionIndex < map->entryCount;
             ++instructionIndex) {
            const SZrExecIrStateMapEntry *entry = &map->entries[instructionIndex];
            TZrUInt32 poolIndex;
            for (poolIndex = 0u; poolIndex < entry->liveValues.count;
                 ++poolIndex) {
                if (map->valuePool[entry->liveValues.start + poolIndex] == value) {
                    if (count != UINT32_MAX) {
                        ++count;
                    }
                }
            }
            for (poolIndex = 0u; poolIndex < entry->rootValues.count;
                 ++poolIndex) {
                if (map->rootPool[entry->rootValues.start + poolIndex] == value) {
                    if (count != UINT32_MAX) {
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}

static TZrBool zr_fusion_tail_uses_value(const SZrExecIrFunction *function,
                                         const SZrExecIrInstruction *tail,
                                         TZrExecIrValueId value) {
    TZrUInt32 index;
    if (tail == ZR_NULL) {
        return ZR_FALSE;
    }
    if (tail->operands.count == ZR_EXEC_IR_VARIADIC) {
        return ZR_FALSE;
    }
    for (index = 0u; index < tail->operands.count; ++index) {
        TZrExecIrValueId operand;
        if (zr_fusion_operand_at(function, tail, index, &operand) &&
            operand == value) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_fusion_typed_instruction_is_compatible(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction) {
    TZrUInt32 index;
    TZrExecIrTypeToken declaredType;
    if (function == ZR_NULL || instruction == ZR_NULL ||
        instruction->operands.count == ZR_EXEC_IR_VARIADIC ||
        instruction->results.count == ZR_EXEC_IR_VARIADIC ||
        (instruction->operands.count == 0u &&
         instruction->results.count == 0u)) {
        return ZR_FALSE;
    }
    /* ExecIR operation type tokens describe the produced value when there is
     * one.  A place projection and its receiver/index operands are expected
     * to have different types, so requiring every operand to equal that
     * token would discard valid typed index/load and binding windows.  The
     * actual proof needed by fusion is that every referenced value has a
     * known type, plus an explicit instruction token (when present) agrees
     * with its result, or with its sole consumer operand for a no-result
     * instruction such as a branch. */
    declaredType = instruction->typeToken;
    for (index = 0u; index < instruction->operands.count; ++index) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_operand_at(function, instruction, index, &valueId) ||
            valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount ||
            function->values[valueId - 1u].typeToken == 0u) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < instruction->results.count; ++index) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_result_at(function, instruction, index, &valueId) ||
            valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount ||
            function->values[valueId - 1u].typeToken == 0u ||
            (declaredType != 0u &&
             function->values[valueId - 1u].typeToken != declaredType)) {
            return ZR_FALSE;
        }
    }
    if (declaredType != 0u && instruction->results.count == 0u) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_operand_at(function, instruction, 0u, &valueId) ||
            function->values[valueId - 1u].typeToken != declaredType) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* Memory tokens are a second dependency chain alongside effect tokens.  A
 * pure head may precede a memory-reading tail without manufacturing a token,
 * and a memory-writing head may precede a pure tail; those cases remain
 * representable in the fused handler.  When both sides name a memory state,
 * however, the state must be the exact same ordered witness. */
static TZrBool zr_fusion_memory_chain_is_compatible(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *head,
        const SZrExecIrInstruction *tail) {
    TZrUInt32 index;
    if (function == ZR_NULL || head == ZR_NULL || tail == ZR_NULL) {
        return ZR_FALSE;
    }
    if (head->memoryOut.count == 0u || tail->memoryIn.count == 0u) {
        return ZR_TRUE;
    }
    if (head->memoryOut.count != tail->memoryIn.count) {
        return ZR_FALSE;
    }
    for (index = 0u; index < head->memoryOut.count; ++index) {
        if (function->memoryTokenPool[head->memoryOut.start + index] !=
            function->memoryTokenPool[tail->memoryIn.start + index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_fusion_opcode_matches(const SZrExecBcFusionPatternInfo *info,
                                        const SZrExecIrInstruction *head,
                                        const SZrExecIrInstruction *tail) {
    if (info == ZR_NULL || head == ZR_NULL || tail == ZR_NULL ||
        (EZrExecIrOpcode)head->opcode != info->headOpcode) {
        return ZR_FALSE;
    }
    if ((EZrExecIrOpcode)tail->opcode == info->tailOpcode) {
        return ZR_TRUE;
    }
    return (info->constraints &
            ZR_EXEC_BC_FUSION_CONSTRAINT_INDEX_STORE_VARIANT) != 0u &&
           (EZrExecIrOpcode)tail->opcode == ZR_EXEC_IR_OPCODE_STORE;
}

/* ExecIR represents a conditional branch with two explicit CFG successors
 * (taken and fall-through), while an unconditional branch has one.  Keep the
 * distinction here instead of silently retaining only successor zero: the
 * latter would make a generated handler choose the wrong edge for one branch
 * outcome. */
static TZrBool zr_fusion_branch_shape_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *tail) {
    TZrUInt32 expected;
    TZrUInt32 index;
    if (function == ZR_NULL || tail == ZR_NULL ||
        !zr_fusion_range_valid(tail->successorRange,
                               function->successorCount)) {
        return ZR_FALSE;
    }
    if ((EZrExecIrOpcode)tail->opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
        expected = 2u;
    } else if ((EZrExecIrOpcode)tail->opcode == ZR_EXEC_IR_OPCODE_BRANCH) {
        expected = 1u;
    } else {
        /* No current schema fuses a switch; retaining this guard makes a
         * future BRANCH_TARGET row fail closed until its side-table shape is
         * explicitly described. */
        return ZR_FALSE;
    }
    if (tail->successorRange.count != expected ||
        tail->successorRange.count > ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS) {
        return ZR_FALSE;
    }
    for (index = 0u; index < tail->successorRange.count; ++index) {
        TZrExecIrBlockId target =
                function->successors[tail->successorRange.start + index];
        if (target == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            target > function->blockCount ||
            function->blocks[target - 1u].instructionRange.count == 0u) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* A zero binding row is the ExecIR producer's historical "not published"
 * marker, while the pointer-free static-binding facts table legitimately uses
 * row index zero for its first validated row.  Keep the default conservative:
 * zero becomes resolved only when an explicit facts witness proves that this
 * instruction owns row zero.  Non-zero rows retain the existing scalar
 * contract and the all-ones value remains the unresolved sentinel. */
static TZrBool zr_fusion_binding_facts_row_matches(
        const SZrExecIrBindingFacts *facts,
        TZrUInt32 rowIndex,
        TZrUInt32 instructionId) {
    const SZrExecIrBindingRow *row;
    TZrUInt32 index;
    if (facts == ZR_NULL || rowIndex >= facts->rowCount ||
        facts->rows == ZR_NULL || instructionId == 0u) {
        return ZR_FALSE;
    }
    row = &facts->rows[rowIndex];
    if (row->rowIndex != rowIndex) {
        return ZR_FALSE;
    }
    if (row->instructionId == instructionId) {
        return ZR_TRUE;
    }
    if (row->instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
        row->segmentIndex == ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE ||
        row->segmentIndex >= facts->segmentCount || facts->segments == ZR_NULL) {
        return ZR_FALSE;
    }
    if (facts->segments[row->segmentIndex].instructionId == instructionId) {
        return ZR_TRUE;
    }
    /* A producer may carry the row association on the segment rather than on
     * the row.  This fallback mirrors the facts validator's lookup rule while
     * remaining bounded by the validated segment count. */
    for (index = 0u; index < facts->segmentCount; ++index) {
        if (facts->segments[index].bindingRow == rowIndex &&
            facts->segments[index].instructionId == instructionId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_fusion_binding_row_resolved(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        const SZrExecIrInstruction *instruction,
        TZrUInt32 instructionId,
        TZrUInt32 row) {
    if (row == ZR_CALL_BINDING_SLOT_NONE) {
        return ZR_FALSE;
    }
    if (row != 0u) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || instruction == ZR_NULL || options == ZR_NULL ||
        options->bindingFacts == ZR_NULL) {
        return ZR_FALSE;
    }
    return zr_fusion_binding_facts_row_matches(options->bindingFacts, 0u,
                                                instructionId);
}

static TZrUInt32 zr_fusion_pair_binding_row(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        const SZrExecIrInstruction *head,
        TZrUInt32 headId,
        const SZrExecIrInstruction *tail,
        TZrUInt32 tailId) {
    if (tail != ZR_NULL &&
        zr_fusion_binding_row_resolved(function, options, tail, tailId,
                                       tail->bindingRow)) {
        return tail->bindingRow;
    }
    if (head != ZR_NULL &&
        zr_fusion_binding_row_resolved(function, options, head, headId,
                                       head->bindingRow)) {
        return head->bindingRow;
    }
    return ZR_CALL_BINDING_SLOT_NONE;
}

static EZrExecBcFusionFallbackReason zr_fusion_match_constraints(
        const SZrExecIrFunction *function,
        const SZrExecBcFusionPatternInfo *info,
        TZrUInt32 headId,
        TZrUInt32 tailId,
        const SZrExecBcPatternOptions *options) {
    const SZrExecIrInstruction *head = zr_fusion_instruction(function, headId);
    const SZrExecIrInstruction *tail = zr_fusion_instruction(function, tailId);
    const SZrExecIrBlock *headBlock =
            zr_fusion_block_for_instruction(function, headId);
    const SZrExecIrBlock *tailBlock =
            zr_fusion_block_for_instruction(function, tailId);
    TZrExecIrValueId headResult = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrUInt32 headBoundary;
    TZrUInt32 tailBoundary;

    if (info == ZR_NULL || head == ZR_NULL || tail == ZR_NULL ||
        !zr_fusion_opcode_matches(info, head, tail)) {
        return ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN;
    }
    if (headBlock == ZR_NULL || tailBlock == ZR_NULL ||
        headBlock != tailBlock) {
        return ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN;
    }
    if (headId == UINT32_MAX || tailId != headId + 1u) {
        return ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN;
    }
    headBoundary = zr_fusion_instruction_boundary_mask(function, head,
                                                        headBlock, headId);
    tailBoundary = zr_fusion_instruction_boundary_mask(function, tail,
                                                        tailBlock, tailId);
    {
        const TZrUInt32 boundary = headBoundary | tailBoundary;
        const TZrUInt32 represented = info->boundaryMask;
        const TZrBool preserve = (TZrBool)((options == ZR_NULL ||
                options->preserveObservableBoundaries) &&
                (info->constraints &
                 ZR_EXEC_BC_FUSION_CONSTRAINT_PRESERVE_BOUNDARY) != 0u);
        /* A rule may preserve a boundary only when its generated contract
         * explicitly names that boundary.  This prevents, for example, an
         * index/load rule from silently absorbing a suspend or guard exit. */
        if (boundary != ZR_EXEC_BC_FUSION_BOUNDARY_NONE &&
            (!preserve || (boundary & ~represented) != 0u)) {
            if ((boundary & ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION) != 0u) {
                return ZR_EXEC_BC_FUSION_FALLBACK_EXCEPTION_BOUNDARY;
            }
            if ((boundary & ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT) != 0u) {
                return ZR_EXEC_BC_FUSION_FALLBACK_REENTRANT_BOUNDARY;
            }
            if ((boundary & ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG) != 0u) {
                return ZR_EXEC_BC_FUSION_FALLBACK_DEBUG_BOUNDARY;
            }
            return ZR_EXEC_BC_FUSION_FALLBACK_SAFEPOINT;
        }
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_TYPED_OPERANDS) != 0u) {
        if (!zr_fusion_typed_instruction_is_compatible(function, head) ||
            !zr_fusion_typed_instruction_is_compatible(function, tail)) {
            return ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH;
        }
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_NO_INTERVENING_EFFECT) != 0u) {
        if ((head->effectOut != 0u || tail->effectIn != 0u) &&
            head->effectOut != tail->effectIn) {
            return ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH;
        }
        if (!zr_fusion_memory_chain_is_compatible(function, head, tail)) {
            return ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH;
        }
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_SAME_RESULT_OPERAND) != 0u) {
        if (head->results.count != 1u ||
            !zr_fusion_result_at(function, head, 0u, &headResult) ||
            !zr_fusion_tail_uses_value(function, tail, headResult)) {
            return ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH;
        }
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_RESULT_SINGLE_USE) != 0u &&
        (headResult == ZR_EXEC_IR_VALUE_ID_INVALID ||
         zr_fusion_use_count(function, headResult) != 1u)) {
        return ZR_EXEC_BC_FUSION_FALLBACK_MULTIPLE_USE;
    }
    if (info->resultForm == ZR_EXEC_BC_FUSION_RESULT_OF_TAIL &&
        (EZrExecIrOpcode)tail->opcode != ZR_EXEC_IR_OPCODE_STORE &&
        tail->results.count != 1u) {
        return ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH;
    }
    if (info->resultForm == ZR_EXEC_BC_FUSION_RESULT_OF_HEAD &&
        head->results.count != 1u) {
        return ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH;
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_LAYOUT_PROVEN) != 0u &&
        (head->layoutId == 0u || head->layoutId == UINT32_MAX ||
         (tail->layoutId != 0u && tail->layoutId != head->layoutId))) {
        return ZR_EXEC_BC_FUSION_FALLBACK_LAYOUT_UNKNOWN;
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_BINDING_RESOLVED) != 0u &&
        zr_fusion_pair_binding_row(function, options, head, headId, tail,
                                    tailId) == ZR_CALL_BINDING_SLOT_NONE &&
        !(options != ZR_NULL && options->allowUnresolvedBinding)) {
        return ZR_EXEC_BC_FUSION_FALLBACK_BINDING_MISSING;
    }
    if ((info->constraints &
         ZR_EXEC_BC_FUSION_CONSTRAINT_BRANCH_TARGET) != 0u &&
        !zr_fusion_branch_shape_valid(function, tail)) {
        return ZR_EXEC_BC_FUSION_FALLBACK_BRANCH_TARGET;
    }
    return ZR_EXEC_BC_FUSION_FALLBACK_NONE;
}

void zr_fusion_diag_set_rich(SZrExecBcFusionDiagnostic *diagnostic,
                                    EZrExecBcFusionStatus status,
                                    EZrExecBcFusionFallbackReason reason,
                                    EZrExecBcFusionPattern pattern,
                                    TZrUInt32 head,
                                    TZrUInt32 tail,
                                    TZrUInt32 source,
                                    TZrUInt32 expected,
                                    TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->status = status;
    diagnostic->fallbackReason = reason;
    diagnostic->pattern = pattern;
    diagnostic->headInstructionId = head;
    diagnostic->tailInstructionId = tail;
    diagnostic->sourceId = source;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

TZrBool zr_fusion_reserve(void **storage,
                                 TZrUInt32 *capacity,
                                 TZrUInt32 requested,
                                 size_t elementSize) {
    TZrUInt32 newCapacity;
    void *resized;
    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        (size_t)requested > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    if (requested <= *capacity) {
        return ZR_TRUE;
    }
    newCapacity = *capacity == 0u ? 4u : *capacity;
    while (newCapacity < requested) {
        if (newCapacity > UINT32_MAX / 2u) {
            newCapacity = requested;
            break;
        }
        newCapacity *= 2u;
    }
    if ((size_t)newCapacity > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    resized = realloc(*storage, (size_t)newCapacity * elementSize);
    if (resized == ZR_NULL) {
        return ZR_FALSE;
    }
    *storage = resized;
    *capacity = newCapacity;
    return ZR_TRUE;
}

TZrBool zr_fusion_append_instruction(SZrExecBcFusionPlan *plan,
                                            SZrExecBcFusionInstruction word) {
    if (plan == ZR_NULL ||
        plan->instructionCount == UINT32_MAX ||
        !zr_fusion_reserve((void **)&plan->instructions,
                           &plan->instructionCapacity,
                           plan->instructionCount + 1u,
                           sizeof(*plan->instructions))) {
        return ZR_FALSE;
    }
    plan->instructions[plan->instructionCount++] = word;
    return ZR_TRUE;
}

TZrBool zr_fusion_append_side(SZrExecBcFusionPlan *plan,
                                     const SZrExecBcFusionSideEntry *entry) {
    if (plan == ZR_NULL || entry == ZR_NULL ||
        plan->sideEntryCount == UINT32_MAX ||
        !zr_fusion_reserve((void **)&plan->sideEntries,
                           &plan->sideEntryCapacity,
                           plan->sideEntryCount + 1u,
                           sizeof(*plan->sideEntries))) {
        return ZR_FALSE;
    }
    plan->sideEntries[plan->sideEntryCount++] = *entry;
    return ZR_TRUE;
}

TZrBool zr_fusion_append_source(SZrExecBcFusionPlan *plan,
                                       const SZrExecBcFusionSourceMap *map) {
    if (plan == ZR_NULL || map == ZR_NULL ||
        plan->sourceMapCount == UINT32_MAX ||
        !zr_fusion_reserve((void **)&plan->sourceMaps,
                           &plan->sourceMapCapacity,
                           plan->sourceMapCount + 1u,
                           sizeof(*plan->sourceMaps))) {
        return ZR_FALSE;
    }
    plan->sourceMaps[plan->sourceMapCount++] = *map;
    return ZR_TRUE;
}

TZrBool zr_fusion_append_fallback(SZrExecBcFusionPlan *plan,
                                         const SZrExecBcFusionFallback *fallback,
                                         TZrUInt32 maxEntries) {
    if (plan == ZR_NULL || fallback == ZR_NULL ||
        plan->fallbackCount >= maxEntries) {
        return ZR_TRUE;
    }
    if (plan->fallbackCount == UINT32_MAX ||
        !zr_fusion_reserve((void **)&plan->fallbacks,
                           &plan->fallbackCapacity,
                           plan->fallbackCount + 1u,
                           sizeof(*plan->fallbacks))) {
        return ZR_FALSE;
    }
    plan->fallbacks[plan->fallbackCount++] = *fallback;
    return ZR_TRUE;
}

SZrExecBcFusionInstruction zr_fusion_encode_original(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction) {
    SZrExecBcFusionInstruction word;
    TZrExecIrValueId value = 0u;
    memset(&word, 0, sizeof(word));
    word.operationCode = instruction != ZR_NULL ? instruction->opcode : 0u;
    if (instruction != ZR_NULL && instruction->operands.count != 0u) {
        (void)zr_fusion_operand_at(function, instruction, 0u, &value);
    } else if (instruction != ZR_NULL && instruction->results.count != 0u) {
        (void)zr_fusion_result_at(function, instruction, 0u, &value);
    }
    memcpy(word.operand.operand0, &value, sizeof(value));
    return word;
}

SZrExecBcFusionInstruction zr_fusion_encode_fused(
        EZrExecBcFusionPattern pattern, TZrUInt32 sideIndex) {
    SZrExecBcFusionInstruction word;
    TZrUInt32 opcode = (TZrUInt32)ZR_EXEC_BC_FUSION_OPCODE(pattern);
    memset(&word, 0, sizeof(word));
    /* The public header statically proves base + generated count fits u16. */
    word.operationCode = (TZrUInt16)opcode;
    word.operandExtra = sideIndex <= UINT16_MAX ? (TZrUInt16)sideIndex : 0u;
    word.operand.operand2[0] = (TZrInt32)sideIndex;
    return word;
}

static TZrUInt32 zr_fusion_side_operand_count(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *head,
        const SZrExecIrInstruction *tail,
        TZrUInt32 *operands,
        TZrUInt32 capacity) {
    TZrUInt32 count = 0u;
    const SZrExecIrInstruction *instructions[2] = {head, tail};
    TZrUInt32 which;
    if (operands == ZR_NULL || capacity == 0u) {
        return UINT32_MAX;
    }
    for (which = 0u; which < 2u; ++which) {
        TZrUInt32 index;
        const SZrExecIrInstruction *instruction = instructions[which];
        if (instruction == ZR_NULL ||
            instruction->operands.count == ZR_EXEC_IR_VARIADIC) {
            /* A variadic operand pool has no bounded enumeration contract.
             * Do not turn UINT32_MAX into a long-running loop; the caller
             * will retain this window unfused with OPERAND_OVERFLOW. */
            return UINT32_MAX;
        }
        for (index = 0u; index < instruction->operands.count; ++index) {
            TZrExecIrValueId value;
            if (!zr_fusion_operand_at(function, instruction, index, &value)) {
                return UINT32_MAX;
            }
            if (count >= capacity) {
                return UINT32_MAX;
            }
            operands[count++] = value;
        }
    }
    return count;
}

TZrUInt32 zr_fusion_find_output_pc(const TZrUInt32 *instructionToPc,
                                          TZrUInt32 count,
                                          TZrUInt32 instructionId) {
    if (instructionToPc == ZR_NULL || instructionId == 0u ||
        instructionId > count) {
        return ZR_EXEC_BC_FUSION_INVALID_INDEX;
    }
    return instructionToPc[instructionId];
}

static EZrExecBcFusionPattern zr_fusion_pattern_for_index(TZrUInt32 index) {
    return (EZrExecBcFusionPattern)index;
}

static TZrBool zr_fusion_pattern_enabled(const SZrExecBcPatternOptions *options,
                                         EZrExecBcFusionPattern pattern) {
    if (options == ZR_NULL || options->enabledPatternMask == 0u) {
        return ZR_TRUE;
    }
    return (TZrBool)(((options->enabledPatternMask &
                      ((TZrUInt32)1u << (TZrUInt32)pattern)) != 0u));
}

static TZrUInt32 zr_fusion_resume_id(const SZrExecIrFunction *function,
                                     TZrUInt32 instructionId) {
    const SZrExecIrInstruction *instruction;
    TZrUInt32 index;
    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount) {
        return instructionId;
    }
    instruction = &function->instructions[instructionId - 1u];
    if (instruction->deoptId != 0u &&
        function->deoptStates != ZR_NULL) {
        for (index = 0u; index < function->deoptStateCount; ++index) {
            const SZrExecIrDeoptState *state = &function->deoptStates[index];
            if (state->id == instruction->deoptId && state->resumeId != 0u) {
                return state->resumeId;
            }
        }
    }
    if (function->stateMap != ZR_NULL && function->stateMap->entries != ZR_NULL) {
        for (index = 0u; index < function->stateMap->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry =
                    &function->stateMap->entries[index];
            if (entry->instructionId == instructionId && entry->resumeId != 0u) {
                return entry->resumeId;
            }
        }
    }
    return instructionId;
}

EZrExecBcFusionFallbackReason zr_fusion_reason_for_pair(
        const SZrExecIrFunction *function,
        TZrUInt32 headId,
        TZrUInt32 tailId,
        const SZrExecBcPatternOptions *options,
        EZrExecBcFusionPattern *matchedPattern) {
    EZrExecBcFusionFallbackReason firstReason =
            ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN;
    TZrUInt32 index;
    if (matchedPattern != ZR_NULL) {
        *matchedPattern = ZR_EXEC_BC_FUSION_PATTERN_COUNT;
    }
    for (index = 0u; index < ZR_EXEC_BC_FUSION_PATTERN_COUNT; ++index) {
        const SZrExecBcFusionPatternInfo *info = ZrParser_ExecBcFusion_PatternInfo((EZrExecBcFusionPattern)index);
        EZrExecBcFusionFallbackReason reason;
        if (!zr_fusion_opcode_matches(
                    info, zr_fusion_instruction(function, headId),
                    zr_fusion_instruction(function, tailId))) {
            continue;
        }
        if (matchedPattern != ZR_NULL &&
            *matchedPattern == ZR_EXEC_BC_FUSION_PATTERN_COUNT) {
            /* Keep the first schema row that recognized the opcode pair in
             * the fallback record, even when a later predicate rejects it. */
            *matchedPattern = zr_fusion_pattern_for_index(index);
        }
        if (!zr_fusion_pattern_enabled(options,
                                       zr_fusion_pattern_for_index(index))) {
            if (firstReason == ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN) {
                firstReason = ZR_EXEC_BC_FUSION_FALLBACK_DISABLED;
            }
            continue;
        }
        reason = zr_fusion_match_constraints(function, info, headId, tailId,
                                             options);
        if (reason == ZR_EXEC_BC_FUSION_FALLBACK_NONE) {
            if (matchedPattern != ZR_NULL) {
                *matchedPattern = zr_fusion_pattern_for_index(index);
            }
            return ZR_EXEC_BC_FUSION_FALLBACK_NONE;
        }
        if (firstReason == ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN) {
            firstReason = reason;
        }
    }
    return firstReason;
}

void zr_fusion_set_contract_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecIrFunction *function,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    zr_fusion_diag_set(diagnostic, code, function, 0u, 0u, 0u, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
}

TZrBool zr_fusion_options_valid(const SZrExecBcPatternOptions *options) {
    TZrUInt32 knownPatternMask;
    if (options == ZR_NULL) {
        return ZR_TRUE;
    }
    if (options->schemaVersion != 0u &&
        options->schemaVersion != ZR_EXEC_BC_FUSION_SCHEMA_VERSION) {
        return ZR_FALSE;
    }
    knownPatternMask = ZR_EXEC_BC_FUSION_PATTERN_COUNT >= 32u
                           ? UINT32_MAX
                           : ((TZrUInt32)1u << ZR_EXEC_BC_FUSION_PATTERN_COUNT) - 1u;
    if ((options->enabledPatternMask & ~knownPatternMask) != 0u ||
        options->maxSideTableEntries > UINT16_MAX || options->reserved != 0u ||
        options->preserveObservableBoundaries > ZR_TRUE ||
        options->requireSealed > ZR_TRUE ||
        options->allowUnresolvedBinding > ZR_TRUE) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrUInt32 zr_fusion_option_or_default(TZrUInt32 value,
                                             TZrUInt32 defaultValue) {
    return value == 0u ? defaultValue : value;
}

TZrBool zr_fusion_budget_allows(const SZrExecBcFusionPlan *plan,
                                       const SZrExecBcPatternOptions *options,
                                       const SZrExecBcFusionPatternInfo *info) {
    TZrUInt32 maxFused;
    TZrUInt32 maxSide;
    TZrUInt32 minBenefit;
    TZrUInt32 maxCode;
    if (plan == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }
    maxFused = options != ZR_NULL
                   ? zr_fusion_option_or_default(options->maxFusedCount,
                                                 UINT32_MAX)
                   : UINT32_MAX;
    maxSide = options != ZR_NULL
                  ? zr_fusion_option_or_default(options->maxSideTableEntries,
                                                UINT16_MAX)
                  : UINT16_MAX;
    minBenefit = options != ZR_NULL
                     ? zr_fusion_option_or_default(options->minDispatchBenefit,
                                                   1u)
                     : 1u;
    maxCode = options != ZR_NULL ? options->maxCodeBytes : 0u;
    if (plan->fusedCount >= maxFused || plan->sideEntryCount >= maxSide ||
        plan->sideEntryCount >= UINT16_MAX ||
        info->dispatchBenefit < minBenefit) {
        return ZR_FALSE;
    }
    if (maxCode != 0u &&
        (plan->estimatedCodeBytes > maxCode ||
         info->codeCostBytes > maxCode - plan->estimatedCodeBytes)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool zr_fusion_append_source_event(
        SZrExecBcFusionPlan *plan,
        TZrUInt32 fusedPc,
        TZrUInt32 instructionId,
        const SZrExecIrInstruction *instruction,
        TZrUInt32 ordinal,
        const SZrExecIrFunction *function) {
    SZrExecBcFusionSourceMap map;
    memset(&map, 0, sizeof(map));
    map.fusedPc = fusedPc;
    map.originalInstructionId = instructionId;
    map.sourceId = instruction != ZR_NULL ? instruction->sourceId : 0u;
    map.resumeId = zr_fusion_resume_id(function, instructionId);
    map.ordinal = ordinal;
    map.boundaryMask = zr_fusion_instruction_boundary_mask(
            function, instruction,
            zr_fusion_block_for_instruction(function, instructionId),
            instructionId);
    return zr_fusion_append_source(plan, &map);
}

TZrBool zr_fusion_fill_side_entry(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        EZrExecBcFusionPattern pattern,
        TZrUInt32 headId,
        TZrUInt32 tailId,
        SZrExecBcFusionSideEntry *entry) {
    const SZrExecIrInstruction *head = zr_fusion_instruction(function, headId);
    const SZrExecIrInstruction *tail = zr_fusion_instruction(function, tailId);
    TZrUInt32 operandCount;
    TZrUInt32 index;
    const SZrExecBcFusionPatternInfo *info;
    if (entry == ZR_NULL || head == ZR_NULL || tail == ZR_NULL) {
        return ZR_FALSE;
    }
    info = ZrParser_ExecBcFusion_PatternInfo(pattern);
    if (info == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(entry, 0, sizeof(*entry));
    entry->pattern = pattern;
    entry->headOpcode = (EZrExecIrOpcode)head->opcode;
    entry->tailOpcode = (EZrExecIrOpcode)tail->opcode;
    entry->headInstructionId = headId;
    entry->tailInstructionId = tailId;
    entry->headSourceId = head->sourceId;
    entry->tailSourceId = tail->sourceId;
    entry->headResumeId = zr_fusion_resume_id(function, headId);
    entry->tailResumeId = zr_fusion_resume_id(function, tailId);
    entry->headResult = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    entry->tailResult = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    if (head->results.count == 1u &&
        !zr_fusion_result_at(function, head, 0u, &entry->headResult)) {
        return ZR_FALSE;
    }
    if (tail->results.count == 1u &&
        !zr_fusion_result_at(function, tail, 0u, &entry->tailResult)) {
        return ZR_FALSE;
    }
    operandCount = zr_fusion_side_operand_count(
            function, head, tail, entry->operands,
            ZR_EXEC_BC_FUSION_MAX_OPERANDS);
    if (operandCount == UINT32_MAX) {
        return ZR_FALSE;
    }
    entry->operandCount = operandCount;
    entry->headOperandCount = head->operands.count;
    entry->tailOperandCount = tail->operands.count;
    entry->branchTarget = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    entry->branchTargetPc = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    for (index = 0u; index < ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS; ++index) {
        entry->branchTargets[index] = ZR_EXEC_BC_FUSION_INVALID_INDEX;
        entry->branchTargetPcs[index] = ZR_EXEC_BC_FUSION_INVALID_INDEX;
    }
    if ((info->constraints & ZR_EXEC_BC_FUSION_CONSTRAINT_BRANCH_TARGET) != 0u) {
        if (!zr_fusion_branch_shape_valid(function, tail)) {
            return ZR_FALSE;
        }
        entry->branchTargetCount = tail->successorRange.count;
        for (index = 0u; index < entry->branchTargetCount; ++index) {
            entry->branchTargets[index] = function->successors[
                    tail->successorRange.start + index];
        }
        entry->branchTarget = entry->branchTargets[0];
    }
    entry->bindingRow = zr_fusion_pair_binding_row(function, options, head,
                                                   headId, tail, tailId);
    entry->guardMask =
            zr_fusion_instruction_boundary_mask(
                    function, head,
                    zr_fusion_block_for_instruction(function, headId), headId) |
            zr_fusion_instruction_boundary_mask(
                    function, tail,
                    zr_fusion_block_for_instruction(function, tailId), tailId);
    entry->generation = function->contract.generation;
    entry->signatureHash = function->contract.signatureHash != 0u
                               ? function->contract.signatureHash
                               : function->signatureHash;
    entry->moduleHash = function->contract.moduleHash;
    entry->layoutHash = function->contract.layoutHash;
    for (index = operandCount; index < ZR_EXEC_BC_FUSION_MAX_OPERANDS; ++index) {
        entry->operands[index] = 0u;
    }
    return ZR_TRUE;
}

TZrUInt32 zr_fusion_block_target_instruction(
        const SZrExecIrFunction *function, TZrUInt32 blockId) {
    const SZrExecIrBlock *block;
    if (function == ZR_NULL || blockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        blockId > function->blockCount || function->blocks == ZR_NULL) {
        return blockId;
    }
    block = &function->blocks[blockId - 1u];
    return block->instructionRange.count == 0u
               ? ZR_EXEC_BC_FUSION_INVALID_INDEX
               : block->instructionRange.start + 1u;
}
