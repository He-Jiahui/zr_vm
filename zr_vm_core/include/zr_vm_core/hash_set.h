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
#define ZR_HASH_MOD(HASH, CAPACITY) (ZR_ASSERT((CAPACITY & (CAPACITY - 1)) == 0), ((HASH) & (CAPACITY - 1)))

struct SZrHashSet {
    SZrHashKeyValuePair **buckets;
    TZrSize bucketSize;
    TZrSize elementCount;
    TZrSize capacity;
    TZrSize resizeThreshold;
    TBool isValid;
};

typedef struct SZrHashSet SZrHashSet;

ZR_FORCE_INLINE void ZrHashSetConstruct(SZrHashSet *set) { set->isValid = ZR_FALSE; }

ZR_CORE_API void ZrHashSetRehash(struct SZrState *state, SZrHashSet *set, TZrSize newCapacity);

ZR_FORCE_INLINE void ZrHashSetInit(struct SZrState *state, SZrHashSet *set, TZrSize capacityLog2) {
    ZR_ASSERT(set != NULL && capacityLog2 != 0);
    const TZrSize capacity = (TZrSize) 1 << capacityLog2;
    set->buckets = ZR_NULL;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    ZrHashSetRehash(state, set, capacity);
    set->isValid = ZR_TRUE;
}

ZR_FORCE_INLINE SZrHashKeyValuePair *ZrHashSetGetBucket(SZrHashSet *set, TUInt64 hash) {
    return set->buckets[ZR_HASH_MOD(hash, set->capacity)];
}
// add unique element, we do not check duplicated element
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrHashSetAdd(struct SZrState *state, SZrHashSet *set,
                                                  const SZrTypeValue *element) {
    SZrGlobalState *global = state->global;
    if (set->elementCount + 1 > set->resizeThreshold) {
        ZrHashSetRehash(state, set, set->capacity << 1);
    }
    TUInt64 hash = ZrValueGetHash(state, element);
    SZrHashKeyValuePair *object = ZrHashSetGetBucket(set, hash);
    SZrHashKeyValuePair *hashElement =
            ZrMemoryRawMallocWithType(global, sizeof(SZrHashKeyValuePair), ZR_VALUE_TYPE_VM_MEMORY);
    ZrValueCopy(state, &hashElement->key, element);
    ZrValueResetAsNull(&hashElement->value);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}
// we will also mark value as element
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrHashSetAddRawObject(struct SZrState *state, SZrHashSet *set,
                                                           SZrRawObject *element) {
    SZrGlobalState *global = state->global;
    if (set->elementCount + 1 > set->resizeThreshold) {
        ZrHashSetRehash(state, set, set->capacity << 1);
    }
    TUInt64 hash = element->hash;
    SZrHashKeyValuePair *object = ZrHashSetGetBucket(set, hash);
    // todo: it should be managed by gc, how to bind it, stringTable and objectTable
    SZrHashKeyValuePair *hashElement =
            ZrMemoryRawMallocWithType(global, sizeof(SZrHashKeyValuePair), ZR_VALUE_TYPE_VM_MEMORY);
    ZrValueInitAsRawObject(state, &hashElement->key, element);
    // object->next = hashElement;
    ZrValueResetAsNull(&hashElement->value);
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}

ZR_FORCE_INLINE SZrTypeValue ZrHashSetRemove(struct SZrState *state, SZrHashSet *set, const SZrTypeValue *element) {
    TUInt64 hash = ZrValueGetHash(state, element);
    SZrHashKeyValuePair *object = ZrHashSetGetBucket(set, hash);
    SZrHashKeyValuePair *prev = ZR_NULL;
    while (object != ZR_NULL) {
        // same address or equal content(customized compare function)
        if (ZrValueCompareDirectly(state, &object->key, element)) {
            if (prev == ZR_NULL) {
                set->buckets[ZR_HASH_MOD(hash, set->capacity)] = object->next;
            } else {
                prev->next = object->next;
            }
            set->elementCount--;
            SZrTypeValue result = object->key;
            ZrMemoryRawFreeWithType(state->global, object, sizeof(SZrHashKeyValuePair), ZR_VALUE_TYPE_VM_MEMORY);
            return result;
        }
        prev = object;
        object = object->next;
    }
    return state->global->nullValue;
}

ZR_FORCE_INLINE SZrHashKeyValuePair *ZrHashSetFind(struct SZrState *state, SZrHashSet *set,
                                                   const SZrTypeValue *element) {
    TUInt64 hash = ZrValueGetHash(state, element);
    SZrHashKeyValuePair *object = ZrHashSetGetBucket(set, hash);
    while (object != ZR_NULL) {
        if (ZrValueCompareDirectly(state, &object->key, element)) {
            return object;
        }
        object = object->next;
    }
    return ZR_NULL;
}

#endif // ZR_VM_CORE_HASH_SET_H
