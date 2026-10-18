#include "exec_ir/exec_ir_pass_internal.h"
#include "exec_ir/exec_ir_gc_map_internal.h"

#include "zr_vm_core/exec_ir_state_map.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static TZrBool zr_dce_size_mul_overflow(TZrUInt64 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     count > (TZrUInt64)(SIZE_MAX / elementSize));
}

static SZrExecIrRange zr_dce_empty_range(void) {
    SZrExecIrRange range;
    range.offset = 0u;
    range.count = 0u;
    return range;
}

static TZrBool zr_dce_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return ZrParser_ExecIr_PassRangeValid(range, count);
}

static TZrBool zr_dce_storage_valid(const SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (function == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->sourceMapCount > function->sourceMapCapacity ||
        !zr_exec_ir_gc_map_outer_shape_is_valid(function) ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->sourceMapCount != 0u && function->sourceMaps == ZR_NULL) ||
        (function->gcMap != ZR_NULL && function->gcMap->entryCount != 0u &&
         function->gcMap->entries == ZR_NULL)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                       ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 region;
        if (!zr_dce_range_valid(block->phis, function->phiCount) ||
            !zr_dce_range_valid(block->effectPhiIncomings, function->phiIncomingCount) ||
            ((block->effectPhiResult == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID) !=
             (block->effectPhiIncomings.count == 0u))) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, block->id, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (!zr_dce_range_valid(block->memoryPhiIncomings[region],
                                     function->phiIncomingCount) ||
                ((block->memoryPhiResults[region] == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) !=
                 (block->memoryPhiIncomings[region].count == 0u))) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                               function, block->id, 0u, 0u, 0u);
                return ZR_FALSE;
            }
        }
        if (function->blocks[index].id != index + 1u) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                           function, function->blocks[index].id, 0u,
                                           index + 1u, function->blocks[index].id);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
        if (info == ZR_NULL) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                           function, 0u, index + 1u,
                                           ZR_EXEC_IR_OPCODE_COUNT - 1u, instruction->opcode);
            return ZR_FALSE;
        }
        if (!zr_dce_range_valid(instruction->results, function->resultCount) ||
            !zr_dce_range_valid(instruction->operands, function->operandCount) ||
            !zr_dce_range_valid(instruction->phiRange, function->phiIncomingCount) ||
            !zr_dce_range_valid(instruction->successorRange, function->successorCount) ||
            !zr_dce_range_valid(instruction->memoryIn, function->memoryTokenCount) ||
            !zr_dce_range_valid(instruction->memoryOut, function->memoryTokenCount)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, 0u, index + 1u, 0u, 0u);
            return ZR_FALSE;
        }
        if (instruction->operands.count < info->minimumOperands ||
            instruction->operands.count > info->maximumOperands ||
            instruction->results.count != info->resultArity) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, 0u, index + 1u,
                                           info->minimumOperands, instruction->operands.count);
            return ZR_FALSE;
        }
        if ((instruction->results.count != 0u && function->results == ZR_NULL) ||
            (instruction->operands.count != 0u && function->operands == ZR_NULL) ||
            (instruction->successorRange.count != 0u && function->successors == ZR_NULL) ||
            (instruction->phiRange.count != 0u && function->phiIncoming == ZR_NULL)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                           ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                           function, 0u, index + 1u, 0u, 0u);
            return ZR_FALSE;
        }
        for (TZrUInt32 at = instruction->operands.start;
             at < instruction->operands.start + instruction->operands.count; ++at) {
            if (function->operands[at] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                function->operands[at] > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, 0u, index + 1u,
                                               function->valueCount, function->operands[at]);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 at = instruction->results.start;
             at < instruction->results.start + instruction->results.count; ++at) {
            if (function->results[at] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                function->results[at] > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, 0u, index + 1u,
                                               function->valueCount, function->results[at]);
                return ZR_FALSE;
            }
        }
        /* Retained legacy opcode PHIs carry ordinary values, even if the
         * same backing row is also declared by a token consumer. */
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_PHI) {
            for (TZrUInt32 at = instruction->phiRange.start;
                 at < instruction->phiRange.start + instruction->phiRange.count; ++at) {
                TZrExecIrValueId value = function->phiIncoming[at].value;
                if (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > function->valueCount) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, index + 1u,
                                                   function->valueCount, value);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (index = 0u; index < function->phiCount; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID || phi->result > function->valueCount ||
            !zr_dce_range_valid(phi->incomings, function->phiIncomingCount)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, 0u, 0u, function->valueCount, phi->result);
            return ZR_FALSE;
        }
        for (TZrUInt32 at = phi->incomings.start;
             at < phi->incomings.start + phi->incomings.count; ++at) {
            TZrExecIrValueId value = function->phiIncoming[at].value;
            if (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, 0u, 0u, function->valueCount, value);
                return ZR_FALSE;
            }
        }
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[index];
        if (incoming->predecessor == 0u || incoming->predecessor > function->blockCount) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, incoming->predecessor, 0u,
                                           function->valueCount, incoming->value);
            return ZR_FALSE;
        }
    }
    if (function->gcMap != ZR_NULL) {
        for (index = 0u; index < function->gcMap->entryCount; ++index) {
            const SZrExecIrGcMapEntry *entry = &function->gcMap->entries[index];
            if (!zr_dce_range_valid(entry->liveRefSlots,
                                    function->gcMap->slotIndexCount) ||
                !zr_dce_range_valid(entry->inlineRefOffsets,
                                    function->gcMap->inlineRefOffsetCount)) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                               function, 0u, entry->site, 0u, 0u);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_dce_mark_value(TZrUInt8 *used, TZrUInt32 valueCount,
                                 TZrExecIrValueId value) {
    if (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > valueCount) return ZR_FALSE;
    used[value] = 1u;
    return ZR_TRUE;
}

static TZrBool zr_dce_pure(EZrExecIrOpcode opcode) {
    switch (opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_TYPE_TEST:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_NEG:
        case ZR_EXEC_IR_OPCODE_COMPARE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_dce_boundary_observable(const SZrExecIrFunction *function,
                                          TZrExecIrInstructionId instructionId) {
    TZrUInt32 index;
    if (function->gcMap != ZR_NULL) {
        for (index = 0u; index < function->gcMap->entryCount; ++index) {
            if (function->gcMap->entries[index].site == instructionId) return ZR_TRUE;
        }
    }
    if (function->stateMap != ZR_NULL) {
        for (index = 0u; index < function->stateMap->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry = &function->stateMap->entries[index];
            if (entry->instructionId == instructionId &&
                (entry->boundaryFlags != 0u || entry->deoptId != 0u ||
                 entry->resumeId != 0u || entry->liveValues.count != 0u ||
                 entry->rootValues.count != 0u)) return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_dce_effectful(const SZrExecIrInstruction *instruction) {
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
    const TZrUInt16 effectFlags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                               ZR_EXEC_IR_FLAG_MAY_THROW |
                                               ZR_EXEC_IR_FLAG_MAY_GC |
                                               ZR_EXEC_IR_FLAG_MAY_SUSPEND |
                                               ZR_EXEC_IR_FLAG_DEBUG_POLL |
                                               ZR_EXEC_IR_FLAG_GUARD_EXIT);
    return (TZrBool)(info == ZR_NULL || instruction->deoptId != 0u ||
                     instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
                     (instruction->flags & effectFlags) != 0u ||
                     info->memoryReads != 0u || info->memoryWrites != 0u ||
                     (info->flags & (ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE |
                                     ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW |
                                     ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC |
                                     ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND |
                                     ZR_EXEC_IR_SCHEMA_FLAG_MAY_DROP)) != 0u);
}

static TZrBool zr_dce_mark_instruction_operands(const SZrExecIrFunction *function,
                                                const SZrExecIrInstruction *instruction,
                                                TZrUInt8 *used, TZrBool *progress) {
    TZrUInt32 index;
    if (!zr_dce_range_valid(instruction->operands, function->operandCount) ||
        (instruction->operands.count != 0u && function->operands == ZR_NULL)) return ZR_FALSE;
    for (index = instruction->operands.start;
         index < instruction->operands.start + instruction->operands.count; ++index) {
        TZrExecIrValueId value = function->operands[index];
        if (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > function->valueCount)
            return ZR_FALSE;
        if (used[value] == 0u) *progress = ZR_TRUE;
        used[value] = 1u;
    }
    return ZR_TRUE;
}

static TZrBool zr_dce_mark_metadata(const SZrExecIrFunction *function, TZrUInt8 *used,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (!ZrCore_ExecIr_ValidateDeoptAggregates(function, diagnostic)) return ZR_FALSE;
    if ((function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->gcRootCount; ++index) {
        if (!zr_dce_mark_value(used, function->valueCount, function->gcRoots[index])) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, 0u, 0u, function->valueCount,
                                           function->gcRoots[index]);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->deoptValueCount; ++index) {
        if (!zr_dce_mark_value(used, function->valueCount, function->deoptValues[index])) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, 0u, 0u, function->valueCount,
                                           function->deoptValues[index]);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->deoptAggregateFieldCount; ++index) {
        const SZrExecIrDeoptAggregateField *field = &function->deoptAggregateFields[index];
        if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE &&
            !zr_dce_mark_value(used, function->valueCount, field->valueId)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, 0u, 0u, function->valueCount,
                                           field->valueId);
            return ZR_FALSE;
        }
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        if ((map->valueCount != 0u && map->valuePool == ZR_NULL) ||
            (map->rootCount != 0u && map->rootPool == ZR_NULL) ||
            (map->ownerStateCount != 0u && map->ownerStatePool == ZR_NULL) ||
            (map->entryCount != 0u && map->entries == ZR_NULL)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                           function, 0u, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (index = 0u; index < map->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry = &map->entries[index];
            if (!zr_dce_range_valid(entry->liveValues, map->valueCount) ||
                !zr_dce_range_valid(entry->rootValues, map->rootCount) ||
                !zr_dce_range_valid(entry->ownerStates, map->ownerStateCount)) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                               ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                               function, 0u, entry->instructionId, 0u, 0u);
                return ZR_FALSE;
            }
            for (TZrUInt32 at = entry->liveValues.start;
                 at < entry->liveValues.start + entry->liveValues.count; ++at) {
                if (!zr_dce_mark_value(used, function->valueCount, map->valuePool[at])) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, entry->instructionId,
                                                   function->valueCount, map->valuePool[at]);
                    return ZR_FALSE;
                }
            }
            for (TZrUInt32 at = entry->rootValues.start;
                 at < entry->rootValues.start + entry->rootValues.count; ++at) {
                if (!zr_dce_mark_value(used, function->valueCount, map->rootPool[at])) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                                   ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, entry->instructionId,
                                                   function->valueCount, map->rootPool[at]);
                    return ZR_FALSE;
                }
            }
        }
    }
    return ZR_TRUE;
}

static void zr_dce_nop(SZrExecIrInstruction *instruction) {
    instruction->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_NOP;
    instruction->flags = 0u;
    instruction->results = zr_dce_empty_range();
    instruction->operands = zr_dce_empty_range();
    instruction->phiRange = zr_dce_empty_range();
    instruction->successorRange = zr_dce_empty_range();
    instruction->memoryIn = zr_dce_empty_range();
    instruction->memoryOut = zr_dce_empty_range();
    instruction->effectIn = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
    instruction->effectOut = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
    instruction->matchTypeToken = 0u;
    instruction->layoutId = 0u;
}

static void zr_dce_remove_metadata_for_instruction(SZrExecIrFunction *function,
                                                   TZrExecIrInstructionId instructionId) {
    TZrUInt32 readIndex, writeIndex;
    if (function->sourceMaps != ZR_NULL) {
        writeIndex = 0u;
        for (readIndex = 0u; readIndex < function->sourceMapCount; ++readIndex) {
            if (function->sourceMaps[readIndex].instructionId != instructionId)
                function->sourceMaps[writeIndex++] = function->sourceMaps[readIndex];
        }
        function->sourceMapCount = writeIndex;
    }
    if (function->gcMap != ZR_NULL && function->gcMap->entries != ZR_NULL) {
        writeIndex = 0u;
        for (readIndex = 0u; readIndex < function->gcMap->entryCount; ++readIndex) {
            if (function->gcMap->entries[readIndex].site != instructionId)
                function->gcMap->entries[writeIndex++] = function->gcMap->entries[readIndex];
        }
        function->gcMap->entryCount = writeIndex;
    }
    if (function->stateMap != ZR_NULL && function->stateMap->entries != ZR_NULL) {
        writeIndex = 0u;
        for (readIndex = 0u; readIndex < function->stateMap->entryCount; ++readIndex) {
            if (function->stateMap->entries[readIndex].instructionId != instructionId)
                function->stateMap->entries[writeIndex++] = function->stateMap->entries[readIndex];
        }
        function->stateMap->entryCount = writeIndex;
    }
}

/* All scratch and work reservations precede publication. Token membership is
 * only a storage-domain classification; it never turns tokens into ValueIds. */
typedef struct SZrDceScratch {
    TZrUInt8 *used;
    TZrUInt8 *activePhis;
    TZrUInt8 *tokenRows;
    TZrUInt8 *erase;
    TZrUInt32 *prefix;
} SZrDceScratch;

static void zr_dce_scratch_free(SZrDceScratch *scratch) {
    free(scratch->used);
    free(scratch->activePhis);
    free(scratch->tokenRows);
    free(scratch->erase);
    free(scratch->prefix);
}

static TZrBool zr_dce_work_add(TZrUInt64 *work, TZrUInt64 extra) {
    if (extra > UINT64_MAX - *work) return ZR_FALSE;
    *work += extra;
    return ZR_TRUE;
}

static TZrBool zr_dce_preflight_work(const SZrExecIrFunction *function,
                                      TZrUInt64 *work) {
    TZrUInt32 index, region;
    *work = function->phiIncomingCount;
    if (!zr_dce_work_add(work, function->phiCount) ||
        !zr_dce_work_add(work, function->blockCount)) return ZR_FALSE;
    for (index = 0u; index < function->phiCount; ++index)
        if (!zr_dce_work_add(work, function->phiPool[index].incomings.count)) return ZR_FALSE;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (!zr_dce_work_add(work, block->phis.count) ||
            !zr_dce_work_add(work, block->effectPhiIncomings.count) ||
            !zr_dce_work_add(work, ZR_EXEC_IR_MEMORY_CLASS_COUNT)) return ZR_FALSE;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region)
            if (!zr_dce_work_add(work, block->memoryPhiIncomings[region].count)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void zr_dce_mark_rows(TZrUInt8 *rows, SZrExecIrRange range) {
    TZrUInt32 at;
    for (at = range.start; at < range.start + range.count; ++at) rows[at] = 1u;
}

static TZrBool zr_dce_classify(const SZrExecIrFunction *function,
                                SZrDceScratch *scratch,
                                SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, region;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        zr_dce_mark_rows(scratch->activePhis, block->phis);
        zr_dce_mark_rows(scratch->tokenRows, block->effectPhiIncomings);
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region)
            zr_dce_mark_rows(scratch->tokenRows, block->memoryPhiIncomings[region]);
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        TZrExecIrValueId value = function->phiIncoming[index].value;
        if (scratch->tokenRows[index] == 0u &&
            (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > function->valueCount)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, function->phiIncoming[index].predecessor,
                                           0u, function->valueCount, value);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_dce_result_used(const SZrExecIrFunction *function,
                                   const SZrExecIrInstruction *instruction,
                                   const TZrUInt8 *used) {
    TZrUInt32 at;
    for (at = instruction->results.start;
         at < instruction->results.start + instruction->results.count; ++at)
        if (used[function->results[at]] != 0u) return ZR_TRUE;
    return ZR_FALSE;
}

static void zr_dce_mark_incomings(const SZrExecIrFunction *function,
                                   SZrExecIrRange range, TZrUInt8 *used,
                                   TZrBool *progress) {
    TZrUInt32 at;
    for (at = range.start; at < range.start + range.count; ++at) {
        TZrExecIrValueId value = function->phiIncoming[at].value;
        if (used[value] == 0u) *progress = ZR_TRUE;
        used[value] = 1u;
    }
}

TZrBool ZrParser_ExecIr_RunDcePass(SZrExecIrFunction *function,
                                   SZrExecIrPassContext *context,
                                   TZrBool *changed,
                                   SZrExecIrDiagnostic *diagnostic) {
    SZrDceScratch scratch = {0};
    TZrUInt64 work;
    TZrUInt32 index;
    TZrBool progress, result = ZR_FALSE;

    if (changed != ZR_NULL) *changed = ZR_FALSE;
    if (!zr_dce_storage_valid(function, diagnostic) ||
        function->valueCount == UINT32_MAX ||
        zr_dce_size_mul_overflow((TZrUInt64)function->valueCount + 1u,
                                  sizeof(*scratch.used)) ||
        zr_dce_size_mul_overflow((TZrUInt64)function->phiCount + 1u,
                                  sizeof(*scratch.prefix)) ||
        zr_dce_size_mul_overflow((TZrUInt64)function->phiIncomingCount + 1u,
                                  sizeof(*scratch.tokenRows)) ||
        zr_dce_size_mul_overflow((TZrUInt64)function->instructionCount + 1u,
                                  sizeof(*scratch.erase))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!zr_dce_preflight_work(function, &work)) goto overflow;
    if (!ZrParser_ExecIr_PassConsumeBudget(function, context, work)) return ZR_TRUE;
    scratch.used = (TZrUInt8 *)calloc((size_t)function->valueCount + 1u, sizeof(*scratch.used));
    scratch.activePhis = (TZrUInt8 *)calloc((size_t)function->phiCount + 1u, sizeof(*scratch.activePhis));
    scratch.tokenRows = (TZrUInt8 *)calloc((size_t)function->phiIncomingCount + 1u, sizeof(*scratch.tokenRows));
    scratch.erase = (TZrUInt8 *)calloc((size_t)function->instructionCount + 1u, sizeof(*scratch.erase));
    scratch.prefix = (TZrUInt32 *)calloc((size_t)function->phiCount + 1u, sizeof(*scratch.prefix));
    if (scratch.used == ZR_NULL || scratch.activePhis == ZR_NULL ||
        scratch.tokenRows == ZR_NULL || scratch.erase == ZR_NULL || scratch.prefix == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, 0u, 0u, function->valueCount, 0u);
        goto done;
    }
    if (!zr_dce_classify(function, &scratch, diagnostic) ||
        !zr_dce_mark_metadata(function, scratch.used, diagnostic)) goto done;
    do {
        progress = ZR_FALSE;
        for (index = 0u; index < function->phiCount; ++index) {
            const SZrExecIrPhi *phi = &function->phiPool[index];
            if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) goto budget;
            if (scratch.activePhis[index] != 0u && scratch.used[phi->result] != 0u) {
                if (!ZrParser_ExecIr_PassConsumeBudget(function, context, phi->incomings.count))
                    goto budget;
                zr_dce_mark_incomings(function, phi->incomings, scratch.used, &progress);
            }
        }
        for (index = function->instructionCount; index != 0u; --index) {
            const SZrExecIrInstruction *instruction = &function->instructions[index - 1u];
            TZrBool observable = (TZrBool)(!zr_dce_pure((EZrExecIrOpcode)instruction->opcode) ||
                                           zr_dce_effectful(instruction) ||
                                           zr_dce_boundary_observable(function, index));
            if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) goto budget;
            if (observable || zr_dce_result_used(function, instruction, scratch.used)) {
                if (!ZrParser_ExecIr_PassConsumeBudget(function, context, instruction->operands.count))
                    goto budget;
                if (!zr_dce_mark_instruction_operands(function, instruction, scratch.used, &progress)) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, index, function->valueCount, 0u);
                    goto done;
                }
            }
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_PHI) {
                /* Legacy PHI opcodes stay conservative: retain their ordinary
                 * side definition as well as operand/phiRange dependencies. */
                work = (TZrUInt64)instruction->results.count + instruction->phiRange.count;
                if (!ZrParser_ExecIr_PassConsumeBudget(function, context, work)) goto budget;
                for (TZrUInt32 at = instruction->results.start;
                     at < instruction->results.start + instruction->results.count; ++at) {
                    TZrExecIrValueId value = function->results[at];
                    if (scratch.used[value] == 0u) progress = ZR_TRUE;
                    scratch.used[value] = 1u;
                }
                zr_dce_mark_incomings(function, instruction->phiRange, scratch.used, &progress);
            }
        }
    } while (progress != ZR_FALSE);

    /* Compute every decision and reserve prefix/remap/deletion work before
     * the first write. The fixed point only grows ValueId bits (<= V+1 rounds). */
    work = (TZrUInt64)function->phiCount + function->blockCount + function->instructionCount;
    for (index = 0u; index < function->phiCount; ++index) {
        TZrBool keep = (TZrBool)(scratch.activePhis[index] != 0u &&
                                 scratch.used[function->phiPool[index].result] != 0u);
        scratch.prefix[index + 1u] = scratch.prefix[index] + (keep ? 1u : 0u);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrBool observable = (TZrBool)(!zr_dce_pure((EZrExecIrOpcode)instruction->opcode) ||
                                       zr_dce_effectful(instruction) ||
                                       zr_dce_boundary_observable(function, index + 1u));
        if (!zr_dce_work_add(&work, instruction->results.count)) goto overflow;
        if (!observable && !zr_dce_result_used(function, instruction, scratch.used)) {
            scratch.erase[index] = 1u;
            if (!zr_dce_work_add(&work, function->sourceMapCount) ||
                !zr_dce_work_add(&work, function->gcMap == ZR_NULL ? 0u : function->gcMap->entryCount) ||
                !zr_dce_work_add(&work, function->stateMap == ZR_NULL ? 0u : function->stateMap->entryCount))
                goto overflow;
        }
    }
    if (!ZrParser_ExecIr_PassConsumeBudget(function, context, work)) goto budget;

    for (index = 0u; index < function->blockCount; ++index) {
        SZrExecIrRange old = function->blocks[index].phis;
        function->blocks[index].phis.start = scratch.prefix[old.start];
        function->blocks[index].phis.count = scratch.prefix[old.start + old.count] - scratch.prefix[old.start];
    }
    for (index = 0u; index < function->phiCount; ++index) {
        if (scratch.prefix[index + 1u] != scratch.prefix[index])
            function->phiPool[scratch.prefix[index]] = function->phiPool[index];
    }
    if (function->phiCount != scratch.prefix[function->phiCount] && changed != ZR_NULL)
        *changed = ZR_TRUE;
    function->phiCount = scratch.prefix[function->phiCount];
    for (index = 0u; index < function->instructionCount; ++index) {
        if (scratch.erase[index] != 0u) {
            TZrExecIrSourceId sourceId = function->instructions[index].sourceId;
            zr_dce_nop(&function->instructions[index]);
            zr_dce_remove_metadata_for_instruction(function, index + 1u);
            if (changed != ZR_NULL) *changed = ZR_TRUE;
            if (context != ZR_NULL && context->lastSourceId == 0u)
                context->lastSourceId = sourceId;
        }
    }
    result = ZR_TRUE;
    goto done;
 overflow:
    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                   function, 0u, 0u, 0u, 0u);
    goto done;
 budget:
    result = ZR_TRUE;
 done:
    zr_dce_scratch_free(&scratch);
    return result;
}
