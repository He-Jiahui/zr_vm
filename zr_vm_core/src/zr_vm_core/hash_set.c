//
// Created by HeJiahui on 2025/6/24.
//
#include "zr_vm_core/hash_set.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"

#define ZR_HASH_SET_MAX_LOAD_NUMERATOR ((TZrSize)3)
#define ZR_HASH_SET_MAX_LOAD_DENOMINATOR ((TZrSize)4)

void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set) {
    SZrGlobalState *global = state->global;
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
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

TZrBool ZrCore_HashSet_Rehash(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    ZR_ASSERT(set != NULL && newCapacity > set->capacity);
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    TZrSize newBucketCount = newCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
    SZrHashKeyValuePair **newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrCore_Memory_GcReallocate(state,
                                       oldBuckets,
                                       oldBucketCount,
                                       newBucketCount,
                                       ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    if (newBuckets == ZR_NULL) {
        return ZR_FALSE;
    }
    oldBuckets = ZR_NULL;
    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    set->resizeThreshold = newCapacity * ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR;
    ZrCore_Memory_RawSet(set->buckets + oldCapacity, 0, newBucketCount - oldBucketCount);
    for (TZrSize i = 0; i < oldCapacity; i++) {
        SZrHashKeyValuePair *objectPtr = newBuckets[i];
        newBuckets[i] = ZR_NULL;
        while (objectPtr != ZR_NULL) {
            SZrHashKeyValuePair *next = objectPtr->next;
            TZrUInt64 hash = ZrCore_Value_GetHash(state, &objectPtr->key);
            TZrSize index = ZR_HASH_MOD(hash, newCapacity);
            objectPtr->next = newBuckets[index];
            newBuckets[index] = objectPtr;
            objectPtr = next;
        }
    }
    return ZR_TRUE;
}
