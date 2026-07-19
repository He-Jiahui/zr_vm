#include "dataflow_ownership_owner_sets.h"

#include <string.h>

#include "zr_vm_core/memory.h"

typedef struct SZrDataflowOwnershipOwnerSetEntry {
    TZrSize *ownerIndices;
    TZrSize count;
    TZrBool isUnknown;
} SZrDataflowOwnershipOwnerSetEntry;

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
    ZrCore_Array_Push(state, &pool->entries, &entry);
    return setId;
}

void ZrParser_DataflowOwnership_OwnerSetPoolConstruct(
        SZrDataflowOwnershipOwnerSetPool *pool) {
    if (pool == ZR_NULL) {
        return;
    }
    ZrCore_Array_Construct(&pool->entries);
}

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

    memset(&unknownEntry, 0, sizeof(unknownEntry));
    unknownEntry.isUnknown = ZR_TRUE;
    memset(&emptyEntry, 0, sizeof(emptyEntry));
    ZrCore_Array_Push(state, &pool->entries, &unknownEntry);
    ZrCore_Array_Push(state, &pool->entries, &emptyEntry);
    return ZR_TRUE;
}

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
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    ownerIndices[0] = ownerIndex;
    return owner_set_intern(state, pool, ownerIndices, 1);
}

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

TZrBool ZrParser_DataflowOwnership_OwnerSetIsUnknown(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId) {
    const SZrDataflowOwnershipOwnerSetEntry *entry = owner_set_entry(pool, setId);
    return entry == ZR_NULL || entry->isUnknown;
}

TZrSize ZrParser_DataflowOwnership_OwnerSetCount(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId) {
    const SZrDataflowOwnershipOwnerSetEntry *entry = owner_set_entry(pool, setId);
    return entry == ZR_NULL || entry->isUnknown ? 0 : entry->count;
}

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
