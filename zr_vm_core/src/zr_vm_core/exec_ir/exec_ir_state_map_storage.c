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

TZrBool ZrCore_ExecIr_StateMapStorageValid(const SZrExecIrStateMap *map) {
    return zr_state_map_storage_shape_valid(map);
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

/**
 * @fn static TZrBool zr_state_map_size_valid(TZrUInt32 count, size_t elementSize)
 *
 * ## Summary
 * 在计算 state-map 池的容量字节数前，检查元素大小非零且 count 与元素大小的乘积可由 size_t 表示，供复制及容量区间检查复用。
 *
 * ## Parameters
 * @param[in] count 拟表示的元素数量；零数量在非零 elementSize 下有效。
 * @param[in] elementSize 单个元素的字节数；零值无效。
 *
 * ## Returns
 * @return 仅当元素大小非零且乘积不超过 SIZE_MAX 时返回 ZR_TRUE；不分配内存，也不修改 map。
 *
 * ## Constraints and lifecycle
 * 调用方须在乘法、除法或指针区间计算前使用该谓词；函数本身不验证 count 对应的内存是否存在。
 *
 * ## References
 * 数组复制在本文件 20 行调用，容量形状与 span 检查分别在 104、58–59 行调用。
 */

/**
 * @fn static TZrBool zr_state_map_copy_array(void **destination, TZrUInt32 *destinationCapacity, const void *source, TZrUInt32 count, size_t elementSize)
 *
 * ## Summary
 * 为 Clone 的单个池复制恰好 count 个元素；把分配出的新池及其容量发布到临时候选 map。
 *
 * ## Parameters
 * @param[out] destination 有效的输出槽；成功时写入独立分配的池，空池写入 NULL。
 * @param[out] destinationCapacity 有效的容量输出槽；成功时写入 count。
 * @param[in] source 提供 count 个可读元素的借用地址；count 非零时不得为空。
 * @param[in] count 复制的逻辑元素数；不会复制 source 的未使用容量。
 * @param[in] elementSize 非零的元素字节数，乘积须能由 size_t 表示。
 *
 * ## Returns
 * @return 分配并复制成功，或 count 为零且输出设为空时返回 ZR_TRUE；参数/尺寸无效或分配失败返回 ZR_FALSE。失败时不发布部分输出；调用方负责清理此前已经构造的其他池。
 *
 * ## Constraints and lifecycle
 * destination 与 destinationCapacity 必须指向可写对象。此 helper 不释放 destination 原有内容，调用方须传入空的临时候选字段；非空 source 在复制期间由调用方保持可读。分配成功的所有权转给候选 map。
 *
 * ## References
 * 唯一生产使用位于本文件 192、195、198、201 行，Clone 在 206 行统一释放失败的临时候选。
 */

/**
 * @struct SZrStateMapStorageSpan
 *
 * ## Summary
 * 描述一个 state-map 池的起始地址、容量和元素大小，供内部检查按整个已分配容量而非逻辑 count 比较字节区间。
 *
 * ## Constraints and lifecycle
 * 结构只保存借用的地址及其度量，不拥有或延长底层池的生命周期。非零 capacity 应与有效 storage 配对；区间计算前必须通过尺寸可表示性检查。
 *
 * ## References
 * 本文件 42–44 行的重叠谓词消费两个 span；形状检查和 Clone 共享检查分别构造池数组。
 */

/**
 * @fn static TZrBool zr_state_map_storage_spans_overlap(const SZrStateMapStorageSpan *left, const SZrStateMapStorageSpan *right)
 *
 * ## Summary
 * 以 capacity * elementSize 所代表的半开字节区间判断两个池是否相交；用于拒绝可能导致重复释放或覆盖的 pool alias。
 *
 * ## Parameters
 * @param[in] left 非空的左侧 span 描述。
 * @param[in] right 非空的右侧 span 描述。
 *
 * ## Returns
 * @return 任一容量为零或地址为空时返回 ZR_FALSE；正常区间按半开范围比较。尺寸乘积无效或末端地址溢出时保守返回 ZR_TRUE，避免把不可安全比较的区域当作不重叠。
 *
 * ## Constraints and lifecycle
 * 调用前须保证 span 描述对象本身有效；函数不解引用池内容、不分配内存。它使用容量而不是 map 中的逻辑 count，边界恰好相接不算重叠。
 *
 * ## References
 * 形状检查在本文件 108 行比较同一 map 内的池，Clone 的跨 map 比较经 143–144 行调用。
 */

/**
 * @fn TZrBool zr_state_map_storage_shape_valid(const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 检查 state map 四个池的存储形状，供 Clone、Core storage validation 和 materialization 在读取池前拒绝结构不一致或池间重叠。
 *
 * ## Parameters
 * @param[in] map 借用的 state map；可以为 NULL，容量为零时指针须为空；容量非零时指针须非空。
 *
 * ## Returns
 * @return 对非空 map，只有各 count 不超过对应 capacity、指针与容量匹配、字节尺寸可表示且四个容量区间互不重叠时返回 ZR_TRUE；本函数不提供诊断。
 *
 * ## Constraints and lifecycle
 * 该检查只验证存储形状，不验证 checkpoint 内容、ID/range 语义、指针所指内存的可读性或池的释放所有权。调用期间 map 与池须保持稳定；函数只读且不取得所有权。
 *
 * ## References
 * Core 对外 storage 校验在本文件 153 行转发到此处；Clone 在 182–184 行校验两张 map，materialization 的调用在 exec_ir_materialize.c 574 行。
 */

/**
 * @fn static TZrBool zr_state_map_storage_is_shared(const SZrExecIrStateMap *left, const SZrExecIrStateMap *right)
 *
 * ## Summary
 * 比较两张形状已验证 map 的全部四乘四池区间，识别 Clone 释放或替换 destination 前不能接受的跨 map alias。
 *
 * ## Parameters
 * @param[in] left 已通过 storage shape 检查的只读源 map。
 * @param[in] right 已通过 storage shape 检查、成功替换时将被释放的 destination map。
 *
 * ## Returns
 * @return 任意一对按容量计算的池区间相交时返回 ZR_TRUE；否则返回 ZR_FALSE。不修改 map 或池。
 *
 * ## Constraints and lifecycle
 * 两个参数必须非空且已经分别通过 zr_state_map_storage_shape_valid；该 helper 不重复验证形状。它仅比较池区间，不检查 map 记录结构体本身的部分字节重叠；无内部同步，调用者须保持两张 map 稳定。
 *
 * ## References
 * StateMapClone 在本文件 182–184 行先验证形状再调用；区间比较由 143–144 行转发给容量 span 谓词。
 */

/**
 * @fn void ZrCore_ExecIr_StateMapInit(SZrExecIrStateMap *map)
 *
 * ## Summary
 * 初始化尚未拥有资源的 state-map 记录，建立可安全传给 Free 或 Clone 的空池状态。
 *
 * ## Parameters
 * @param[out] map 可为空；非空时指向可写的 map 记录。
 *
 * ## Returns
 * 无 C 返回值；成功结果通过将整条记录清零交付。NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * 此函数不会先释放 map 中已有的池；只能用于新鲜、零初始化或已释放的记录。对仍拥有池的 map 调用会丢失其指针并泄漏资源。无内部同步。
 *
 * ## References
 * Clone 在本文件 188 行初始化局部候选；Core function clone 在 exec_ir.c 443 行初始化目标 map。
 */

/**
 * @fn void ZrCore_ExecIr_StateMapFree(SZrExecIrStateMap *map)
 *
 * ## Summary
 * 释放 map 持有的 entries、value、root 和 owner-state 四个池，并把记录重置为空状态，供生命周期结束或失败清理时调用。
 *
 * ## Parameters
 * @param[in,out] map 可为空；非空时须指向已初始化、可写记录，且每个非空池地址均可独立释放。
 *
 * ## Returns
 * 无 C 返回值；非空调用完成后记录全零，NULL 输入无操作。重置后的记录可再次 Free。
 *
 * ## Constraints and lifecycle
 * 本函数不验证存储形状或池别名，调用者须保证每个非空池指向可释放的独立分配；畸形/共享池可能导致无效释放或重复释放。释放后原池指针及其借用内容立即失效；无内部同步。
 *
 * ## References
 * Clone 在本文件 206 行回收部分候选、213 行替换旧 destination；函数销毁路径也在 exec_ir.c 181–183 行释放关联 map。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapClone(const SZrExecIrStateMap *source, SZrExecIrStateMap *destination)
 *
 * ## Summary
 * 为 Core state-map 侧表创建独立深拷贝，供函数复制、AOT projection 和需要单独拥有 map 的调用方使用；只复制逻辑 count 对应的四个池元素。
 *
 * ## Parameters
 * @param[in] source 非空源 map；当地址不同于 destination 时，存储形状须有效且池在调用期间保持可读。
 * @param[in,out] destination 非空目标记录；同址路径不读取或释放池。地址不同于 source 时，目标须已初始化、当前池可独立释放，两个记录对象及源/目标池容量区间须互不重叠。
 *
 * ## Returns
 * @return 参数非空且 source 与 destination 是同一记录地址时直接返回 ZR_TRUE，不验证形状、不分配或修改内容。其他成功路径在所有新池复制完成后替换 destination 并返回 ZR_TRUE；参数为空、任一存储形状无效、池区间重叠或任一分配失败时返回 ZR_FALSE，且失败路径保留 source 与 destination 原状态。
 *
 * ## Constraints and lifecycle
 * 以下资源、并发与替换约束适用于 source 与 destination 地址不同的路径。两张 map 记录的结构体存储必须互不重叠，且各池须分别可安全释放；函数按容量检查 map 内部与跨 map 的四个池区间，但不验证 map 内容语义，也不检测记录对象重叠。新副本复制 functionToken、signatureHash、generation 和四池逻辑元素，副本 capacity 收紧为对应 count；不递增 generation。失败时局部候选统一释放，成功时先释放旧 destination 再转交新候选，旧 destination 池借用随即失效。无内部锁；调用期间 source 记录及池不得被并发修改或释放，destination 须独占写入和释放，旧目标池借用不得继续访问。
 *
 * ## References
 * Core function clone 在 exec_ir.c 444 行复制侧表；Parser AOT projection 在 exec_ir_lower_aot.c 74 行复制；本测试目标通过 allocator wrapper 调用同一实现，其他 direct caller 包括 tests/parser/test_ssa_deopt_aggregates.c 376 行。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapStorageValid(const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 检查 Core state-map 四个池的存储形状，供复制和投影入口在读取内容前拒绝结构不一致的 map。
 *
 * ## Parameters
 * @param[in] map 借用的 map 记录；NULL、count 超过 capacity、容量与指针配对不一致或池区间重叠时无效。
 *
 * ## Returns
 * @return 仅当四池的 count/capacity、NULL 配对、容量字节可表示性及容量区间互斥检查均通过时返回 ZR_TRUE；不分配、不修改 map，也不生成诊断。
 *
 * ## Constraints and lifecycle
 * 只验证存储形状，不验证 checkpoint 内容、ID/range 语义、池指针是否实际可读或是否可安全释放。调用期间 map 记录和池须保持稳定；函数只借用输入。
 *
 * ## References
 * 实现转发到本文件 75 行的 shape helper；生产调用包括 exec_ir.c 337 行、aot_ir.c 305 行、exec_ir_lower_aot.c 70 行和 exec_ir_projection_common.c 713 行。
 */

/**
 * @fn void ZrCore_ExecIr_MaterializedStateInit(SZrExecIrMaterializedState *state)
 *
 * ## Summary
 * 将尚未拥有池的 materialized-state 记录清零，建立可供后续填充或 Free 使用的空状态。
 *
 * ## Parameters
 * @param[out] state 可为 NULL；非空时须指向可写记录。
 *
 * ## Returns
 * 不返回 C 值；非空输入被清零，NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * 仅用于新鲜、零初始化或已释放的记录；函数不会先释放已有池。若记录仍拥有池，清零会丢失其所有权指针并泄漏池。无内部同步。
 *
 * ## References
 * MaterializeState 在本文件外的 exec_ir_materialize.c 728 行初始化候选；interpreter resume 在 exec_ir_interpreter_resume.c 308 行初始化逻辑状态。
 */

/**
 * @fn void ZrCore_ExecIr_MaterializedStateFree(SZrExecIrMaterializedState *state)
 *
 * ## Summary
 * 释放 materialized-state 记录的五个池，并将记录清零；记录本身仍由调用方持有。
 *
 * ## Parameters
 * @param[in,out] state 可为 NULL；非空时须指向可写记录，且每个非空池必须是独立且符合 C free 前提的分配。
 *
 * ## Returns
 * 不返回 C 值；非空记录清零，NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * 此函数不检查池指针的有效性或别名；调用方须保证 values、roots、ownerStates、aggregates 和 aggregateFields 各自是独立且可传给 C free 的分配。完成后旧池及指向池元素的借用失效，但 state 记录地址仍有效且为空。无内部同步。
 *
 * ## References
 * MaterializeState 在 exec_ir_materialize.c 764 行等回滚路径释放候选，并在 818 行替换旧目标；interpreter resume 的释放路径见 exec_ir_interpreter_resume.c 312 行和 367 行。
 */

/**
 * @fn TZrBool zr_state_map_input_span_disjoint(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, const void *storage, size_t bytes)
 *
 * ## Summary
 * 检查给定字节范围是否与函数及 state-map 输入记录和其容量池重叠，供 materialization 与恢复入口验证目标缓冲区隔离。
 *
 * ## Parameters
 * @param[in] function 调用期间保持稳定且可读取的函数记录。
 * @param[in] map 调用期间保持稳定且可读取的 state-map 记录。
 * @param[in] storage 待比较字节范围的起始地址；bytes 为零时可为 NULL。
 * @param[in] bytes 待比较范围的字节数；零长度不构成候选区间。
 *
 * ## Returns
 * @return function/map 为空、非零候选范围为空或地址末端溢出、任一输入容量字节数不可表示或候选与输入区间重叠时返回 ZR_FALSE；否则返回 ZR_TRUE。零长度仍会检查输入容量的尺寸可表示性。
 *
 * ## Constraints and lifecycle
 * 本函数按容量而非逻辑 count 比较记录、函数池、map 池及 frameLayout、gcMap、stateMap 的嵌套池地址。它不验证输入池的实际分配来源、可读性或完整语义，也不读取候选范围内容；调用方须保证记录和嵌套记录可安全读取并在调用期间不变。函数不检查 request、diagnostic 或 target 记录别名，也无内部同步。
 *
 * ## References
 * target 数组逐项检查在本文件 345 行调用此 helper；interpreter resume 的独立输入范围检查位于 exec_ir_interpreter_resume.c 256 行。
 */

/**
 * @fn TZrBool zr_state_map_target_valid(const SZrExecIrMaterializedState *target, const SZrExecIrStateMap *map, const SZrExecIrFunction *function)
 *
 * ## Summary
 * 检查 materialized-state 现有池的计数、容量及容量区间隔离，供调用方确认目标池不覆盖函数或 map 输入池；旧池能否安全释放仍由调用方保证。
 *
 * ## Parameters
 * @param[in] target 调用方提供的目标记录；须可读取，且池指针与容量描述须稳定。
 * @param[in] map 将被读取的 state-map 输入。
 * @param[in] function 将被读取的 ExecIR 函数输入。
 *
 * ## Returns
 * @return 任一记录为空、count 超出 capacity、NULL/capacity 配对不一致、容量字节不可表示、目标池彼此或与 target 记录重叠，或任一目标池覆盖函数/map 输入记录及池时返回 ZR_FALSE；否则返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * helper 检查五组池的容量范围，不证明指针指向的分配真实存在或可释放，也不检查 target 记录自身与 function/map/request/diagnostic 的别名。MaterializeState 成功时会 Free 旧 target 后发布新候选；调用方须保证每个非空旧池独立且符合 C free 的前提，并提供与输入记录分离且可写的 target 记录。function、map 及其嵌套池须有效且在检查期间保持稳定。本 helper 不分配、不修改目标，也不提供同步。
 *
 * ## References
 * 唯一生产调用位于 exec_ir_materialize.c 720 行；通过后候选准备于 728 行开始，成功提交在 818–819 行。
 */
