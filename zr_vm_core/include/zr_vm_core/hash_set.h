//
// Created by HeJiahui on 2025/6/24.
//

#ifndef ZR_VM_CORE_HASH_SET_H
#define ZR_VM_CORE_HASH_SET_H
#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrGlobalState;
#define ZR_HASH_MOD(HASH, CAPACITY) (ZR_ASSERT((CAPACITY & (CAPACITY - 1)) == 0), ((HASH) & (CAPACITY - 1)))

ZR_FORCE_INLINE void ZrHashSetConstruct(SZrHashSet *set) { set->isValid = ZR_FALSE; }

ZR_CORE_API void ZrHashSetRehash(struct SZrGlobalState *global, SZrHashSet *set, TZrSize newCapacity);

ZR_FORCE_INLINE void ZrHashSetInit(struct SZrGlobalState *global, SZrHashSet *set, TZrSize capacityLog2) {
    ZR_ASSERT(set != NULL && capacityLog2 != 0);
    const TZrSize capacity = (TZrSize) 1 << capacityLog2;
    set->buckets = ZR_NULL;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    ZrHashSetRehash(global, set, capacity);
    set->isValid = ZR_TRUE;
}

ZR_FORCE_INLINE SZrHashRawObject *ZrHashSetGetBucket(SZrHashSet *set, TUInt64 hash) {
    return set->buckets[ZR_HASH_MOD(hash, set->capacity)];
}

ZR_FORCE_INLINE void ZrHashSetAdd(struct SZrGlobalState *global, SZrHashSet *set, SZrHashRawObject *element) {
    if (set->elementCount + 1 > set->resizeThreshold) {
        ZrHashSetRehash(global, set, set->capacity << 1);
    }
    SZrHashRawObject *object = ZrHashSetGetBucket(set, element->hash);
    element->next = object;
    set->buckets[ZR_HASH_MOD(element->hash, set->capacity)] = element;
    set->elementCount++;
}

ZR_FORCE_INLINE TBool ZrHashSetRemove(SZrHashSet *set, SZrHashRawObject *element) {
    SZrHashRawObject *object = ZrHashSetGetBucket(set, element->hash);
    SZrHashRawObject *prev = ZR_NULL;
    while (object != ZR_NULL) {
        if (object == element) {
            if (prev == ZR_NULL) {
                set->buckets[ZR_HASH_MOD(element->hash, set->capacity)] = object->next;
            } else {
                prev->next = object->next;
            }
            set->elementCount--;
            return ZR_TRUE;
        }
        prev = object;
        object = object->next;
    }
    return ZR_FALSE;
}

ZR_FORCE_INLINE TBool ZrHashSetFind(SZrHashSet *set, SZrHashRawObject *element) {
    SZrHashRawObject *object = ZrHashSetGetBucket(set, element->hash);
    while (object != ZR_NULL) {
        if (object == element) {
            return ZR_TRUE;
        }
        object = object->next;
    }
    return ZR_FALSE;
}

#endif // ZR_VM_CORE_HASH_SET_H
