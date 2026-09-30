#include "dataflow_ownership_owner_sets.h"

#include <string.h>

#include "zr_vm_core/memory.h"

/* 非空条目拥有一份独立的排序 ownerIndices 缓冲；UNKNOWN/EMPTY 保留条目不拥有缓冲。 */
typedef struct SZrDataflowOwnershipOwnerSetEntry {
    TZrSize *ownerIndices;
    TZrSize count;
    TZrBool isUnknown;
} SZrDataflowOwnershipOwnerSetEntry;

/* 校验池状态和集合 ID；返回的条目借自 entries，池扩容或释放后不得继续使用。 */
static const SZrDataflowOwnershipOwnerSetEntry *owner_set_entry(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId) {
    if (pool == ZR_NULL ||
        !pool->entries.isValid ||
        setId >= pool->entries.length) {
        return ZR_NULL;
    }
    return (const SZrDataflowOwnershipOwnerSetEntry *)ZrCore_Array_Get(
            (SZrArray *)&pool->entries,
            setId);
}

/* 仅比较已知集合；调用方提供的 owner 列表与驻留列表都按符号索引递增排列。 */
static TZrBool owner_set_indices_equal(
        const SZrDataflowOwnershipOwnerSetEntry *entry,
        const TZrSize *ownerIndices,
        TZrSize count) {
    TZrSize index;

    if (entry == ZR_NULL || entry->isUnknown || entry->count != count) {
        return ZR_FALSE;
    }
    for (index = 0; index < count; index++) {
        if (entry->ownerIndices[index] != ownerIndices[index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 线性查找相同的已知集合；无效池或未命中返回符号索引无效哨兵。 */
static TZrSize owner_set_find(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        const TZrSize *ownerIndices,
        TZrSize count) {
    TZrSize setId;

    if (pool == ZR_NULL || !pool->entries.isValid) {
        return ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    }
    for (setId = 0; setId < pool->entries.length; setId++) {
        if (owner_set_indices_equal(
                    owner_set_entry(pool, setId),
                    ownerIndices,
                    count)) {
            return setId;
        }
    }
    return ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
}

/* 释放由 RawMallocWithType 分配的非空 owner 列表；零元素条目不持有分配块。 */
static void owner_set_free_indices(
        SZrState *state,
        TZrSize *ownerIndices,
        TZrSize count) {
    if (state == ZR_NULL || ownerIndices == ZR_NULL || count == 0) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(
            state->global,
            ownerIndices,
            count * sizeof(TZrSize),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

/* 驻留排序非空集合并接管缓冲；重复项释放临时副本，count==0 映射 EMPTY，池无效时回退 UNKNOWN。 */
static TZrSize owner_set_intern(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize *ownerIndices,
        TZrSize count) {
    SZrDataflowOwnershipOwnerSetEntry entry;
    TZrSize existingSetId;
    TZrSize setId;

    if (state == ZR_NULL ||
        pool == ZR_NULL ||
        !pool->entries.isValid ||
        pool->entries.head == ZR_NULL) {
        owner_set_free_indices(state, ownerIndices, count);
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    if (count == 0) {
        owner_set_free_indices(state, ownerIndices, count);
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_EMPTY;
    }
    existingSetId = owner_set_find(pool, ownerIndices, count);
    if (existingSetId != ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID) {
        owner_set_free_indices(state, ownerIndices, count);
        return existingSetId;
    }

    entry.ownerIndices = ownerIndices;
    entry.count = count;
    entry.isUnknown = ZR_FALSE;
    setId = pool->entries.length;
    /* BUG: 仅在单例/合并缓冲已成功分配且当前表已满时触发：初始 8 格含 2 个哨兵，
     * 因而第 7 个不同非空集合会扩容；若这次 Allocate 返回 NULL，Array_Push 仍对空 head
     * 执行 RawCopy，导致分析断言失败或非法写入。PoolInit 的初始分配失败另有显式检查。 */
    ZrCore_Array_Push(state, &pool->entries, &entry);
    return setId;
}

/* 将 entries 构造成未初始化状态；不能用于覆盖仍持有集合缓冲的活动池。 */
void ZrParser_DataflowOwnership_OwnerSetPoolConstruct(
        SZrDataflowOwnershipOwnerSetPool *pool) {
    if (pool == ZR_NULL) {
        return;
    }
    ZrCore_Array_Construct(&pool->entries);
}

/* 为一次分析分配初始驻留表，并在固定下标放入 UNKNOWN、EMPTY 哨兵。 */
TZrBool ZrParser_DataflowOwnership_OwnerSetPoolInit(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool) {
    SZrDataflowOwnershipOwnerSetEntry unknownEntry;
    SZrDataflowOwnershipOwnerSetEntry emptyEntry;

    if (state == ZR_NULL || pool == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            state,
            &pool->entries,
            sizeof(SZrDataflowOwnershipOwnerSetEntry),
            8);
    if (pool->entries.head == ZR_NULL) {
        ZrCore_Array_Construct(&pool->entries);
        return ZR_FALSE;
    }

    /* 保留稳定 ID 0/1；其余集合 ID 由 entries 下标分配。 */
    memset(&unknownEntry, 0, sizeof(unknownEntry));
    unknownEntry.isUnknown = ZR_TRUE;
    memset(&emptyEntry, 0, sizeof(emptyEntry));
    ZrCore_Array_Push(state, &pool->entries, &unknownEntry);
    ZrCore_Array_Push(state, &pool->entries, &emptyEntry);
    return ZR_TRUE;
}

/* 释放池拥有的所有 owner 列表和 entries 数组；成功路径最终复位为未初始化状态。 */
void ZrParser_DataflowOwnership_OwnerSetPoolFree(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool) {
    TZrSize index;

    if (state == ZR_NULL || pool == ZR_NULL) {
        return;
    }
    if (pool->entries.isValid) {
        for (index = 0; index < pool->entries.length; index++) {
            SZrDataflowOwnershipOwnerSetEntry *entry =
                    (SZrDataflowOwnershipOwnerSetEntry *)ZrCore_Array_Get(
                            &pool->entries,
                            index);
            if (entry != ZR_NULL) {
                owner_set_free_indices(state, entry->ownerIndices, entry->count);
                entry->ownerIndices = ZR_NULL;
                entry->count = 0;
            }
        }
        ZrCore_Array_Free(state, &pool->entries);
    }
    ZrCore_Array_Construct(&pool->entries);
}

/* 创建并驻留单 owner 集合；相同集合复用 ID，owner 列表分配失败降为 UNKNOWN。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetSingleton(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize ownerIndex) {
    TZrSize *ownerIndices;
    TZrSize existingSetId;

    if (state == ZR_NULL ||
        pool == ZR_NULL ||
        ownerIndex == ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    existingSetId = owner_set_find(pool, &ownerIndex, 1);
    if (existingSetId != ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID) {
        return existingSetId;
    }
    ownerIndices = (TZrSize *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrSize),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (ownerIndices == ZR_NULL) {
        /* BUG: 对需追踪且有效的 owner，若此列表 RawMalloc 单次失败而后续分析分配成功，
         * 返回 UNKNOWN 会令 driver 跳过该借用/loan/weak alias 的 owner-release 归因，漏报诊断。 */
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    ownerIndices[0] = ownerIndex;
    return owner_set_intern(state, pool, ownerIndices, 1);
}

/* 对两个已驻留的排序集合做线性归并、去重，再复用或加入池中的规范集合。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetUnion(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize leftSetId,
        TZrSize rightSetId) {
    const SZrDataflowOwnershipOwnerSetEntry *left;
    const SZrDataflowOwnershipOwnerSetEntry *right;
    TZrSize *ownerIndices;
    TZrSize leftIndex = 0;
    TZrSize rightIndex = 0;
    TZrSize count = 0;
    TZrSize capacity;

    left = owner_set_entry(pool, leftSetId);
    right = owner_set_entry(pool, rightSetId);
    if (state == ZR_NULL ||
        left == ZR_NULL ||
        right == ZR_NULL ||
        left->isUnknown ||
        right->isUnknown) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    if (leftSetId == rightSetId) {
        return leftSetId;
    }
    if (left->count == 0) {
        return rightSetId;
    }
    if (right->count == 0) {
        return leftSetId;
    }
    if (left->count > ((TZrSize)-1) - right->count) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    capacity = left->count + right->count;
    if (capacity == 0) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_EMPTY;
    }
    if (capacity > ((TZrSize)-1) / sizeof(TZrSize)) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    /* BUG: 对后续释放归因所需的 owner 并集，若结果或重叠后的紧缩缓冲 RawMalloc 单次失败、
     * 且其余分析分配成功，UNKNOWN 会使 driver 跳过该 alias 的 owner-release 检查并漏报诊断。 */
    ownerIndices = (TZrSize *)ZrCore_Memory_RawMallocWithType(
            state->global,
            capacity * sizeof(TZrSize),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (ownerIndices == ZR_NULL) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }

    while (leftIndex < left->count || rightIndex < right->count) {
        if (rightIndex >= right->count ||
            (leftIndex < left->count &&
             left->ownerIndices[leftIndex] < right->ownerIndices[rightIndex])) {
            ownerIndices[count++] = left->ownerIndices[leftIndex++];
        } else if (leftIndex >= left->count ||
                   right->ownerIndices[rightIndex] < left->ownerIndices[leftIndex]) {
            ownerIndices[count++] = right->ownerIndices[rightIndex++];
        } else {
            ownerIndices[count++] = left->ownerIndices[leftIndex];
            leftIndex++;
            rightIndex++;
        }
    }
    if (count != capacity) {
        TZrSize *exactOwnerIndices = (TZrSize *)ZrCore_Memory_RawMallocWithType(
                state->global,
                count * sizeof(TZrSize),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (exactOwnerIndices == ZR_NULL) {
            owner_set_free_indices(state, ownerIndices, capacity);
            return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
        }
        ZrCore_Memory_RawCopy(
                exactOwnerIndices,
                ownerIndices,
                count * sizeof(TZrSize));
        owner_set_free_indices(state, ownerIndices, capacity);
        ownerIndices = exactOwnerIndices;
    }
    return owner_set_intern(state, pool, ownerIndices, count);
}

/* 将无效 ID 与保留 UNKNOWN ID 都视为未知集合。 */
TZrBool ZrParser_DataflowOwnership_OwnerSetIsUnknown(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId) {
    const SZrDataflowOwnershipOwnerSetEntry *entry = owner_set_entry(pool, setId);
    return entry == ZR_NULL || entry->isUnknown;
}

/* 读取已知集合大小；调用方需先用 IsUnknown 区分空集和无效/未知集合。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetCount(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId) {
    const SZrDataflowOwnershipOwnerSetEntry *entry = owner_set_entry(pool, setId);
    return entry == ZR_NULL || entry->isUnknown ? 0 : entry->count;
}

/* 按零起始下标读取 owner；未知 ID 或越界位置返回符号索引无效哨兵。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetAt(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId,
        TZrSize index) {
    const SZrDataflowOwnershipOwnerSetEntry *entry = owner_set_entry(pool, setId);
    if (entry == ZR_NULL || entry->isUnknown || index >= entry->count) {
        return ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    }
    return entry->ownerIndices[index];
}
