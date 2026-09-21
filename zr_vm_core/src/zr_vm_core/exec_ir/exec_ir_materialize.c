#include "zr_vm_core/exec_ir_state_map.h"

#include <limits.h>
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

static TZrBool zr_state_map_copy_array(void **destination,
                                       TZrUInt32 *destinationCapacity,
                                       const void *source,
                                       TZrUInt32 count,
                                       size_t elementSize) {
    void *copy = ZR_NULL;

    if (destination == ZR_NULL || destinationCapacity == ZR_NULL ||
        !zr_state_map_size_valid(count, elementSize) ||
        (count != 0u && source == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        copy = malloc((size_t)count * elementSize);
        if (copy == ZR_NULL) {
            return ZR_FALSE;
        }
        memcpy(copy, source, (size_t)count * elementSize);
    }
    *destination = copy;
    *destinationCapacity = count;
    return ZR_TRUE;
}

void ZrCore_ExecIr_StateMapInit(SZrExecIrStateMap *map) {
    if (map != ZR_NULL) {
        memset(map, 0, sizeof(*map));
    }
}

void ZrCore_ExecIr_StateMapFree(SZrExecIrStateMap *map) {
    if (map != ZR_NULL) {
        free(map->entries);
        free(map->valuePool);
        free(map->rootPool);
        free(map->ownerStatePool);
        memset(map, 0, sizeof(*map));
    }
}

TZrBool ZrCore_ExecIr_StateMapClone(const SZrExecIrStateMap *source,
                                    SZrExecIrStateMap *destination) {
    SZrExecIrStateMap clone;

    if (source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    if (source == destination) {
        return ZR_TRUE;
    }
    if (source->entryCount > source->entryCapacity ||
        source->valueCount > source->valueCapacity ||
        source->rootCount > source->rootCapacity ||
        source->ownerStateCount > source->ownerStateCapacity ||
        (source->entryCount != 0u && source->entries == ZR_NULL) ||
        (source->valueCount != 0u && source->valuePool == ZR_NULL) ||
        (source->rootCount != 0u && source->rootPool == ZR_NULL) ||
        (source->ownerStateCount != 0u && source->ownerStatePool == ZR_NULL)) {
        return ZR_FALSE;
    }

    ZrCore_ExecIr_StateMapInit(&clone);
    clone.functionToken = source->functionToken;
    clone.signatureHash = source->signatureHash;
    clone.generation = source->generation;
    if (!zr_state_map_copy_array((void **)&clone.entries, &clone.entryCapacity,
                                  source->entries, source->entryCount,
                                  sizeof(*clone.entries)) ||
        !zr_state_map_copy_array((void **)&clone.valuePool, &clone.valueCapacity,
                                  source->valuePool, source->valueCount,
                                  sizeof(*clone.valuePool)) ||
        !zr_state_map_copy_array((void **)&clone.rootPool, &clone.rootCapacity,
                                  source->rootPool, source->rootCount,
                                  sizeof(*clone.rootPool)) ||
        !zr_state_map_copy_array((void **)&clone.ownerStatePool,
                                  &clone.ownerStateCapacity,
                                  source->ownerStatePool,
                                  source->ownerStateCount,
                                  sizeof(*clone.ownerStatePool))) {
        ZrCore_ExecIr_StateMapFree(&clone);
        return ZR_FALSE;
    }
    clone.entryCount = source->entryCount;
    clone.valueCount = source->valueCount;
    clone.rootCount = source->rootCount;
    clone.ownerStateCount = source->ownerStateCount;
    ZrCore_ExecIr_StateMapFree(destination);
    *destination = clone;
    return ZR_TRUE;
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

void ZrCore_ExecIr_MaterializedStateInit(SZrExecIrMaterializedState *state) {
    if (state != ZR_NULL) {
        memset(state, 0, sizeof(*state));
    }
}

void ZrCore_ExecIr_MaterializedStateFree(SZrExecIrMaterializedState *state) {
    if (state != ZR_NULL) {
        free(state->values);
        free(state->roots);
        free(state->ownerStates);
        memset(state, 0, sizeof(*state));
    }
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
            value->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
            value->nullability >= ZR_EXEC_IR_NULLABILITY_COUNT) {
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

static TZrBool zr_state_map_owner_range_valid(const SZrExecIrFunction *function,
                                              const SZrExecIrStateMap *map,
                                              const SZrExecIrStateMapEntry *entry,
                                              SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (!zr_state_map_range_valid(entry->ownerStates, map->ownerStateCount) ||
        entry->ownerStates.count != entry->liveValues.count ||
        (entry->ownerStates.count != 0u && map->ownerStatePool == ZR_NULL)) {
        zr_state_map_set_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                    function, entry, entry->instructionId, entry->sourceId,
                                    entry->liveValues.count, entry->ownerStates.count);
        return ZR_FALSE;
    }
    for (index = entry->ownerStates.start;
         index < entry->ownerStates.start + entry->ownerStates.count;
         ++index) {
        if (map->ownerStatePool[index] >= ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId, entry->sourceId,
                                        ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT,
                                        map->ownerStatePool[index]);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_entry_deopt_valid(const SZrExecIrFunction *function,
                                              const SZrExecIrStateMapEntry *entry,
                                              SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;

    if (entry->deoptId == 0u) {
        return ZR_TRUE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (state->deoptId == entry->deoptId) {
            if (!zr_state_map_range_valid(state->valueRange, function->deoptValueCount)) {
                break;
            }
            if (state->valueRange.count != 0u && function->deoptValues == ZR_NULL) {
                break;
            }
            {
                TZrUInt32 valueIndex;
                for (valueIndex = state->valueRange.start;
                     valueIndex < state->valueRange.start + state->valueRange.count;
                     ++valueIndex) {
                    if (function->deoptValues[valueIndex] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                        function->deoptValues[valueIndex] > function->valueCount) {
                        break;
                    }
                }
                if (valueIndex != state->valueRange.start + state->valueRange.count) {
                    break;
                }
            }
            return ZR_TRUE;
        }
    }
    zr_state_map_set_diagnostic(diagnostic,
                                ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                function, entry, entry->instructionId, entry->sourceId,
                                entry->deoptId, 0u);
    return ZR_FALSE;
}

static TZrBool zr_state_map_entry_valid(const SZrExecIrFunction *function,
                                        const SZrExecIrStateMap *map,
                                        const SZrExecIrStateMapEntry *entry,
                                        SZrExecIrDiagnostic *diagnostic) {
    if (entry == ZR_NULL || entry->sourceId == 0u ||
        !zr_state_map_phase_valid(entry->phase) ||
        entry->resumeId == 0u ||
        entry->boundaryFlags == 0u ||
        (entry->boundaryFlags & ~ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK) != 0u ||
        entry->instructionId == 0u || entry->instructionId > function->instructionCount ||
        (entry->handlerBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID &&
         entry->handlerBlockId > function->blockCount) ||
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
        !zr_state_map_owner_range_valid(function, map, entry, diagnostic) ||
        !zr_state_map_entry_deopt_valid(function, entry, diagnostic) ||
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
        if (entry->sourceId != 0u && instruction->sourceId != 0u &&
            entry->sourceId != instruction->sourceId) {
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                        function, entry, entry->instructionId,
                                        entry->sourceId, instruction->sourceId,
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

    if (!zr_state_map_function_values_valid(function, diagnostic) || map == ZR_NULL ||
        map->entryCount > map->entryCapacity || map->valueCount > map->valueCapacity ||
        map->rootCount > map->rootCapacity ||
        map->ownerStateCount > map->ownerStateCapacity ||
        (map->entryCount != 0u && map->entries == ZR_NULL) ||
        (map->valueCount != 0u && map->valuePool == ZR_NULL) ||
        (map->rootCount != 0u && map->rootPool == ZR_NULL) ||
        (map->ownerStateCount != 0u && map->ownerStatePool == ZR_NULL) ||
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
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if (!zr_state_map_entry_valid(function, map, entry, diagnostic)) {
            return ZR_FALSE;
        }
        for (other = 0u; other < index; ++other) {
            const SZrExecIrStateMapEntry *prior = &map->entries[other];
            if (prior->sourceId == entry->sourceId &&
                prior->resumeId == entry->resumeId && prior->phase == entry->phase) {
                zr_state_map_set_diagnostic(diagnostic,
                                            ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, entry, entry->instructionId,
                                            entry->sourceId, 0u, entry->resumeId);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_target_valid(const SZrExecIrMaterializedState *target,
                                         const SZrExecIrStateMap *map) {
    if (target == ZR_NULL || target->valueCount > target->valueCapacity ||
        target->rootCount > target->rootCapacity ||
        target->ownerStateCount > target->ownerStateCapacity ||
        (target->valueCount != 0u && target->values == ZR_NULL) ||
        (target->rootCount != 0u && target->roots == ZR_NULL) ||
        (target->ownerStateCount != 0u && target->ownerStates == ZR_NULL) ||
        (target->valueCapacity == 0u && target->values != ZR_NULL) ||
        (target->rootCapacity == 0u && target->roots != ZR_NULL) ||
        (target->ownerStateCapacity == 0u && target->ownerStates != ZR_NULL)) {
        return ZR_FALSE;
    }
    if ((target->values != ZR_NULL && target->values == target->roots) ||
        (target->values != ZR_NULL && target->values == target->ownerStates) ||
        (target->roots != ZR_NULL && target->roots == target->ownerStates)) {
        return ZR_FALSE;
    }
    /* A target must not alias a map-owned pool: commit frees the old target. */
    return (TZrBool)(!(target->values != ZR_NULL &&
                       (target->values == map->valuePool ||
                        target->values == map->rootPool ||
                        target->values == map->ownerStatePool)) &&
                     !(target->roots != ZR_NULL &&
                       (target->roots == map->valuePool ||
                        target->roots == map->rootPool ||
                        target->roots == map->ownerStatePool)) &&
                     !(target->ownerStates != ZR_NULL &&
                       (target->ownerStates == map->valuePool ||
                        target->ownerStates == map->rootPool ||
                        target->ownerStates == map->ownerStatePool)));
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
    if (!zr_state_map_target_valid(request->target, map)) {
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

    /* The single commit point is after every validation and allocation. */
    ZrCore_ExecIr_MaterializedStateFree(request->target);
    *request->target = prepared;
    return ZR_TRUE;
}
