#include "zr_vm_core/exec_ir_state_map.h"
#include "exec_ir_materialize_owners.h"
#include "exec_ir_deopt_aggregate.h"
#include "exec_ir_state_map_storage.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void zr_state_map_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrUInt32 zr_state_map_containing_block(const SZrExecIrFunction *function,
                                                TZrUInt32 instructionId) {
    TZrUInt32 index;

    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount || function->blocks == ZR_NULL) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 instructionIndex = instructionId - 1u;
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start < block->instructionRange.count) {
            return block->id;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static void zr_state_map_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                        EZrExecutionDiagnosticCode code,
                                        const SZrExecIrFunction *function,
                                        const SZrExecIrStateMapEntry *entry,
                                        TZrUInt32 instructionId,
                                        TZrUInt32 sourceId,
                                        TZrUInt64 expected,
                                        TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
    diagnostic->blockId = zr_state_map_containing_block(function, instructionId);
    diagnostic->expectedVersion = (TZrUInt32)(expected & UINT32_MAX);
    diagnostic->actualVersion = (TZrUInt32)(actual & UINT32_MAX);
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
    if (entry != ZR_NULL) {
        if (diagnostic->instructionId == 0u) {
            diagnostic->instructionId = entry->instructionId;
        }
        if (diagnostic->sourceId == 0u) {
            diagnostic->sourceId = entry->sourceId;
        }
        if (diagnostic->blockId == ZR_EXEC_IR_BLOCK_ID_INVALID) {
            diagnostic->blockId = zr_state_map_containing_block(function,
                                                                  entry->instructionId);
        }
    }
}

static TZrBool zr_state_map_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_state_map_size_valid(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
}

static TZrBool zr_state_map_phase_valid(EZrExecIrStateMapPhase phase) {
    switch (phase) {
        case ZR_EXEC_IR_STATE_BEFORE_EFFECT:
        case ZR_EXEC_IR_STATE_AFTER_EFFECT:
        case ZR_EXEC_IR_STATE_CLEANUP_COMPLETE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}


TZrBool ZrCore_ExecIr_StateMapBoundaryFlags(
        const SZrExecIrInstruction *instruction,
        TZrUInt32 *flags) {
    const SZrExecIrOpcodeInfo *info;
    TZrUInt32 boundary = 0u;

    if (instruction == ZR_NULL || flags == ZR_NULL) {
        return ZR_FALSE;
    }
    *flags = 0u;
    info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
    if (info == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_ALLOCATE) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_GC) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_MAY_SUSPEND) != 0u ||
        (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_DEBUG_POLL) != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL;
    }
    if ((instruction->flags & ZR_EXEC_IR_FLAG_GUARD_EXIT) != 0u ||
        instruction->deoptId != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT;
    }
    if (instruction->deoptId != 0u) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT;
    }
    if (((EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_DROP ||
         (EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED)) {
        boundary |= ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP;
    }
    *flags = boundary;
    return (TZrBool)(boundary != 0u);
}

const SZrExecIrStateMapEntry *ZrCore_ExecIr_StateMapFind(
        const SZrExecIrStateMap *map,
        TZrExecIrSourceId sourceId,
        TZrUInt32 resumeId,
        EZrExecIrStateMapPhase phase) {
    TZrUInt32 index;

    if (map == ZR_NULL || !zr_state_map_phase_valid(phase) ||
        resumeId == 0u || map->entryCount > map->entryCapacity ||
        (map->entryCount != 0u && map->entries == ZR_NULL)) {
        return ZR_NULL;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if (entry->sourceId == sourceId && entry->resumeId == resumeId &&
            entry->phase == phase) {
            return entry;
        }
    }
    return ZR_NULL;
}


static TZrBool zr_state_map_function_values_valid(const SZrExecIrFunction *function,
                                                  SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->id == ZR_EXEC_IR_FUNCTION_ID_INVALID ||
        function->functionToken == 0u ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operandPool == ZR_NULL) ||
        (function->resultCount != 0u && function->resultPool == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        if (value->id != index + 1u ||
            (TZrUInt32)value->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
            (TZrUInt32)value->nullability >= ZR_EXEC_IR_NULLABILITY_COUNT) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, ZR_NULL, 0u, 0u,
                                        index + 1u, value->id);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (block->id != index + 1u ||
            !zr_state_map_range_valid(block->instructionRange,
                                      function->instructionCount) ||
            !zr_state_map_range_valid(block->predecessorRange,
                                      function->predecessorCount) ||
            !zr_state_map_range_valid(block->successorRange,
                                      function->successorCount) ||
            !zr_state_map_range_valid(block->phis, function->phiCount) ||
            !zr_state_map_range_valid(block->effectPhiIncomings,
                                      function->phiIncomingCount)) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, ZR_NULL, 0u, block->id,
                                        index + 1u, block->id);
            return ZR_FALSE;
        }
        {
            TZrUInt32 region;
            for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
                if (!zr_state_map_range_valid(block->memoryPhiIncomings[region],
                                              function->phiIncomingCount)) {
                    zr_state_map_set_diagnostic(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                            function, ZR_NULL, 0u, block->id,
                            index + 1u,
                            block->memoryPhiIncomings[region].count);
                    return ZR_FALSE;
                }
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_validate_value_range(
        const SZrExecIrFunction *function,
        const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry,
        SZrExecIrRange range,
        TZrBool roots,
        TZrBool suspended,
        SZrExecIrDiagnostic *diagnostic) {
    const TZrExecIrValueId *pool = roots ? map->rootPool : map->valuePool;
    TZrUInt32 poolCount = roots ? map->rootCount : map->valueCount;
    TZrUInt32 index;

    if (!zr_state_map_range_valid(range, poolCount) ||
        (range.count != 0u && pool == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, entry, entry != ZR_NULL ? entry->instructionId : 0u,
                                    entry != ZR_NULL ? entry->sourceId : 0u,
                                    poolCount, range.start);
        return ZR_FALSE;
    }
    for (index = range.start; index < range.start + range.count; ++index) {
        TZrExecIrValueId valueId = pool[index];
        const SZrExecIrValue *value;
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > function->valueCount) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry != ZR_NULL ? entry->instructionId : 0u,
                                        entry != ZR_NULL ? entry->sourceId : 0u,
                                        function->valueCount, valueId);
            return ZR_FALSE;
        }
        value = &function->values[valueId - 1u];
        if (roots && value->ownership != ZR_EXEC_IR_OWNERSHIP_GC &&
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE &&
            value->ownership != ZR_EXEC_IR_OWNERSHIP_SHARED) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry != ZR_NULL ? entry->instructionId : 0u,
                                        entry != ZR_NULL ? entry->sourceId : 0u,
                                        ZR_EXEC_IR_OWNERSHIP_GC, value->ownership);
            return ZR_FALSE;
        }
        if (suspended && !roots &&
            value->ownership == ZR_EXEC_IR_OWNERSHIP_BORROWED) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND,
                                        function, entry, entry != ZR_NULL ? entry->instructionId : 0u,
                                        entry != ZR_NULL ? entry->sourceId : 0u,
                                        0u, valueId);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_range_values_unique(const SZrExecIrStateMap *map,
                                                SZrExecIrRange range,
                                                TZrBool roots) {
    const TZrExecIrValueId *pool = roots ? map->rootPool : map->valuePool;
    TZrUInt32 index;
    TZrUInt32 prior;

    for (index = 0u; index < range.count; ++index) {
        for (prior = 0u; prior < index; ++prior) {
            if (pool[range.start + index] == pool[range.start + prior]) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_roots_are_live(const SZrExecIrStateMap *map,
                                            const SZrExecIrStateMapEntry *entry) {
    TZrUInt32 rootIndex;
    TZrUInt32 liveIndex;

    for (rootIndex = 0u; rootIndex < entry->rootValues.count; ++rootIndex) {
        TZrExecIrValueId root = map->rootPool[entry->rootValues.start + rootIndex];
        TZrBool found = ZR_FALSE;
        for (liveIndex = 0u; liveIndex < entry->liveValues.count; ++liveIndex) {
            if (map->valuePool[entry->liveValues.start + liveIndex] == root) {
                found = ZR_TRUE;
                break;
            }
        }
        if (!found) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_entry_deopt_valid(const SZrExecIrFunction *function,
                                              const SZrExecIrStateMap *map,
                                              const SZrExecIrStateMapEntry *entry,
                                              SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 matchingStates = 0u;
    const SZrExecIrDeoptState *matchingState = ZR_NULL;

    if (entry->deoptId == 0u) {
        return ZR_TRUE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (state->deoptId == entry->deoptId) {
            ++matchingStates;
            matchingState = state;
        }
    }
    if (matchingStates != 1u) {
        zr_state_map_set_diagnostic(
                diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                function, entry, entry->instructionId, entry->sourceId,
                1u, matchingStates);
        return ZR_FALSE;
    }
    if (matchingState->sourceId != entry->sourceId ||
        matchingState->resumeId == 0u ||
        matchingState->resumeId != entry->resumeId ||
        !zr_state_map_range_valid(matchingState->valueRange,
                                  function->deoptValueCount) ||
        (matchingState->valueRange.count != 0u &&
         function->deoptValues == ZR_NULL) ||
        !zr_state_map_range_valid(entry->liveValues, map->valueCount) ||
        (entry->liveValues.count != 0u && map->valuePool == ZR_NULL)) {
        zr_state_map_set_diagnostic(
                diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                function, entry, entry->instructionId, entry->sourceId,
                matchingState->resumeId, entry->resumeId);
        return ZR_FALSE;
    }
    for (index = matchingState->valueRange.start;
         index < matchingState->valueRange.start + matchingState->valueRange.count;
         ++index) {
        TZrUInt32 liveIndex;
        TZrBool live = ZR_FALSE;
        TZrExecIrValueId valueId = function->deoptValues[index];
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount) {
            break;
        }
        for (liveIndex = entry->liveValues.start;
             liveIndex < entry->liveValues.start + entry->liveValues.count;
             ++liveIndex) {
            if (map->valuePool[liveIndex] == valueId) {
                live = ZR_TRUE;
                break;
            }
        }
        if (!live) {
            break;
        }
    }
    if (index == matchingState->valueRange.start + matchingState->valueRange.count) {
        return zr_exec_ir_deopt_aggregates_live(function, matchingState,
                                                map, entry, diagnostic);
    }
    zr_state_map_set_diagnostic(diagnostic,
                                ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                function, entry, entry->instructionId, entry->sourceId,
                                entry->deoptId, 0u);
    return ZR_FALSE;
}

static TZrExecIrBlockId zr_state_map_expected_handler(
        const SZrExecIrFunction *function,
        TZrUInt32 instructionId,
        TZrUInt32 boundaryFlags) {
    const SZrExecIrBlock *sourceBlock;
    TZrExecIrBlockId sourceBlockId;
    TZrUInt32 successorIndex;

    if (function == ZR_NULL || instructionId == 0u ||
        (boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW) == 0u) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    sourceBlockId = zr_state_map_containing_block(function, instructionId);
    sourceBlock = ZrCore_ExecIr_FunctionBlockAtConst(function, sourceBlockId);
    if (sourceBlock == ZR_NULL ||
        !zr_state_map_range_valid(sourceBlock->successorRange,
                                  function->successorCount) ||
        (sourceBlock->successorRange.count != 0u &&
         function->successors == ZR_NULL)) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    for (successorIndex = sourceBlock->successorRange.start;
         successorIndex < sourceBlock->successorRange.start +
                              sourceBlock->successorRange.count;
         ++successorIndex) {
        TZrExecIrBlockId successor = function->successors[successorIndex];
        const SZrExecIrBlock *successorBlock =
            ZrCore_ExecIr_FunctionBlockAtConst(function, successor);
        if (successorBlock != ZR_NULL &&
            (successorBlock->flags & (ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION |
                                      ZR_EXEC_IR_BLOCK_FLAG_CLEANUP)) != 0u) {
            return successor;
        }
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool zr_state_map_entry_handler_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrStateMapEntry *entry) {
    TZrExecIrBlockId expectedHandler;

    if ((entry->boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW) == 0u) {
        return (TZrBool)(entry->handlerBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID);
    }
    expectedHandler = zr_state_map_expected_handler(
            function, entry->instructionId, entry->boundaryFlags);
    return (TZrBool)(entry->handlerBlockId == expectedHandler);
}

static TZrBool zr_state_map_entry_boundary_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrStateMapEntry *entry) {
    TZrUInt32 expectedFlags;

    if (entry->instructionId == 0u ||
        entry->instructionId > function->instructionCount ||
        !ZrCore_ExecIr_StateMapBoundaryFlags(
                &function->instructions[entry->instructionId - 1u],
                &expectedFlags)) {
        return ZR_FALSE;
    }
    return (TZrBool)(entry->boundaryFlags == expectedFlags);
}

static TZrBool zr_state_map_entry_valid(const SZrExecIrFunction *function,
                                        const SZrExecIrOwnerAnalysis *ownership,
                                        const SZrStateMapLiveness *liveness,
                                        const SZrExecIrStateMap *map,
                                        const SZrExecIrStateMapEntry *entry,
                                        SZrExecIrDiagnostic *diagnostic) {
    if (entry == ZR_NULL || entry->sourceId == 0u ||
        !zr_state_map_phase_valid(entry->phase) ||
        entry->resumeId == 0u ||
        entry->boundaryFlags == 0u ||
        (entry->boundaryFlags & ~ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK) != 0u ||
        entry->exceptionState !=
            (entry->boundaryFlags & ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK) ||
        entry->instructionId == 0u || entry->instructionId > function->instructionCount ||
        !ownership->reachable[entry->instructionId - 1u] ||
        !zr_state_map_entry_boundary_valid(function, entry) ||
        !zr_state_map_entry_handler_valid(function, entry) ||
        (entry->phase == ZR_EXEC_IR_STATE_CLEANUP_COMPLETE &&
         (entry->boundaryFlags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW |
                                  ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND |
                                  ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)) == 0u) ||
        (entry->phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT && entry->cleanupState != 0u) ||
        (entry->phase == ZR_EXEC_IR_STATE_AFTER_EFFECT && entry->cleanupState != 1u) ||
        (entry->phase == ZR_EXEC_IR_STATE_CLEANUP_COMPLETE && entry->cleanupState != 2u) ||
        ((entry->boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT) != 0u &&
         entry->deoptId == 0u) ||
        !zr_state_map_range_valid(entry->liveValues, map->valueCount) ||
        !zr_state_map_range_valid(entry->rootValues, map->rootCount) ||
        !zr_state_map_owners_valid(function, ownership, liveness, map, entry, diagnostic) ||
        !zr_state_map_entry_deopt_valid(function, map, entry, diagnostic) ||
        !zr_state_map_validate_value_range(
                function, map, entry, entry->liveValues, ZR_FALSE,
                (TZrBool)((entry->boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND) != 0u),
                diagnostic) ||
        !zr_state_map_validate_value_range(function, map, entry, entry->rootValues,
                                           ZR_TRUE, ZR_FALSE, diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry,
                                        entry != ZR_NULL ? entry->instructionId : 0u,
                                        entry != ZR_NULL ? entry->sourceId : 0u,
                                        0u, 0u);
        }
        return ZR_FALSE;
    }
    if (!zr_state_map_range_values_unique(map, entry->liveValues, ZR_FALSE) ||
        !zr_state_map_range_values_unique(map, entry->rootValues, ZR_TRUE) ||
        !zr_state_map_roots_are_live(map, entry)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, entry, entry->instructionId,
                                    entry->sourceId, 0u, 0u);
        return ZR_FALSE;
    }
    if (entry->instructionId != 0u && function->instructionCount != 0u) {
        const SZrExecIrInstruction *instruction = &function->instructions[entry->instructionId - 1u];
        TZrExecIrSourceId expectedSource =
            instruction->sourceId != 0u ? instruction->sourceId : entry->instructionId;
        if (entry->sourceId != expectedSource) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, expectedSource,
                                        entry->sourceId);
            return ZR_FALSE;
        }
        if (entry->effectIn != instruction->effectIn ||
            entry->effectOut != instruction->effectOut ||
            entry->deoptId != instruction->deoptId) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, instruction->effectOut,
                                        entry->effectOut);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ValidateStateMap(const SZrExecIrFunction *function,
                                     const SZrExecIrStateMap *map,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 other;
    SZrExecIrOwnerAnalysis ownership = {0};
    SZrStateMapLiveness liveness = {0};
    EZrExecutionDiagnosticCode livenessCode;

    zr_state_map_clear_diagnostic(diagnostic);
    if (function == ZR_NULL || map == ZR_NULL) {
        zr_state_map_set_diagnostic(diagnostic,
                ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                function, ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (!zr_state_map_function_values_valid(function, diagnostic) ||
        !zr_exec_ir_deopt_aggregates_validate(function, diagnostic) ||
        !zr_state_map_storage_shape_valid(map) ||
        map->functionToken == 0u || map->functionToken != function->functionToken ||
        map->signatureHash != function->signatureHash ||
        map->generation != function->contract.generation) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, ZR_NULL, 0u, 0u, 0u, 0u);
        }
        return ZR_FALSE;
    }
    if (function->instructionCount != 0u && function->instructions == ZR_NULL) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, ZR_NULL, 0u, 0u,
                                    function->instructionCount, 0u);
        return ZR_FALSE;
    }
    if (function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, ZR_NULL, 0u, 0u,
                                    function->deoptStateCount,
                                    function->deoptStateCapacity);
        return ZR_FALSE;
    }
    if (function->blockCount > function->blockCapacity ||
        (function->blockCount != 0u && function->blocks == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, ZR_NULL, 0u, 0u,
                                    function->blockCount, function->blockCapacity);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_OwnerAnalysisBuild(function, &ownership, diagnostic)) {
        return ZR_FALSE;
    }
    livenessCode = ZrCore_ExecIr_StateMapLivenessBuild(function, &liveness);
    if (livenessCode != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        if (diagnostic != ZR_NULL) diagnostic->code = livenessCode;
        goto invalid;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if (!zr_state_map_entry_valid(function, &ownership, &liveness, map, entry, diagnostic)) {
            goto invalid;
        }
        for (other = 0u; other < index; ++other) {
            const SZrExecIrStateMapEntry *prior = &map->entries[other];
            if (prior->instructionId == entry->instructionId &&
                (prior->sourceId != entry->sourceId ||
                 prior->resumeId != entry->resumeId)) {
                zr_state_map_set_diagnostic(diagnostic,
                                            ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, entry, entry->instructionId,
                                            entry->sourceId, prior->resumeId,
                                            entry->resumeId);
                goto invalid;
            }
            if (prior->resumeId == entry->resumeId &&
                (prior->instructionId != entry->instructionId ||
                 prior->sourceId != entry->sourceId)) {
                zr_state_map_set_diagnostic(diagnostic,
                                            ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, entry, entry->instructionId,
                                            entry->sourceId, prior->instructionId,
                                            entry->instructionId);
                goto invalid;
            }
            if (prior->sourceId == entry->sourceId &&
                prior->resumeId == entry->resumeId && prior->phase == entry->phase) {
                zr_state_map_set_diagnostic(diagnostic,
                                            ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, entry, entry->instructionId,
                                            entry->sourceId, 0u, entry->resumeId);
                goto invalid;
            }
        }
    }
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
    return ZR_TRUE;
invalid:
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
    return ZR_FALSE;
}


TZrBool ZrCore_ExecIr_MaterializeState(const SZrExecIrResumeRequest *request,
                                       SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *function;
    const SZrExecIrStateMap *map;
    const SZrExecIrStateMapEntry *entry;
    SZrExecIrMaterializedState prepared;
    TZrUInt32 index;

    zr_state_map_clear_diagnostic(diagnostic);
    if (request == ZR_NULL || request->function == ZR_NULL ||
        !zr_state_map_phase_valid(request->phase)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    request != ZR_NULL ? request->function : ZR_NULL,
                                    ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    function = request->function;
    map = request->map != ZR_NULL ? request->map : function->stateMap;
    if (!ZrCore_ExecIr_ValidateStateMap(function, map, diagnostic)) {
        return ZR_FALSE;
    }
    entry = ZrCore_ExecIr_StateMapFind(map, request->sourceId,
                                       request->resumeId, request->phase);
    if (entry == ZR_NULL) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_RESUME_NOT_FOUND,
                                    function, ZR_NULL, 0u, request->sourceId,
                                    request->resumeId, 0u);
        return ZR_FALSE;
    }
    if (request->functionToken != 0u && request->functionToken != function->functionToken) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                                    function, entry, entry->instructionId, request->sourceId,
                                    function->functionToken, request->functionToken);
        return ZR_FALSE;
    }
    if (request->generation != 0u && request->generation != map->generation) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                                    function, entry, entry->instructionId, request->sourceId,
                                    map->generation, request->generation);
        return ZR_FALSE;
    }
    if (request->signatureHash != 0u && request->signatureHash != map->signatureHash) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                                    function, entry, entry->instructionId, request->sourceId,
                                    map->signatureHash, request->signatureHash);
        return ZR_FALSE;
    }
    if (request->target == ZR_NULL)
        return zr_state_map_resolve_owners(request, map, entry, ZR_NULL, diagnostic);
    if (!zr_state_map_target_valid(request->target, map, function)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                                    function, entry, entry->instructionId,
                                    entry->sourceId, 0u, 0u);
        return ZR_FALSE;
    }

    ZrCore_ExecIr_MaterializedStateInit(&prepared);
    prepared.sourceId = entry->sourceId;
    prepared.resumeId = entry->resumeId;
    prepared.cleanupState = entry->cleanupState;
    prepared.phase = entry->phase;
    prepared.generation = map->generation;
    prepared.effectIn = entry->effectIn;
    prepared.effectOut = entry->effectOut;
    prepared.handlerBlockId = entry->handlerBlockId;
    prepared.exceptionState = entry->exceptionState;
    prepared.boundaryFlags = entry->boundaryFlags;
    prepared.deoptId = entry->deoptId;
    if (entry->liveValues.count != 0u) {
        if (!zr_state_map_size_valid(entry->liveValues.count,
                                     sizeof(*prepared.values))) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->liveValues.count, 0u);
            return ZR_FALSE;
        }
        prepared.values = malloc((size_t)entry->liveValues.count * sizeof(*prepared.values));
        if (prepared.values == ZR_NULL) {
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->liveValues.count, 0u);
            return ZR_FALSE;
        }
        memcpy(prepared.values,
               &map->valuePool[entry->liveValues.start],
               (size_t)entry->liveValues.count * sizeof(*prepared.values));
        prepared.valueCount = entry->liveValues.count;
        prepared.valueCapacity = entry->liveValues.count;
    }
    if (entry->rootValues.count != 0u) {
        if (!zr_state_map_size_valid(entry->rootValues.count, sizeof(*prepared.roots))) {
            ZrCore_ExecIr_MaterializedStateFree(&prepared);
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->rootValues.count, 0u);
            return ZR_FALSE;
        }
        prepared.roots = malloc((size_t)entry->rootValues.count * sizeof(*prepared.roots));
        if (prepared.roots == ZR_NULL) {
            ZrCore_ExecIr_MaterializedStateFree(&prepared);
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->rootValues.count, 0u);
            return ZR_FALSE;
        }
        memcpy(prepared.roots,
               &map->rootPool[entry->rootValues.start],
               (size_t)entry->rootValues.count * sizeof(*prepared.roots));
        prepared.rootCount = entry->rootValues.count;
        prepared.rootCapacity = entry->rootValues.count;
    }
    if (entry->ownerStates.count != 0u) {
        if (!zr_state_map_size_valid(entry->ownerStates.count,
                                     sizeof(*prepared.ownerStates))) {
            ZrCore_ExecIr_MaterializedStateFree(&prepared);
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->ownerStates.count, 0u);
            return ZR_FALSE;
        }
        prepared.ownerStates = malloc((size_t)entry->ownerStates.count *
                                       sizeof(*prepared.ownerStates));
        if (prepared.ownerStates == ZR_NULL) {
            ZrCore_ExecIr_MaterializedStateFree(&prepared);
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, entry->ownerStates.count, 0u);
            return ZR_FALSE;
        }
        for (index = 0u; index < entry->ownerStates.count; ++index) {
            prepared.ownerStates[index] = map->ownerStatePool[entry->ownerStates.start + index];
        }
        prepared.ownerStateCount = entry->ownerStates.count;
        prepared.ownerStateCapacity = entry->ownerStates.count;
    }

    if (!zr_state_map_resolve_owners(request, map, entry, &prepared, diagnostic) ||
        !zr_exec_ir_deopt_aggregates_prepare(function, entry, &prepared, diagnostic)) {
        ZrCore_ExecIr_MaterializedStateFree(&prepared);
        return ZR_FALSE;
    }

    /* The single commit point is after every validation and allocation. */
    ZrCore_ExecIr_MaterializedStateFree(request->target);
    *request->target = prepared;
    return ZR_TRUE;
}

/**
 * @fn static TZrBool zr_state_map_phase_valid(EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 判断 StateMap 查询、条目及请求使用的逻辑阶段是否受当前 Core 实现支持。
 *
 * ## Parameters
 * @param[in] phase 待检查的逻辑阶段；接受 BEFORE_EFFECT、AFTER_EFFECT 与 CLEANUP_COMPLETE。
 *
 * ## Returns
 * @return 三种受支持阶段返回 ZR_TRUE；PHASE_COUNT 及其他未知值返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 只比较枚举值，不访问或保存 map、function 或 instruction；调用后不保留输入。
 *
 * ## References
 * Core 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:146；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:482；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:676。Parser 同名静态函数属于另一物理定义。
 */
/**
 * @fn TZrBool ZrCore_ExecIr_StateMapBoundaryFlags(const SZrExecIrInstruction *instruction, TZrUInt32 *flags)
 *
 * ## Summary
 * 合并指令显式 effect 标志与 opcode schema 属性，计算该指令对应的 state-map 边界位，供 Core 校验和 Parser 构造器共用。
 *
 * ## Parameters
 * @param[in] instruction 调用期间可读的待分类指令。
 * @param[out] flags 独立、可写的输出槽；两参数均非空时函数先将其清零，再写入计算结果。不得与 instruction 存储重叠。
 *
 * ## Returns
 * @return 两参数非空、opcode schema 可查询且结果边界位非零时返回 ZR_TRUE；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 任一参数为空时在清零前返回 FALSE；两者非空后先清零，未知 opcode 时保持零并返回 FALSE。对 MAY_ALLOCATE、MAY_GC、MAY_THROW、MAY_SUSPEND，指令或 schema 任一来源具备相应属性就设置对应边界位。DEBUG_POLL 仅看指令标志；GUARD_EXIT 接受显式指令标志或非零 deoptId；DEOPT 由非零 deoptId 贡献；DROP/DROP_IF_INITIALIZED 设置 CLEANUP。已知 opcode 得到零边界位也返回 FALSE。指令只在调用期间借用；无分配、无持久化、无内部同步，且不检测输出别名。
 *
 * ## References
 * 定义：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:91；Core 边界比较调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:467；Parser 构造调用：zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:402。
 */
/**
 * @fn const SZrExecIrStateMapEntry *ZrCore_ExecIr_StateMapFind(const SZrExecIrStateMap *map, TZrExecIrSourceId sourceId, TZrUInt32 resumeId, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 在 state-map entry 数组中按 sourceId、resumeId 与逻辑 phase 查询首个匹配项。
 *
 * ## Parameters
 * @param[in] map 待查询的映射；函数仅读取轻量数组描述符及被扫描的条目。
 * @param[in] sourceId 精确匹配的源码点 ID；本接口不额外禁止零值。
 * @param[in] resumeId 精确匹配的恢复 ID；零值被拒绝。
 * @param[in] phase 精确匹配的受支持逻辑阶段。
 *
 * ## Returns
 * @return 返回首个三元组相等 entry 的只读借用指针；轻量描述符检查失败或未命中时返回 NULL。
 *
 * ## Constraints and lifecycle
 * 只检查 map/phase/resumeId、entryCount 不大于 entryCapacity，以及 entryCount 非零时 entries 非空；不调用 StateMapStorageValid 或 ValidateStateMap，不验证底层分配 extent、实际可读性或 entry 语义，也不拒绝重复项。顺序扫描，重复键返回数组首项。返回指针直接别名 map.entries 中的元素，不是快照；调用方须在读取期间保持 map 描述符、entries 数组及对应条目稳定。修改条目会改变借用所见内容；替换、重分配或释放 entries 后借用指针失效。函数不写 map、不保存参数、不分配资源、无内部同步。
 *
 * ## References
 * Core 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:688；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:38；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:117。直接调用还见 tests/parser/ssa_state_map_verifier_cases.inc:60；tests/parser/ssa_pass_manager_state_maps_cases.inc:129；tests/parser/ssa_pass_manager_state_maps_cases.inc:146；tests/parser/test_ssa_state_maps.c:230；tests/parser/test_ssa_state_maps.c:235；tests/parser/test_ssa_state_maps.c:237；tests/parser/test_ssa_state_maps.c:843；tests/parser/test_ssa_state_maps.c:845；tests/parser/test_ssa_state_maps.c:1198；tests/parser/test_ssa_state_maps.c:1200；tests/parser/test_ssa_state_maps.c:1202；tests/parser/test_ssa_state_maps.c:1204；tests/parser/test_ssa_retained_builder_cfg.c:100；tests/parser/test_ssa_deopt_aggregates.c:116。
 */

/**
 * @fn static TZrUInt32 zr_state_map_containing_block(const SZrExecIrFunction *function, TZrUInt32 instructionId)
 *
 * ## Summary
 * 按 function 的 block 数组顺序查找覆盖指定 1 基 instruction ID 的首个基本块。
 *
 * ## Parameters
 * @param[in] function 提供 instruction 数量和 block 数组的函数记录。
 * @param[in] instructionId 1 基指令 ID；函数内部以 instructionId - 1 转为 0 基下标。
 *
 * ## Returns
 * @return 找到覆盖该 instruction 下标的 block 时返回 block ID；函数为空、ID 为零或超出 instructionCount、blocks 为空或没有覆盖区间时返回 ZR_EXEC_IR_BLOCK_ID_INVALID。
 *
 * ## Constraints and lifecycle
 * 本函数只按每个 block 的 instructionRange 做区间查找，不验证整个 CFG，也不证明非空 blocks 指针指向足够且可读的分配。若多个 block 范围匹配，返回数组中的第一个。
 *
 * ## References
 * 诊断 block 回填调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:51、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:64；handler 查找调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:422；诊断 setter 定义：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:36。
 */

/**
 * @fn static void zr_state_map_set_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry, TZrUInt32 instructionId, TZrUInt32 sourceId, TZrUInt64 expected, TZrUInt64 actual)
 *
 * ## Summary
 * 写入 StateMap 验证或物化路径显式提供的诊断字段，并在缺少身份时用 entry 回填。
 *
 * ## Parameters
 * @param[out] diagnostic 可为空；非空时接收本函数写入的诊断字段。
 * @param[in] code 本次失败的诊断代码。
 * @param[in] function 可为空；非空时提供 functionToken 和 block 查找所需的指令/块数据。
 * @param[in] entry 可为空；非空时用于回填未指定的 instructionId、sourceId 与 blockId。
 * @param[in] instructionId 显式 instruction ID；零值可由 entry 补足。
 * @param[in] sourceId 显式 source ID；零值可由 entry 补足。
 * @param[in] expected 依调用点语义传入的预期值；低 32 位写入 expectedVersion，完整 64 位写入 expectedHash。
 * @param[in] actual 依调用点语义传入的实际值；低 32 位写入 actualVersion，完整 64 位写入 actualHash。
 *
 * ## Returns
 * 无 C 返回值；diagnostic 为空时无操作，非空时只写代码、身份及 expected/actual 字段。
 *
 * ## Constraints and lifecycle
 * 本函数不清零诊断中未写字段；需要空白输出时调用方须先调用 zr_state_map_clear_diagnostic。expected/actual 的业务含义由调用点决定，不能解释为一种固定版本比较。function 的 block 查找数据只在同步调用期间读取；diagnostic 必须是独立可写输出。
 *
 * ## References
 * block 查找 helper：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:17；完整直接调用点：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:190；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:200；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:219；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:230；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:257；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:268；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:279；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:288；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:356；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:371；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:403；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:512；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:524；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:535；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:545；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:567；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:579；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:586；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:596；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:605；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:629；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:639；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:648；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:677；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:691；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:698；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:705；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:712；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:721；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:743；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:751；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:765；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:774；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:789；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:799。
 */

/**
 * @fn static TZrBool zr_state_map_range_valid(SZrExecIrRange range, TZrUInt32 count)
 *
 * ## Summary
 * 检查 range 是否落在 count 个元素组成的半开区间内。
 *
 * ## Parameters
 * @param[in] range 待验证的 start/count 描述符。
 * @param[in] count 被引用池或数组的有效元素数量。
 *
 * ## Returns
 * @return 仅当 range.start <= count 且 range.count <= count - range.start 时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 第二项使用减法而非 start + range.count，避免校验本身发生无符号加法溢出；允许零长度范围从 count 位置开始。本函数只检查整数边界，不校验指针、底层分配大小或内容可读性。
 *
 * ## References
 * function block 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:210、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:212、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:214、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:216、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:217、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:228；value/deopt/handler/entry 范围调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:255、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:365、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:369、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:425、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:501、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:502。
 */

/**
 * @fn static TZrBool zr_state_map_function_values_valid(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在分析 StateMap 前检查 function 的值表、IR 数组描述符、逻辑 ID、枚举和 block 子范围。
 *
 * ## Parameters
 * @param[in] function 待检查的 function；可为空，空值被显式拒绝并返回 ZR_FALSE。
 * @param[out] diagnostic 可为空；发现结构错误时接收 STATE_MAP_INVALID 及相关 identity。
 *
 * ## Returns
 * @return function 非空、function.id 有效且 functionToken 非零，所检查的计数不超过容量、非空数组具有所需指针、value/block ID 连续有效、枚举有效且各 block 子范围均在对应池计数内时返回 ZR_TRUE；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 检查 value、instruction、block、operand、result、phi、phiIncoming、predecessor、successor、deoptState 与 deoptValue 的 count/capacity 及非空指针配对；value 与 block ID 均要求按数组位置从 1 连续编号。block 的 instruction、predecessor、successor、phi、effectPhi 和每个 memory-phi 区域均执行范围检查。本函数不检查 constants pool，也不证明任意非空指针实际指向足量或可读的分配。
 *
 * ## References
 * 唯一生产调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:572；空值及池指针检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:166、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:179；诊断填充 helper：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:36。
 */

/**
 * @fn static TZrBool zr_state_map_validate_value_range(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrRange range, TZrBool roots, TZrBool suspended, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 校验 entry 所选 live-value 或 root-value 池范围中的逻辑 value ID 及相应 ownership 限制。
 *
 * ## Parameters
 * @param[in] function 非空函数描述符，提供 valueCount 和按 valueId - 1 索引的可读 value ownership。
 * @param[in] map 非空映射描述符，提供 valuePool、rootPool 及其有效计数；选定范围为空时所选池可为空。
 * @param[in] entry 当前 checkpoint；可为空以省略部分诊断身份回填。
 * @param[in] range 要检查的池内区间；本 helper 自行验证其逻辑边界。
 * @param[in] roots 为 ZR_TRUE 时读取 rootPool 并要求 GC、UNIQUE 或 SHARED owner；否则读取 valuePool。
 * @param[in] suspended 为 ZR_TRUE 时拒绝 live 集合中的 BORROWED owner。
 * @param[out] diagnostic 可为空；范围越界、非空范围缺少池指针、ID 或 owner 规则失败时接收相应诊断。
 *
 * ## Returns
 * @return 逻辑 range 位于所选池计数内、非空范围具有非空池指针，且每个 ID 非 INVALID、不大于 function.valueCount 并满足所选 owner 规则时返回 ZR_TRUE；范围/池指针、ID 或 owner 不符合规则时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 本 helper 自行检查逻辑 range，并在 range.count 非零时拒绝空的所选池指针；调用方仍须保证非空 function/map 描述符及实际数组存储在同步调用期间稳定、真实可读。整数边界与空指针检查不证明实际分配 extent。本函数不复制、pin 或保活运行时对象；live/root 的唯一性与 roots 属于 live 的关系由相邻 helper 分别检查。
 *
 * ## References
 * live/root 两个调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:505、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:509；本 helper 的 range/空池失败分支：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:255、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:256、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:262；诊断填充 helper：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:36。
 */

/**
 * @fn static TZrBool zr_state_map_range_values_unique(const SZrExecIrStateMap *map, SZrExecIrRange range, TZrBool roots)
 *
 * ## Summary
 * 检查 map 所选池的 range 中没有重复 value ID。
 *
 * ## Parameters
 * @param[in] map 提供 valuePool 与 rootPool。
 * @param[in] range 已由外层检查过的池内区间。
 * @param[in] roots 为 ZR_TRUE 时选择 rootPool，否则选择 valuePool。
 *
 * ## Returns
 * @return range 中所有 ID 两两不同时返回 ZR_TRUE；有重复时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 两个池执行相同的重复规则；roots 参数只选择池，不改变规则。本 helper 不校验范围、池指针、ID 合法性，也不比较跨池重复或 owner 语义，调用方须保证选定 range 与池可读。
 *
 * ## References
 * valuePool 与 rootPool 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:521、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:522。
 */

/**
 * @fn static TZrBool zr_state_map_roots_are_live(const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry)
 *
 * ## Summary
 * 检查 entry 的每个 root value ID 都出现在同一 entry 的 live-value range 中。
 *
 * ## Parameters
 * @param[in] map 提供 live valuePool 与 rootPool。
 * @param[in] entry 提供待比较的 liveValues 与 rootValues range。
 *
 * ## Returns
 * @return 所有 root ID 都在该 entry 的 live range 中时返回 ZR_TRUE；否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 调用前须已验证两个 range 和对应池指针；本函数只做同一 entry 的集合包含检查，不验证 owner、跨entry关系或完整 StateMap 语义，也不设置诊断。
 *
 * ## References
 * 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:523；相邻两池唯一性检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:521、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:522。
 */

/**
 * @fn static TZrBool zr_state_map_entry_deopt_valid(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 将 entry 的 deoptId 与唯一匹配的 function deopt state、source/resume identity、live value 和 aggregate recipe 关联校验。
 *
 * ## Parameters
 * @param[in] function 提供 deopt states、deopt values 和 valueCount。
 * @param[in] map 提供 entry live-value range 与 valuePool。
 * @param[in] entry 待验证的单个 checkpoint。
 * @param[out] diagnostic 可为空；关联、range 或 value 不一致时接收 STATE_MAP_INVALID 或下层 aggregate 诊断。
 *
 * ## Returns
 * @return entry.deoptId 为零时返回 ZR_TRUE；否则仅在恰有一个匹配 deopt state、sourceId 相同、非零 resumeId 相同、deopt range 有效且其中每个 ID 都属于 entry live values，并且相关 aggregate recipe 值仍 live 时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 外层 ZrCore_ExecIr_ValidateStateMap 先校验全局 function/deopt/aggregate 结构及 map shape；本 helper只处理选中 entry 的引用关系。它不检查 constants pool，不拥有或释放 function/map 池；输入和嵌套数组须在同步调用期间稳定可读。
 *
 * ## References
 * 唯一调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504；aggregate-live 委托调用点：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:400。
 */

/**
 * @fn static TZrExecIrBlockId zr_state_map_expected_handler(const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 boundaryFlags)
 *
 * ## Summary
 * 从 instruction 所属 source block 的 successor 数组中选择第一个现存的 exception 或 cleanup block。
 *
 * ## Parameters
 * @param[in] function 提供 instruction/block/successor 结构的 function。
 * @param[in] instructionId 1 基 instruction ID，用于定位 source block。
 * @param[in] boundaryFlags 当前 entry 的 boundary flags；没有 THROW 时不推导 handler。
 *
 * ## Returns
 * @return 按 successor 数组顺序遇到首个带 EXCEPTION 或 CLEANUP 标志且可解析的 block 时返回其 ID；没有 THROW、输入/范围无效、source block 不存在或没有合格 successor 时返回 ZR_EXEC_IR_BLOCK_ID_INVALID。
 *
 * ## Constraints and lifecycle
 * EXCEPTION 与 CLEANUP 在本 helper 中没有额外优先级，数组顺序决定结果；本函数不要求一定存在 handler。function 的 blocks、successors 和嵌套范围须在同步调用期间有效可读。
 *
 * ## References
 * 结果的精确比较调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:455；source block 查找：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:422。
 */

/**
 * @fn static TZrBool zr_state_map_entry_handler_valid(const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry)
 *
 * ## Summary
 * 检查 entry.handlerBlockId 与其 THROW boundary 所推导的 expected handler 一致。
 *
 * ## Parameters
 * @param[in] function 提供所属 block 与 successor 数组的 function。
 * @param[in] entry 提供 instructionId、boundaryFlags 与记录的 handlerBlockId。
 *
 * ## Returns
 * @return entry 没有 THROW 时仅当 handlerBlockId 为 INVALID 返回 ZR_TRUE；有 THROW 时仅当 recorded ID 等于 zr_state_map_expected_handler 的结果返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 当有 THROW 但找不到合格 successor 时 expected 仍是 INVALID，所以 INVALID/INVALID 会通过；本 helper 不附加“必须存在handler”的规则。调用方须保证 entry 和 function CFG 数据可读。
 *
 * ## References
 * expected handler helper 定义：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:410；推导调用与相等比较：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:455、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:457；唯一外层调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:491。
 */

/**
 * @fn static TZrBool zr_state_map_entry_boundary_valid(const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry)
 *
 * ## Summary
 * 重新计算 entry 指向 instruction 的 boundary flags，并要求与记录值完全一致。
 *
 * ## Parameters
 * @param[in] function 提供 instruction 数组。
 * @param[in] entry 提供 1 基 instructionId 与 boundaryFlags。
 *
 * ## Returns
 * @return instructionId 非零且在 instructionCount 内、StateMapBoundaryFlags 能计算出非零边界并与 entry.boundaryFlags 相等时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 本 helper 只比较 boundaryFlags，不比较 exceptionState；后者由 zr_state_map_entry_valid 单独比对。instruction 数组须稳定可读。
 *
 * ## References
 * BoundaryFlags 定义：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:91；计算调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:467；本 helper 外层调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:490；exceptionState 的独立比较：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:486。
 */

/**
 * @fn static TZrBool zr_state_map_entry_valid(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在当前 owner 与 liveness scratch 上验证一个已提供 StateMap entry。
 *
 * ## Parameters
 * @param[in] function 经外层基础结构检查的 function。
 * @param[in] ownership 当前 function 的 owner-analysis 结果。
 * @param[in] liveness 当前 function 的 StateMap liveness 结果。
 * @param[in] map 待检查的 StateMap 及其 value/root 池。
 * @param[in] entry 待检查的单个 checkpoint。
 * @param[out] diagnostic 可为空；子校验未设置具体代码时补充 STATE_MAP_INVALID。
 *
 * ## Returns
 * @return entry identity/phase、boundary/exception/handler、cleanup/deopt、live/root ranges、owner/liveness 关系及 instruction source/effect/deopt 关联均满足当前实现规则时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 本 helper 只验证传入 entry，不要求 StateMap 覆盖所有潜在边界；规则还包括 exceptionState 等于 boundary exception mask、各 phase 对应 cleanupState 为 0/1/2，以及 DEOPT boundary 必须有非零 deoptId。跨entry身份约束由 ZrCore_ExecIr_ValidateStateMap 的后续pairwise阶段检查。ownership 与 liveness 是由外层构建并释放的借用scratch，本函数不取得所有权。它组合调用已分列的 boundary、handler、deopt、owner、range、unique 与 roots-live 检查。
 *
 * ## References
 * 唯一调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:621；边界/handler/owner/deopt/value 子检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:490、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:491、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:505、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:509；唯一性/root-live 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:521、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:522、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:523。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ValidateStateMap(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 同步验证显式 StateMap 与 function 当前 identity、结构、owner/liveness 及已提供 checkpoint 的一致性。
 *
 * ## Parameters
 * @param[in] function 验证基准 function；可为空，空值返回 ZR_FALSE。
 * @param[in] map 可为空的显式 StateMap；空值返回 ZR_FALSE，非空时可不同于 function.stateMap，但 identity 必须匹配。
 * @param[out] diagnostic 可为空；非空时入口在读取输入前先清零，失败时记录可得到的阶段诊断。
 *
 * ## Returns
 * @return function/map 非空，基础形状和 token/signature/generation 匹配、function/deopt 结构有效、owner/liveness 分析成功且每个已提供 entry 与 pairwise identity 规则均有效时返回 ZR_TRUE；否则返回 ZR_FALSE。任一输入为空时，在非空 diagnostic 中记录 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 本接口验证 map 中已提供的 entries，但不要求每个可能的 boundary 都有 entry。function、map 及其嵌套数组仅同步只读借用；调用期间必须稳定且可读，本接口不持有、复制或内部同步输入。诊断必须独立可写并且不得与 function、map 或任一嵌套存储别名，因为入口先清零。
 *
 * ## Resources and failure
 * 验证会构建临时 owner-analysis 与 liveness scratch，可能分配内存。OwnerAnalysisBuild 若自身失败，由该builder清理部分scratch后返回；之后若 liveness/entry/pairwise检查失败，统一路径释放两种 scratch；成功路径也释放。false可来自参数、结构、身份、deopt、owner/liveness、entry或资源失败，诊断码随实际阶段而异。count/capacity和整数range检查不证明任意非空地址可读。
 *
 * ## References
 * 生产调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:910。测试直接调用：tests/parser/test_ssa_sccp_phi_copy_edges.c:256；tests/parser/test_ssa_retained_builder_cfg.c:60；tests/parser/test_ssa_licm_scalar_context.inc:136；tests/parser/test_ssa_licm_scalar_context.inc:202。这些引用不授予测试单元审查信用。
 */

/**
 * @anchor zr_state_map_validation_checkpoint_identity_rules
 *
 * ## Summary
 * 规定 ValidateStateMap 对 map 已提供 entries 执行的 pairwise identity 一致性约束。
 *
 * ## Parameters
 * 此代码块没有独立参数；比较对象是当前 entry 与先前已验证的 entries。
 *
 * ## Returns
 * 此代码块没有独立返回值；冲突时设置 STATE_MAP_INVALID 并跳转到外层统一 scratch 清理路径。
 *
 * ## Constraints and lifecycle
 * 若两项 instructionId 相同，则 sourceId 与 resumeId 必须都相同；若 resumeId 相同，则 instructionId 与 sourceId 必须都相同；完全相同的 (sourceId, resumeId, phase) 被拒绝。相同 sourceId/resumeId 搭配不同 phase 可以共存；sourceId 本身不要求全局唯一。规则仅覆盖 map 中显式提供的 entries。
 *
 * ## References
 * same-instruction 关系：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:626；same-resume 关系：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:636；重复 triple：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:646；失败统一释放：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661。
 */

/**
 * @fn static TZrBool zr_state_map_size_valid(TZrUInt32 count, size_t elementSize)
 *
 * ## Summary
 * 在物化候选的 live value、root 和 owner-state 池分配前，检查 count 与元素大小的乘积能否用 size_t 表示，避免字节数计算截断后分配过小。
 *
 * ## Parameters
 * @param[in] count 待分配的元素数量；数量为零时仍要求 elementSize 非零。
 * @param[in] elementSize 单个元素的字节数；零表示无效大小。
 *
 * ## Returns
 * @return elementSize 非零且 count 不超过 SIZE_MAX / elementSize 时为 ZR_TRUE，否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 这是纯算术检查，不分配、释放或访问存储，也不证明实际地址、容量、对齐或可读性。没有资源转移或内部同步。
 * 本定义只供本翻译单元的三处候选池分配调用；storage 翻译单元中的同名 static helper 是独立定义。
 *
 * ## References
 * live value 分配前检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:741；root 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:763；owner-state 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:786。
 * 另一翻译单元的同名独立定义：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:7。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_MaterializeState(const SZrExecIrResumeRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为解释器恢复及对象物化选择一个逻辑 checkpoint。先验证整个 function/map 对，再按 sourceId、resumeId、phase 查找 entry 并检查可选请求身份；target 非空时在局部候选中构造逻辑值、root、owner-state 及 deopt recipe 池，全部成功后一次替换调用方结果。
 * target 为空时只走 metadata/owner-witness 验证路径，不创建或发布物化输出；该路径仍可能由 map 验证和 witness 分析分配临时存储。
 *
 * ## Parameters
 * @param[in] request 同步借用的请求描述符；NULL、NULL function 或未知 phase 返回 ZR_FALSE。非空指针及将被读取的嵌套对象必须真实有效。map 为 NULL 时使用 function->stateMap；target 的可写借用及所有权约束见下节。
 * @param[out] diagnostic 可为 NULL；否则须为独立、可写的诊断对象。本函数在检查 request 前先清零它，失败后可写入所命中阶段的诊断；不保存该指针。
 *
 * ## Returns
 * @return ZR_TRUE 表示所选 checkpoint 及适用的 owner witness 验证成功；target 非空时还表示所有候选池与 recipe 已准备并提交。ZR_FALSE 表示参数、map、entry 查找、可选身份、target、尺寸、分配、owner witness 或 recipe 准备的某一步失败。按下述独立存储和有效所有权前提，失败不改变旧 target；diagnostic 不属于该事务。
 *
 * ## Constraints and lifecycle
 * 调用顺序先于请求身份检查：ValidateStateMap 必须先成功，再查找 entry，最后依次检查非零的 functionToken、generation、signatureHash；任一请求身份字段为零时仅跳过该项额外比较，不跳过 function/map 的一致性验证。
 * request、function、所选 map 及可选 ownerStates 只在本次同步调用期间借用，不取得其所有权。输入及其嵌套存储在读取期间必须稳定；调用方负责同步，函数没有保护并发修改或释放的锁。
 * 非空 ownerStates 按 valueId - 1 索引，ownerStateCount 必须等于 function->valueCount；空指针与零数量必须成对。具体 witness 必须落在该 checkpoint 的静态 owner mask 内，并与非 CONDITIONAL 的静态状态一致。非空 target 所选 entry 中的 CONDITIONAL owner 状态需要具体 witness；空 target 可以在没有 witness 时只验证此类 metadata。
 * 非空 target 必须是已初始化的可写记录。旧的五个池须满足数量/容量/指针对应关系、所检查的区间分离关系，并各自来自可由 C free 释放的独立分配。target 校验不证明分配来源或实际地址可读性，也不检测 target 记录本身对 request、function/map 等输入的别名。
 * request、diagnostic 与 target 记录须相互独立；target 记录及旧池还须与 request、function/map、被读池和 owner witness 分离，这是调用方前提而非 helper 已证明的全部别名关系。diagnostic 不得与输入、owner witness、target 记录或其池重叠。不得把将继续使用的请求或输入置于提交时将释放的旧 target 池内；本接口不承诺任意输出别名安全。
 * live value、root、owner-state 池按该顺序分配；首池分配或尺寸失败前尚无候选池，后续失败释放已取得的候选存储。owner 解析可收缩有效 rootCount；容量仍记录已分配空间。owner 解析或 aggregate 准备失败时，由此函数释放整个候选。
 * 唯一提交点先释放旧 target 的五个池，再把完整候选写入同一记录。成功后旧池的全部借用失效；新池所有权交给 target，调用方最终调用 ZrCore_ExecIr_MaterializedStateFree。失败后可保留旧结果并按具体诊断决定是否重试。
 * values/roots 是逻辑 value ID，ownerStates 是状态元数据，aggregate/field 是复制并重排字段区间的逻辑 recipe；它们不是运行时对象或 GC root 注册，不保活原 function/map 或运行时对象。ID 的解释仍依赖对应函数和版本语义。
 *
 * ## References
 * 公开接口：zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:148；完整 map 验证及 scratch 清理：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556；entry 选择：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:688。
 * target 约束检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308；候选初始化与五池释放：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:218；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:224。
 * witness 解析、临时分析清理和 root 筛选：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:85；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:100；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:121；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:127；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:143。
 * aggregate 准备及其两处 malloc：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:361；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:387；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:391；失败回收与提交：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:811；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 * 生产调用为解释器的验证/物化两阶段及对象物化：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:31；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:80；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684。
 * 未知请求 phase 的有限早退断言：tests/parser/test_ssa_state_maps.c:242；root witness 与旧 target 记录保持用例：tests/parser/test_ssa_conditional_cleanup.c:268；tests/parser/test_ssa_conditional_cleanup.c:293。pass-manager 的调用在测试 include 中：tests/parser/ssa_pass_manager_state_maps_cases.inc:161。这些引用不授予相关测试的新审查信用。
 */
