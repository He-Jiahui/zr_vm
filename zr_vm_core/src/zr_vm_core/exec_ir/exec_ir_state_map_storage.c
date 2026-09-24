#include "exec_ir_state_map_storage.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static TZrBool zr_state_map_size_valid(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
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

typedef struct SZrStateMapStorageSpan {
    const void *storage;
    TZrUInt32 capacity;
    size_t elementSize;
} SZrStateMapStorageSpan;

static TZrBool zr_state_map_storage_spans_overlap(
        const SZrStateMapStorageSpan *left,
        const SZrStateMapStorageSpan *right) {
    size_t leftBytes;
    size_t rightBytes;
    uintptr_t leftStart;
    uintptr_t rightStart;
    uintptr_t leftEnd;
    uintptr_t rightEnd;

    if (left->capacity == 0u || right->capacity == 0u) {
        return ZR_FALSE;
    }
    if (left->storage == ZR_NULL || right->storage == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_state_map_size_valid(left->capacity, left->elementSize) ||
        !zr_state_map_size_valid(right->capacity, right->elementSize)) {
        return ZR_TRUE;
    }
    leftBytes = (size_t)left->capacity * left->elementSize;
    rightBytes = (size_t)right->capacity * right->elementSize;
    leftStart = (uintptr_t)left->storage;
    rightStart = (uintptr_t)right->storage;
    if (leftBytes > (size_t)(UINTPTR_MAX - leftStart) ||
        rightBytes > (size_t)(UINTPTR_MAX - rightStart)) {
        return ZR_TRUE;
    }
    leftEnd = leftStart + (uintptr_t)leftBytes;
    rightEnd = rightStart + (uintptr_t)rightBytes;
    return (TZrBool)(leftStart < rightEnd && rightStart < leftEnd);
}

TZrBool zr_state_map_storage_shape_valid(const SZrExecIrStateMap *map) {
    SZrStateMapStorageSpan storage[4];
    TZrUInt32 index;
    TZrUInt32 other;

    if (map == ZR_NULL || map->entryCount > map->entryCapacity ||
        map->valueCount > map->valueCapacity ||
        map->rootCount > map->rootCapacity ||
        map->ownerStateCount > map->ownerStateCapacity ||
        ((map->entryCapacity == 0u) != (map->entries == ZR_NULL)) ||
        ((map->valueCapacity == 0u) != (map->valuePool == ZR_NULL)) ||
        ((map->rootCapacity == 0u) != (map->rootPool == ZR_NULL)) ||
        ((map->ownerStateCapacity == 0u) !=
             (map->ownerStatePool == ZR_NULL))) {
        return ZR_FALSE;
    }
    storage[0].storage = map->entries;
    storage[0].capacity = map->entryCapacity;
    storage[0].elementSize = sizeof(*map->entries);
    storage[1].storage = map->valuePool;
    storage[1].capacity = map->valueCapacity;
    storage[1].elementSize = sizeof(*map->valuePool);
    storage[2].storage = map->rootPool;
    storage[2].capacity = map->rootCapacity;
    storage[2].elementSize = sizeof(*map->rootPool);
    storage[3].storage = map->ownerStatePool;
    storage[3].capacity = map->ownerStateCapacity;
    storage[3].elementSize = sizeof(*map->ownerStatePool);
    for (index = 0u; index < 4u; ++index) {
        if (!zr_state_map_size_valid(storage[index].capacity, storage[index].elementSize)) {
            return ZR_FALSE;
        }
        for (other = index + 1u; other < 4u; ++other) {
            if (zr_state_map_storage_spans_overlap(&storage[index],
                                                   &storage[other])) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_storage_is_shared(const SZrExecIrStateMap *left,
                                               const SZrExecIrStateMap *right) {
    const SZrStateMapStorageSpan leftStorage[] = {
        {left->entries, left->entryCapacity, sizeof(*left->entries)},
        {left->valuePool, left->valueCapacity, sizeof(*left->valuePool)},
        {left->rootPool, left->rootCapacity, sizeof(*left->rootPool)},
        {left->ownerStatePool, left->ownerStateCapacity,
         sizeof(*left->ownerStatePool)}
    };
    const SZrStateMapStorageSpan rightStorage[] = {
        {right->entries, right->entryCapacity, sizeof(*right->entries)},
        {right->valuePool, right->valueCapacity, sizeof(*right->valuePool)},
        {right->rootPool, right->rootCapacity, sizeof(*right->rootPool)},
        {right->ownerStatePool, right->ownerStateCapacity,
         sizeof(*right->ownerStatePool)}
    };
    TZrUInt32 leftIndex;
    TZrUInt32 rightIndex;

    for (leftIndex = 0u;
         leftIndex < (TZrUInt32)(sizeof(leftStorage) / sizeof(leftStorage[0]));
         ++leftIndex) {
        for (rightIndex = 0u;
             rightIndex < (TZrUInt32)(sizeof(rightStorage) /
                                      sizeof(rightStorage[0]));
             ++rightIndex) {
            if (zr_state_map_storage_spans_overlap(&leftStorage[leftIndex],
                                                   &rightStorage[rightIndex])) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
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
    if (!zr_state_map_storage_shape_valid(source) ||
        !zr_state_map_storage_shape_valid(destination) ||
        zr_state_map_storage_is_shared(source, destination)) {
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
        free(state->aggregates);
        free(state->aggregateFields);
        memset(state, 0, sizeof(*state));
    }
}


TZrBool zr_state_map_input_span_disjoint(
        const SZrExecIrFunction *function, const SZrExecIrStateMap *map,
        const void *storage, size_t bytes) {
    SZrStateMapStorageSpan candidate = {storage, bytes != 0u ? 1u : 0u, bytes};
    TZrUInt32 index;
    uintptr_t start = (uintptr_t)storage;
    if (function == ZR_NULL || map == ZR_NULL ||
        (bytes != 0u && (storage == ZR_NULL || start > UINTPTR_MAX - bytes)))
        return ZR_FALSE;
    {
        const SZrStateMapStorageSpan sourceStorage[] = {
            {map, 1u, sizeof(*map)},
            {function, 1u, sizeof(*function)},
            {map->entries, map->entryCapacity, sizeof(*map->entries)},
            {map->valuePool, map->valueCapacity, sizeof(*map->valuePool)},
            {map->rootPool, map->rootCapacity, sizeof(*map->rootPool)},
            {map->ownerStatePool, map->ownerStateCapacity, sizeof(*map->ownerStatePool)},
            {function->values, function->valueCapacity, sizeof(*function->values)},
            {function->instructions, function->instructionCapacity, sizeof(*function->instructions)},
            {function->blocks, function->blockCapacity, sizeof(*function->blocks)},
            {function->operandPool, function->operandCapacity, sizeof(*function->operandPool)},
            {function->resultPool, function->resultCapacity, sizeof(*function->resultPool)},
            {function->memoryTokenPool, function->memoryTokenCapacity, sizeof(*function->memoryTokenPool)},
            {function->phiPool, function->phiCapacity, sizeof(*function->phiPool)},
            {function->phiIncoming, function->phiIncomingCapacity, sizeof(*function->phiIncoming)},
            {function->predecessors, function->predecessorCapacity, sizeof(*function->predecessors)},
            {function->successors, function->successorCapacity, sizeof(*function->successors)},
            {function->gcRoots, function->gcRootCapacity, sizeof(*function->gcRoots)},
            {function->deoptStates, function->deoptStateCapacity, sizeof(*function->deoptStates)},
            {function->deoptValues, function->deoptValueCapacity, sizeof(*function->deoptValues)},
            {function->deoptAggregates, function->deoptAggregateCapacity, sizeof(*function->deoptAggregates)},
            {function->deoptAggregateFields, function->deoptAggregateFieldCapacity,
             sizeof(*function->deoptAggregateFields)},
            {function->sourceMaps, function->sourceMapCapacity, sizeof(*function->sourceMaps)},
            {function->frameLayout, 1u, sizeof(SZrExecIrFrameLayout)},
            {function->frameLayout != ZR_NULL ? function->frameLayout->slots : ZR_NULL,
             function->frameLayout != ZR_NULL ? function->frameLayout->slotCapacity : 0u,
             sizeof(SZrExecIrFrameSlot)},
            {function->gcMap, 1u, sizeof(SZrExecIrGcMap)},
            {function->gcMap != ZR_NULL ? function->gcMap->entries : ZR_NULL,
             function->gcMap != ZR_NULL ? function->gcMap->entryCapacity : 0u,
             sizeof(SZrExecIrGcMapEntry)},
            {function->gcMap != ZR_NULL ? function->gcMap->slotIndexPool : ZR_NULL,
             function->gcMap != ZR_NULL ? function->gcMap->slotIndexCapacity : 0u,
             sizeof(TZrUInt32)},
            {function->gcMap != ZR_NULL ? function->gcMap->inlineRefOffsetPool : ZR_NULL,
             function->gcMap != ZR_NULL ? function->gcMap->inlineRefOffsetCapacity : 0u,
             sizeof(TZrUInt32)},
            {function->stateMap, 1u, sizeof(SZrExecIrStateMap)},
            {function->stateMap != ZR_NULL ? function->stateMap->entries : ZR_NULL,
             function->stateMap != ZR_NULL ? function->stateMap->entryCapacity : 0u,
             sizeof(SZrExecIrStateMapEntry)},
            {function->stateMap != ZR_NULL ? function->stateMap->valuePool : ZR_NULL,
             function->stateMap != ZR_NULL ? function->stateMap->valueCapacity : 0u,
             sizeof(TZrExecIrValueId)},
            {function->stateMap != ZR_NULL ? function->stateMap->rootPool : ZR_NULL,
             function->stateMap != ZR_NULL ? function->stateMap->rootCapacity : 0u,
             sizeof(TZrExecIrValueId)},
            {function->stateMap != ZR_NULL ? function->stateMap->ownerStatePool : ZR_NULL,
             function->stateMap != ZR_NULL ? function->stateMap->ownerStateCapacity : 0u,
             sizeof(TZrUInt32)}
        };
        for (index = 0u; index < sizeof(sourceStorage) / sizeof(sourceStorage[0]); ++index) {
            if (!zr_state_map_size_valid(sourceStorage[index].capacity,
                                          sourceStorage[index].elementSize) ||
                zr_state_map_storage_spans_overlap(&candidate, &sourceStorage[index]))
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool zr_state_map_target_valid(const SZrExecIrMaterializedState *target,
                                    const SZrExecIrStateMap *map,
                                    const SZrExecIrFunction *function) {
    TZrUInt32 index, other;
    if (target == ZR_NULL || map == ZR_NULL || function == ZR_NULL ||
        target->valueCount > target->valueCapacity ||
        target->rootCount > target->rootCapacity ||
        target->ownerStateCount > target->ownerStateCapacity ||
        target->aggregateCount > target->aggregateCapacity ||
        target->aggregateFieldCount > target->aggregateFieldCapacity ||
        ((target->valueCapacity == 0u) != (target->values == ZR_NULL)) ||
        ((target->rootCapacity == 0u) != (target->roots == ZR_NULL)) ||
        ((target->ownerStateCapacity == 0u) != (target->ownerStates == ZR_NULL)) ||
        ((target->aggregateCapacity == 0u) != (target->aggregates == ZR_NULL)) ||
        ((target->aggregateFieldCapacity == 0u) != (target->aggregateFields == ZR_NULL))) {
        return ZR_FALSE;
    }
    {
        const SZrStateMapStorageSpan targetStorage[] = {
            {target->values, target->valueCapacity, sizeof(*target->values)},
            {target->roots, target->rootCapacity, sizeof(*target->roots)},
            {target->ownerStates, target->ownerStateCapacity, sizeof(*target->ownerStates)},
            {target->aggregates, target->aggregateCapacity, sizeof(*target->aggregates)},
            {target->aggregateFields, target->aggregateFieldCapacity, sizeof(*target->aggregateFields)}
        };
        /* Committing frees the old target. None of those allocations may own
         * any part of the function, source map or their recipe side tables. */
        const SZrStateMapStorageSpan record = {target, 1u, sizeof(*target)};
        for (index = 0u; index < sizeof(targetStorage) / sizeof(targetStorage[0]); ++index) {
            if (!zr_state_map_size_valid(targetStorage[index].capacity,
                                          targetStorage[index].elementSize)) return ZR_FALSE;
            for (other = index + 1u; other < sizeof(targetStorage) / sizeof(targetStorage[0]); ++other) {
                if (zr_state_map_storage_spans_overlap(&targetStorage[index], &targetStorage[other])) {
                    return ZR_FALSE;
                }
            }
            if (zr_state_map_storage_spans_overlap(&targetStorage[index], &record) ||
                !zr_state_map_input_span_disjoint(function, map, targetStorage[index].storage,
                        (size_t)targetStorage[index].capacity * targetStorage[index].elementSize))
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
