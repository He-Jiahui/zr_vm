//
// Created by HeJiahui on 2025/6/24.
//
#include "zr_vm_core/hash_set.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(ZR_DEBUG)
static TZrBool hash_set_trace_enabled(void);
static void hash_set_trace(const TZrChar *format, ...);
#else
#define hash_set_trace(...) ((void)0)
#endif

static TZrSize zr_hash_pair_pool_block_bytes(TZrSize capacity) {
    TZrSize extraPairs = capacity > ZR_HASH_PAIR_POOL_INLINE_COUNT ? capacity - ZR_HASH_PAIR_POOL_INLINE_COUNT : 0;
    return sizeof(SZrHashPairPoolBlock) + extraPairs * sizeof(SZrHashKeyValuePair);
}

static ZR_FORCE_INLINE void zr_hash_set_zero_bucket_slots(SZrHashKeyValuePair **bucketSlots, TZrSize slotCount) {
    ZR_ASSERT(bucketSlots != ZR_NULL || slotCount == 0);
    if (slotCount > 0) {
        ZrCore_Memory_RawSet(bucketSlots, 0, slotCount * sizeof(*bucketSlots));
    }
}

static void zr_hash_pair_pool_release(SZrGlobalState *global, SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    if (global == ZR_NULL || set == ZR_NULL) {
        return;
    }

    block = set->pairPoolHead;
    while (block != ZR_NULL) {
        SZrHashPairPoolBlock *next = block->next;
        ZrCore_Memory_RawFreeWithType(global,
                                      block,
                                      zr_hash_pair_pool_block_bytes(block->capacity),
                                      ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
        block = next;
    }

    set->pairPoolHead = ZR_NULL;
    set->pairPoolActive = ZR_NULL;
    set->pairPoolTail = ZR_NULL;
    set->pairPoolCapacity = 0;
    set->pairPoolUsed = 0;
}

void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set) {
    SZrGlobalState *global = state->global;
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;

    zr_hash_pair_pool_release(global, set);
    if (oldBuckets != ZR_NULL) {
        ZrCore_Memory_Allocate(global, oldBuckets, oldBucketCount, 0, ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
        set->buckets = ZR_NULL;
    }
    set->bucketSize = 0;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->isValid = ZR_FALSE;
}

TZrBool ZrCore_HashSet_EnsurePairPoolForElementCount(SZrState *state, SZrHashSet *set, TZrSize elementCount) {
    TZrSize additionalCapacity;
    TZrSize blockBytes;
    SZrHashPairPoolBlock *block;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || elementCount <= set->pairPoolCapacity) {
        return state != ZR_NULL && set != ZR_NULL;
    }

    additionalCapacity = elementCount - set->pairPoolCapacity;
    blockBytes = zr_hash_pair_pool_block_bytes(additionalCapacity);
    block = ZR_CAST(SZrHashPairPoolBlock *,
                    ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_HASH_PAIR, blockBytes));
    if (block == ZR_NULL) {
        return ZR_FALSE;
    }

    block->next = ZR_NULL;
    block->capacity = additionalCapacity;
    block->used = 0;

    if (set->pairPoolHead == ZR_NULL) {
        set->pairPoolHead = block;
        set->pairPoolActive = block;
        set->pairPoolTail = block;
    } else {
        ZR_ASSERT(set->pairPoolTail != ZR_NULL);
        set->pairPoolTail->next = block;
        set->pairPoolTail = block;
        if (set->pairPoolActive == ZR_NULL || set->pairPoolActive->used >= set->pairPoolActive->capacity) {
            set->pairPoolActive = block;
        }
    }
    set->pairPoolCapacity += additionalCapacity;
    return ZR_TRUE;
}

TZrBool ZrCore_HashSet_Rehash(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    ZR_ASSERT(set != NULL && newCapacity > set->capacity);
    const TZrSize elementSize = sizeof(TZrPtr);
    SZrGlobalState *global;
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    TZrSize newBucketCount = newCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
    SZrHashKeyValuePair **newBuckets;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        hash_set_trace("rehash reject state=%p global=%p set=%p newCapacity=%llu",
                       (void *)state,
                       state != ZR_NULL ? (void *)state->global : ZR_NULL,
                       (void *)set,
                       (unsigned long long)newCapacity);
        return ZR_FALSE;
    }

    global = state->global;
    hash_set_trace("rehash enter set=%p oldCapacity=%llu oldBuckets=%p newCapacity=%llu",
                   (void *)set,
                   (unsigned long long)oldCapacity,
                   (void *)oldBuckets,
                   (unsigned long long)newCapacity);
    newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrCore_Memory_Allocate(global,
                                   oldBuckets,
                                   oldBucketCount,
                                   newBucketCount,
                                   ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    if (newBuckets == ZR_NULL) {
        hash_set_trace("rehash allocate failed set=%p oldBucketBytes=%llu newBucketBytes=%llu",
                       (void *)set,
                       (unsigned long long)oldBucketCount,
                       (unsigned long long)newBucketCount);
        return ZR_FALSE;
    }
    oldBuckets = ZR_NULL;
    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    set->resizeThreshold = newCapacity * ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR;
    zr_hash_set_zero_bucket_slots(set->buckets + oldCapacity, newCapacity - oldCapacity);
    for (TZrSize i = 0; i < oldCapacity; i++) {
        SZrHashKeyValuePair *objectPtr = newBuckets[i];
        newBuckets[i] = ZR_NULL;
        while (objectPtr != ZR_NULL) {
            SZrHashKeyValuePair *next = objectPtr->next;
            TZrUInt64 hash;
            TZrSize index;

            if (ZR_VALUE_IS_TYPE_SIGNED_INT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_FLOAT(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_BOOL(objectPtr->key.type) ||
                ZR_VALUE_IS_TYPE_NULL(objectPtr->key.type)) {
                hash = objectPtr->key.value.nativeObject.nativeUInt64;
            } else if (objectPtr->key.isGarbageCollectable && objectPtr->key.value.object != ZR_NULL) {
                hash = objectPtr->key.value.object->hash;
            } else {
                hash = ZrCore_Value_GetHash(state, &objectPtr->key);
            }

            index = ZR_HASH_MOD(hash, newCapacity);
            objectPtr->next = newBuckets[index];
            newBuckets[index] = objectPtr;
            objectPtr = next;
        }
    }
    hash_set_trace("rehash exit set=%p capacity=%llu bucketSize=%llu threshold=%llu buckets=%p",
                   (void *)set,
                   (unsigned long long)set->capacity,
                   (unsigned long long)set->bucketSize,
                   (unsigned long long)set->resizeThreshold,
                   (void *)set->buckets);
    return ZR_TRUE;
}

TZrBool ZrCore_HashSet_GrowDenseSequentialIntKeys(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    const TZrSize elementSize = sizeof(TZrPtr);
    SZrGlobalState *global;
    TZrSize oldCapacity;
    TZrSize oldBucketCount;
    TZrSize newBucketCount;
    SZrHashKeyValuePair **oldBuckets;
    SZrHashKeyValuePair **newBuckets;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(newCapacity > (set != ZR_NULL ? set->capacity : 0));

    if (state == ZR_NULL || state->global == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    oldCapacity = set->capacity;
    oldBucketCount = oldCapacity * elementSize;
    newBucketCount = newCapacity * elementSize;
    oldBuckets = set->buckets;

    newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrCore_Memory_Allocate(global,
                                   oldBuckets,
                                   oldBucketCount,
                                   newBucketCount,
                                   ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    if (newBuckets == ZR_NULL) {
        return ZR_FALSE;
    }

    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    /*
     * Dense sequential int-key tables index buckets directly by element index, so
     * the bucket array itself is the real append ceiling. Keeping the threshold at
     * full capacity avoids premature resize checks on this specialized path.
     */
    set->resizeThreshold = newCapacity;
    zr_hash_set_zero_bucket_slots(set->buckets + oldCapacity, newCapacity - oldCapacity);
    return ZR_TRUE;
}

#if defined(ZR_DEBUG)
static TZrBool hash_set_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void hash_set_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!hash_set_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-hash-set] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
#endif
