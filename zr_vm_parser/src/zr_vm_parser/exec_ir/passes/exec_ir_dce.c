#include "../exec_ir_pass_internal.h"

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
        function->gcMapCount > function->gcMapCapacity ||
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
        (void)info;
    }
    for (index = 0u; index < function->phiCount; ++index) {
        const SZrExecIrPhi *phi = &function->phiPool[index];
        if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID || phi->result > function->valueCount ||
            !zr_dce_range_valid(phi->incomings, function->phiIncomingCount)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                           function, 0u, 0u, function->valueCount, phi->result);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[index];
        if (incoming->value == ZR_EXEC_IR_VALUE_ID_INVALID ||
            incoming->value > function->valueCount || incoming->predecessor == 0u ||
            incoming->predecessor > function->blockCount) {
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
        case ZR_EXEC_IR_OPCODE_MOVE:
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
                                                TZrUInt8 *used) {
    TZrUInt32 index;
    if (!zr_dce_range_valid(instruction->operands, function->operandCount) ||
        (instruction->operands.count != 0u && function->operands == ZR_NULL)) return ZR_FALSE;
    for (index = instruction->operands.start;
         index < instruction->operands.start + instruction->operands.count; ++index) {
        if (!zr_dce_mark_value(used, function->valueCount, function->operands[index]))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_dce_mark_metadata(const SZrExecIrFunction *function, TZrUInt8 *used,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
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

TZrBool ZrParser_ExecIr_RunDcePass(SZrExecIrFunction *function,
                                   SZrExecIrPassContext *context,
                                   TZrBool *changed,
                                   SZrExecIrDiagnostic *diagnostic) {
    TZrUInt8 *used;
    size_t bytes;
    TZrUInt32 index;
    TZrBool progress;

    if (changed != ZR_NULL) *changed = ZR_FALSE;
    if (!zr_dce_storage_valid(function, diagnostic) ||
        function->valueCount == UINT32_MAX ||
        zr_dce_size_mul_overflow((TZrUInt64)function->valueCount + 1u,
                                  sizeof(*used))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    bytes = (size_t)((TZrUInt64)function->valueCount + 1u) * sizeof(*used);
    used = (TZrUInt8 *)calloc(1u, bytes);
    if (used == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, 0u, 0u, function->valueCount, 0u);
        return ZR_FALSE;
    }
    if (!zr_dce_mark_metadata(function, used, diagnostic)) {
        free(used);
        return ZR_FALSE;
    }
    do {
        progress = ZR_FALSE;
        /* Phi incoming values are uses only when the phi result itself is
         * live.  Treating the entire pool as a root would keep unreachable
         * branches alive forever. */
        for (index = 0u; index < function->phiCount; ++index) {
            const SZrExecIrPhi *phi = &function->phiPool[index];
            if (used[phi->result] == 0u) continue;
            for (TZrUInt32 at = phi->incomings.start;
                 at < phi->incomings.start + phi->incomings.count; ++at) {
                TZrExecIrValueId value = function->phiIncoming[at].value;
                if (used[value] == 0u) {
                    used[value] = 1u;
                    progress = ZR_TRUE;
                }
            }
        }
        for (index = function->instructionCount; index != 0u; --index) {
            SZrExecIrInstruction *instruction = &function->instructions[index - 1u];
            TZrBool observable;
            TZrBool resultUsed = ZR_FALSE;
            TZrUInt32 resultIndex;
            if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) {
                free(used);
                return ZR_TRUE;
            }
            if (!zr_dce_range_valid(instruction->results, function->resultCount) ||
                (instruction->results.count != 0u && function->results == ZR_NULL)) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                               function, 0u, index, 0u, 0u);
                free(used);
                return ZR_FALSE;
            }
            for (resultIndex = instruction->results.start;
                 resultIndex < instruction->results.start + instruction->results.count;
                 ++resultIndex) {
                TZrExecIrValueId value = function->results[resultIndex];
                if (value == ZR_EXEC_IR_VALUE_ID_INVALID || value > function->valueCount) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, index, function->valueCount, value);
                    free(used);
                    return ZR_FALSE;
                }
                if (used[value] != 0u) resultUsed = ZR_TRUE;
            }
            observable = (TZrBool)(!zr_dce_pure((EZrExecIrOpcode)instruction->opcode) ||
                                   zr_dce_effectful(instruction) ||
                                   zr_dce_boundary_observable(function, index));
            if (observable || resultUsed) {
                if (!zr_dce_mark_instruction_operands(function, instruction, used)) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, index, function->valueCount, 0u);
                    free(used);
                    return ZR_FALSE;
                }
            } else if (zr_dce_pure((EZrExecIrOpcode)instruction->opcode)) {
                TZrExecIrSourceId sourceId = instruction->sourceId;
                zr_dce_nop(instruction);
                zr_dce_remove_metadata_for_instruction(function, index);
                if (changed != ZR_NULL) *changed = ZR_TRUE;
                if (context != ZR_NULL && context->lastSourceId == 0u)
                    context->lastSourceId = sourceId;
                progress = ZR_TRUE;
            }
        }
    } while (progress != ZR_FALSE);
    free(used);
    return ZR_TRUE;
}
