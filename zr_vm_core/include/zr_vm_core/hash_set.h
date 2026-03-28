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
    TZrBool isValid;
};

typedef struct SZrHashSet SZrHashSet;

ZR_FORCE_INLINE void ZrCore_HashSet_Construct(SZrHashSet *set) { set->isValid = ZR_FALSE; }
ZR_CORE_API void ZrCore_HashSet_Deconstruct(struct SZrState *state, SZrHashSet *set);

ZR_CORE_API void ZrCore_HashSet_Rehash(struct SZrState *state, SZrHashSet *set, TZrSize newCapacity);

ZR_FORCE_INLINE void ZrCore_HashSet_Init(struct SZrState *state, SZrHashSet *set, TZrSize capacityLog2) {
    ZR_ASSERT(set != NULL && capacityLog2 != 0);
    const TZrSize capacity = (TZrSize) 1 << capacityLog2;
    set->buckets = ZR_NULL;
    set->elementCount = 0;
    set->capacity = 0;
    set->resizeThreshold = 0;
    ZrCore_HashSet_Rehash(state, set, capacity);
    set->isValid = ZR_TRUE;
}

ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_GetBucket(SZrHashSet *set, TZrUInt64 hash) {
    return set->buckets[ZR_HASH_MOD(hash, set->capacity)];
}
// add unique element, we do not check duplicated element
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_Add(struct SZrState *state, SZrHashSet *set,
                                                  const SZrTypeValue *element) {
    SZrGlobalState *global = state->global;
    if (set->elementCount + 1 > set->resizeThreshold) {
        ZrCore_HashSet_Rehash(state, set, set->capacity << 1);
    }
    TZrUInt64 hash = ZrCore_Value_GetHash(state, element);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    SZrHashKeyValuePair *hashElement = (SZrHashKeyValuePair *)
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrHashKeyValuePair), ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
    ZrCore_Value_Copy(state, &hashElement->key, element);
    ZrCore_Value_ResetAsNull(&hashElement->value);
    // object->next = hashElement;
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}
// we will also mark value as element
ZR_FORCE_INLINE SZrHashKeyValuePair *ZrCore_HashSet_AddRawObject(struct SZrState *state, SZrHashSet *set,
                                                           SZrRawObject *element) {
    SZrGlobalState *global = state->global;
    if (set->elementCount + 1 > set->resizeThreshold) {
        ZrCore_HashSet_Rehash(state, set, set->capacity << 1);
    }
    TZrUInt64 hash = element->hash;
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(set, hash);
    // todo: it should be managed by gc, how to bind it, stringTable and objectTable
    SZrHashKeyValuePair *hashElement = (SZrHashKeyValuePair *)
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrHashKeyValuePair), ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
    ZrCore_Value_InitAsRawObject(state, &hashElement->key, element);
    // object->next = hashElement;
    ZrCore_Value_ResetAsNull(&hashElement->value);
    hashElement->next = object;
    set->buckets[ZR_HASH_MOD(hash, set->capacity)] = hashElement;
    set->elementCount++;
    return hashElement;
}

ZR_FORCE_INLINE SZrTypeValue ZrCore_HashSet_Remove(struct SZrState *state, SZrHashSet *set, const SZrTypeValue *element) {
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
