//
// Created by HeJiahui on 2025/6/24.
//
#include "zr_vm_core/hash_set.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/memory.h"

void ZrHashSetRehash(SZrGlobalState *global, SZrHashSet *set, TZrSize newCapacity) {
    ZR_ASSERT(set != NULL && newCapacity > set->capacity);
    const TZrSize elementSize = sizeof(TZrPtr);
    TZrSize oldCapacity = set->capacity;
    TZrSize oldBucketCount = oldCapacity * elementSize;
    TZrSize newBucketCount = newCapacity * elementSize;
    SZrHashRawObject **oldBuckets = set->buckets;
    SZrHashRawObject **newBuckets = ZR_CAST_HASH_RAW_OBJECT_PTR(
        ZrMemoryAllocate(global, oldBuckets, oldBucketCount, newBucketCount));
    oldBuckets = ZR_NULL;
    set->buckets = newBuckets;
    set->capacity = newCapacity;
    set->resizeThreshold = newCapacity * 3 / 4;
    ZrMemoryRawSet(set->buckets + oldBucketCount, 0, newBucketCount - oldBucketCount);
    for (TZrSize i = 0; i < oldCapacity; i++) {
        SZrHashRawObject *objectPtr = newBuckets[i];
        newBuckets[i] = ZR_NULL;
        while (objectPtr != ZR_NULL) {
            SZrHashRawObject *next = objectPtr->next;
            TZrSize hash = objectPtr->hash;
            TZrSize index = ZR_HASH_MOD(hash, newCapacity);
            objectPtr->next = newBuckets[index];
            newBuckets[index] = objectPtr;
            objectPtr = next;
        }
    }
}
