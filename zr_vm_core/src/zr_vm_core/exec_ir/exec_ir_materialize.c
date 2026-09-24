#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/exec_ir_owner_state.h"
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
    if ((EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_DROP) {
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
            !zr_state_map_range_valid(block->phis, function->phiCount)) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, ZR_NULL, 0u, block->id,
                                        index + 1u, block->id);
            return ZR_FALSE;
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

static TZrBool zr_state_map_roots_match_live(
        const SZrExecIrFunction *function,
        const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry) {
    TZrUInt32 liveIndex;

    for (liveIndex = 0u;
         liveIndex < entry->liveValues.count;
         ++liveIndex) {
        TZrExecIrValueId valueId = map->valuePool[entry->liveValues.start + liveIndex];
        EZrExecIrOwnership ownership = function->values[valueId - 1u].ownership;
        TZrBool managed = (TZrBool)(ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
                                    ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
                                    ownership == ZR_EXEC_IR_OWNERSHIP_SHARED);
        TZrUInt32 rootIndex;
        TZrBool found = ZR_FALSE;

        for (rootIndex = 0u; rootIndex < entry->rootValues.count; ++rootIndex) {
            if (map->rootPool[entry->rootValues.start + rootIndex] == valueId) {
                found = ZR_TRUE;
                break;
            }
        }
        if (managed != found) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}


static TZrBool zr_state_map_owner_range_valid(const SZrExecIrFunction *function,
                                              const SZrExecIrOwnerAnalysis *ownership,
                                              const SZrExecIrStateMap *map,
                                              const SZrExecIrStateMapEntry *entry,
                                              SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (!zr_state_map_range_valid(entry->ownerStates, map->ownerStateCount) ||
        entry->ownerStates.count != entry->liveValues.count ||
        (entry->ownerStates.count != 0u && map->ownerStatePool == ZR_NULL) ||
        !zr_state_map_range_valid(entry->liveValues, map->valueCount) ||
        (entry->liveValues.count != 0u && map->valuePool == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, entry, entry->instructionId, entry->sourceId,
                                    entry->liveValues.count, entry->ownerStates.count);
        return ZR_FALSE;
    }
    for (index = entry->ownerStates.start;
         index < entry->ownerStates.start + entry->ownerStates.count;
         ++index) {
        TZrUInt32 liveIndex = entry->liveValues.start +
                              (index - entry->ownerStates.start);
        TZrExecIrValueId valueId = map->valuePool[liveIndex];
        EZrExecIrStateMapOwnerState expected =
            ZrCore_ExecIr_OwnerStateAt(ownership, entry->instructionId,
                                       valueId, entry->phase);
        if (map->ownerStatePool[index] >= ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId, entry->sourceId,
                                        ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT,
                                        map->ownerStatePool[index]);
            return ZR_FALSE;
        }
        if ((expected != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED &&
             expected != ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) ||
            map->ownerStatePool[index] != (TZrUInt32)expected) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, (TZrUInt32)expected,
                                        map->ownerStatePool[index]);
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
        !zr_state_map_owner_range_valid(function, ownership, map, entry, diagnostic) ||
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
        !zr_state_map_roots_are_live(map, entry) ||
        !zr_state_map_roots_match_live(function, map, entry)) {
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

static TZrBool zr_state_map_validate(const SZrExecIrFunction *function,
                                     const SZrExecIrStateMap *map,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 other;
    SZrExecIrOwnerAnalysis ownership = {0};

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
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if (!zr_state_map_entry_valid(function, &ownership, map, entry, diagnostic)) {
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
    return ZR_TRUE;
invalid:
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
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
        request->target == ZR_NULL || !zr_state_map_phase_valid(request->phase)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    request != ZR_NULL ? request->function : ZR_NULL,
                                    ZR_NULL, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    function = request->function;
    map = request->map != ZR_NULL ? request->map : function->stateMap;
    if (!zr_state_map_validate(function, map, diagnostic)) {
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

    if (!zr_exec_ir_deopt_aggregates_prepare(function, entry, &prepared, diagnostic)) {
        ZrCore_ExecIr_MaterializedStateFree(&prepared);
        return ZR_FALSE;
    }

    /* The single commit point is after every validation and allocation. */
    ZrCore_ExecIr_MaterializedStateFree(request->target);
    *request->target = prepared;
    return ZR_TRUE;
}
