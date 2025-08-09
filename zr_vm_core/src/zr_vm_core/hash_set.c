//
// Created by HeJiahui on 2025/6/24.
//
#include "zr_vm_core/hash_set.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"

void ZrHashSetDeconstruct(struct SZrState *state, SZrHashSet *set) {
    SZrGlobalState *global = state->global;
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
    if (oldBuckets != ZR_NULL) {
        ZrMemoryAllocate(global, oldBuckets, oldBucketCount, 0, ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
        set->buckets = ZR_NULL;
    }
}

void ZrHashSetRehash(SZrState *state, SZrHashSet *set, TZrSize newCapacity) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(set != NULL && newCapacity > set->capacity);
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    TZrSize newBucketCount = newCapacity * elementSize;
    SZrHashKeyValuePair **oldBuckets = set->buckets;
    SZrHashKeyValuePair **newBuckets = ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(
            ZrMemoryAllocate(global, oldBuckets, oldBucketCount, newBucketCount, ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET));
    oldBuckets = ZR_NULL;
    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->bucketSize = newBucketCount;
    set->resizeThreshold = newCapacity * 3 / 4;
    ZrMemoryRawSet(set->buckets + oldBucketCount, 0, newBucketCount - oldBucketCount);
    for (TZrSize i = 0; i < oldCapacity; i++) {
        SZrHashKeyValuePair *objectPtr = newBuckets[i];
        newBuckets[i] = ZR_NULL;
        while (objectPtr != ZR_NULL) {
            SZrHashKeyValuePair *next = objectPtr->next;
            TUInt64 hash = ZrValueGetHash(state, &objectPtr->key);
            TZrSize index = ZR_HASH_MOD(hash, newCapacity);
            objectPtr->next = newBuckets[index];
            newBuckets[index] = objectPtr;
            objectPtr = next;
        }
    }
}
