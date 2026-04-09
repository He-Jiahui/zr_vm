//
// Created by HeJiahui on 2025/6/24.
//

#ifndef ZR_VM_CORE_HASH_SET_H
#define ZR_VM_CORE_HASH_SET_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrGlobalState;

#if defined(_MSC_VER)
    #define ZR_HASH_PAIR_POOL_INLINE_COUNT 1
    #define ZR_HASH_PAIR_POOL_FLEX_DECL 1
#else
    #define ZR_HASH_PAIR_POOL_INLINE_COUNT 0
    #define ZR_HASH_PAIR_POOL_FLEX_DECL
#endif

struct SZrHashPairPoolBlock {
    struct SZrHashPairPoolBlock *next;
    TZrSize capacity;
    TZrSize used;
    SZrHashKeyValuePair pairs[ZR_HASH_PAIR_POOL_FLEX_DECL];
};

typedef struct SZrHashPairPoolBlock SZrHashPairPoolBlock;

#define ZR_HASH_SET_CAPACITY_GROWTH_FACTOR ((TZrSize)2)
#define ZR_HASH_SET_MAX_LOAD_NUMERATOR ((TZrSize)3)
#define ZR_HASH_SET_MAX_LOAD_DENOMINATOR ((TZrSize)4)

// MSVC 不支持在数组下标中使用逗号运算符，使用内联函数替代
ZR_FORCE_INLINE TZrSize zr_hash_mod_inline(TZrUInt64 hash, TZrSize capacity) {
    ZR_ASSERT((capacity & (capacity - 1)) == 0);
    return (TZrSize)(hash & (capacity - 1));
}
#define ZR_HASH_MOD(HASH, CAPACITY) zr_hash_mod_inline((HASH), (CAPACITY))

struct SZrHashSet {
    SZrHashKeyValuePair **buckets;
    TZrSize bucketSize;
    TZrSize elementCount;
    TZrSize capacity;
    TZrSize resizeThreshold;
    struct SZrHashPairPoolBlock *pairPoolHead;
    struct SZrHashPairPoolBlock *pairPoolActive;
    TZrSize pairPoolCapacity;
    TZrSize pairPoolUsed;
    TZrBool isValid;
};

typedef struct SZrHashSet SZrHashSet;

ZR_FORCE_INLINE void ZrCore_HashSet_Construct(SZrHashSet *set) {
    set->buckets = ZR_NULL;
    set->bucketSize = 0;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->pairPoolHead = ZR_NULL;
    set->pairPoolActive = ZR_NULL;
    set->pairPoolCapacity = 0;
    set->pairPoolUsed = 0;
    set->isValid = ZR_FALSE;
}
ZR_CORE_API void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set);

ZR_CORE_API TZrBool ZrCore_HashSet_Rehash(struct SZrState *state, SZrHashSet *set, TZrSize newCapacity);
ZR_CORE_API TZrBool ZrCore_HashSet_GrowDenseSequentialIntKeys(struct SZrState *state,
                                                              SZrHashSet *set,
                                                              TZrSize newCapacity);
ZR_CORE_API TZrBool ZrCore_HashSet_EnsurePairPoolForElementCount(struct SZrState *state,
                                                                 SZrHashSet *set,
                                                                 TZrSize elementCount);

static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPair(SZrHashSet *set) {
    struct SZrHashPairPoolBlock *block;

    if (set == ZR_NULL) {
        return ZR_NULL;
    }

    block = set->pairPoolActive;
    while (block != ZR_NULL && block->used >= block->capacity) {
        block = block->next;
    }
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    set->pairPoolActive = block;
    set->pairPoolUsed++;
    return &block->pairs[block->used++];
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairAssumeAvailable(SZrHashSet *set) {
    struct SZrHashPairPoolBlock *block;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    if (ZR_LIKELY(block != ZR_NULL && block->used < block->capacity)) {
        set->pairPoolUsed++;
        return &block->pairs[block->used++];
    }

    while (block != ZR_NULL && block->used >= block->capacity) {
        block = block->next;
    }

    ZR_ASSERT(block != ZR_NULL);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    set->pairPoolActive = block;
    set->pairPoolUsed++;
    return &block->pairs[block->used++];
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_TakeReservedPairSpanAssumeAvailable(SZrHashSet *set,
                                                                                                TZrSize *inOutCount) {
    struct SZrHashPairPoolBlock *block;
    TZrSize available;
    TZrSize takeCount;
    SZrHashKeyValuePair *result;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(inOutCount != ZR_NULL);
    ZR_ASSERT(*inOutCount > 0);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    if (!(ZR_LIKELY(block != ZR_NULL && block->used < block->capacity))) {
        while (block != ZR_NULL && block->used >= block->capacity) {
            block = block->next;
        }
        ZR_ASSERT(block != ZR_NULL);
        if (block == ZR_NULL) {
            return ZR_NULL;
        }
        set->pairPoolActive = block;
    }

    available = block->capacity - block->used;
    takeCount = *inOutCount < available ? *inOutCount : available;
    result = &block->pairs[block->used];
    block->used += takeCount;
    set->pairPoolUsed += takeCount;
    *inOutCount = takeCount;
    return result;
}

ZR_FORCE_INLINE TZrSize ZrCore_HashSet_MinCapacityForElementCount(TZrSize elementCount) {
    TZrSize capacity = 1;

    while (capacity * ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR < elementCount) {
        capacity *= ZR_HASH_SET_CAPACITY_GROWTH_FACTOR;
    }

    return capacity;
}

ZR_FORCE_INLINE TZrSize ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(TZrSize elementCount) {
    TZrSize capacity = 1;

    while (capacity < elementCount) {
        capacity *= ZR_HASH_SET_CAPACITY_GROWTH_FACTOR;
    }

    return capacity;
}

ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(struct SZrState *state,
                                                                            SZrHashSet *set,
                                                                            TZrSize elementCount) {
    TZrSize requiredCapacity;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return ZR_FALSE;
    }

    requiredCapacity = ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(elementCount);
    if (requiredCapacity > set->capacity) {
        return ZrCore_HashSet_GrowDenseSequentialIntKeys(state, set, requiredCapacity);
    }

    /*
     * Dense sequential int-key tables use direct bucket indexing, so once a
     * table is on this path the current bucket array is the true append limit.
     * Preserve that invariant even when the table was originally initialized
     * through the generic hash-set constructor with a 0.75 load threshold.
     */
    if (set->resizeThreshold < set->capacity) {
        set->resizeThreshold = set->capacity;
    }
    return ZR_TRUE;
}

ZR_FORCE_INLINE TZrBool ZrCore_HashSet_EnsureCapacityForElementCount(struct SZrState *state,
                                                                     SZrHashSet *set,
                                                                     TZrSize elementCount) {
    TZrSize requiredCapacity;

    if (state == ZR_NULL || set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return ZR_FALSE;
    }

    requiredCapacity = ZrCore_HashSet_MinCapacityForElementCount(elementCount);
    if (requiredCapacity <= set->capacity) {
        return ZR_TRUE;
    }

    return ZrCore_HashSet_Rehash(state, set, requiredCapacity);
}

ZR_FORCE_INLINE SZrTypeValue ZrCore_HashSet_MakeNullValueFallback(void) {
    SZrTypeValue value;
    value.type = ZR_VALUE_TYPE_NULL;
    value.value.nativeObject.nativeUInt64 = 0;
    value.isGarbageCollectable = ZR_FALSE;
    value.isNative = ZR_TRUE;
    value.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value.ownershipControl = ZR_NULL;
    value.ownershipWeakRef = ZR_NULL;
    return value;
}

ZR_FORCE_INLINE void ZrCore_HashSet_Init(struct SZrState *state, SZrHashSet *set, TZrSize capacityLog2) {
    ZR_ASSERT(set != NULL && capacityLog2 != 0);
    const TZrSize capacity = (TZrSize) 1 << capacityLog2;
    set->buckets = ZR_NULL;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    set->bucketSize = 0;
    set->isValid = ZrCore_HashSet_Rehash(state, set, capacity);
}

ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_GetBucket(SZrHashSet *set, TZrUInt64 hash) {
    return set->buckets[ZR_HASH_MOD(hash, set->capacity)];
}
// add unique element, we do not check duplicated element
static ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_Add(struct SZrState *state, SZrHashSet *set,
                                                         const SZrTypeValue *element) {
    SZrHashKeyValuePair *object;
    SZrHashKeyValuePair *hashElement;
    TZrUInt64 hash;

    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    if (set->elementCount + 1 > set->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, set, set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    hash = ZrCore_Value_GetHash(state, element);
    object = ZrCore_HashSet_GetBucket(set, hash);
    hashElement = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                                ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                                sizeof(SZrHashKeyValuePair));
    if (hashElement == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_ResetAsNull(&hashElement->key);
    ZrCore_Value_ResetAsNull(&hashElement->value);
    ZrCore_Value_Copy(state, &hashElement->key, element);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}
// we will also mark value as element
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_AddRawObject(struct SZrState *state, SZrHashSet *set,
                                                           SZrRawObject *element) {
    SZrHashKeyValuePair *object;
    SZrHashKeyValuePair *hashElement;
    TZrUInt64 hash;

    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    if (set->elementCount + 1 > set->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, set, set->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    hash = element->hash;
    object = ZrCore_HashSet_GetBucket(set, hash);
    hashElement = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                                ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                                sizeof(SZrHashKeyValuePair));
    if (hashElement == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_ResetAsNull(&hashElement->key);
    ZrCore_Value_ResetAsNull(&hashElement->value);
    ZrCore_Value_InitAsRawObject(state, &hashElement->key, element);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}

ZR_FORCE_INLINE SZrTypeValue ZrCore_HashSet_Remove(struct SZrState *state, SZrHashSet *set, const SZrTypeValue *element) {
    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return state != ZR_NULL && state->global != ZR_NULL
            ? state->global->nullValue
            : ZrCore_HashSet_MakeNullValueFallback();
    }
    TZrUInt64 hash = ZrCore_Value_GetHash(state, element);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    SZrHashKeyValuePair *prev = ZR_NULL;
    while (object != ZR_NULL) {
        // same address or equal content(customized compare function)
        if (ZrCore_Value_CompareDirectly(state, &object->key, element)) {
            if (prev == ZR_NULL) {
                set->buckets[ZR_HASH_MOD(hash, set->capacity)] = object->next;
            } else {
                prev->next = object->next;
            }
            set->elementCount--;
            SZrTypeValue result = object->key;
            ZrCore_Memory_RawFreeWithType(state->global, object, sizeof(SZrHashKeyValuePair),
                                    ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
            return result;
        }
        prev = object;
        object = object->next;
    }
    return state->global->nullValue;
}

ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_Find(struct SZrState *state, SZrHashSet *set,
                                                   const SZrTypeValue *element) {
    if (state == ZR_NULL || set == ZR_NULL || element == ZR_NULL || !set->isValid || set->buckets == ZR_NULL ||
        set->capacity == 0) {
        return ZR_NULL;
    }
    TZrUInt64 hash = ZrCore_Value_GetHash(state, element);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    while (object != ZR_NULL) {
        if (ZrCore_Value_CompareDirectly(state, &object->key, element)) {
            return object;
        }
        object = object->next;
    }
    return ZR_NULL;
}

#endif // ZR_VM_CORE_HASH_SET_H
