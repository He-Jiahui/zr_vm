//
// Extracted super-array fast paths and hot object-field caches.
//

#include "object_super_array_internal.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include <string.h>

#if defined(_MSC_VER)
#define ZR_SUPER_ARRAY_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define ZR_SUPER_ARRAY_NOINLINE __attribute__((noinline))
#else
#define ZR_SUPER_ARRAY_NOINLINE
#endif

/*
 * Super-array helpers sit on the benchmark hot path; avoid paying helper
 * profiling branches for internal value moves and null writes here.
 */
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile

typedef struct SZrObjectLiteralStringCacheEntry {
    const TZrChar *literal;
    SZrString *stringObject;
} SZrObjectLiteralStringCacheEntry;

typedef struct SZrObjectHotLiteralCache {
    const SZrGlobalState *cachedGlobal;
    const void *cachedGarbageCollector;
    const void *cachedStringTable;
    SZrString *itemsField;
    SZrString *lengthField;
    SZrString *capacityField;
    SZrString *addMember;
} SZrObjectHotLiteralCache;

static ZR_FORCE_INLINE TZrBool object_value_is_plain_primitive(const SZrTypeValue *value) {
    ZR_ASSERT(value != ZR_NULL);
    return !value->isGarbageCollectable && ZrCore_Value_HasNormalizedNoOwnership(value);
}

static ZR_FORCE_INLINE TZrBool object_value_can_overwrite_without_release(const SZrTypeValue *value) {
    ZR_ASSERT(value != ZR_NULL);
    return ZrCore_Value_HasNormalizedNoOwnership(value);
}

static ZR_FORCE_INLINE void object_store_plain_int_reuse(SZrTypeValue *destination, TZrInt64 value) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(object_value_can_overwrite_without_release(destination));
    destination->type = ZR_VALUE_TYPE_INT64;
    destination->value.nativeObject.nativeInt64 = value;
    destination->isGarbageCollectable = ZR_FALSE;
    destination->isNative = ZR_TRUE;
    destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    destination->ownershipControl = ZR_NULL;
    destination->ownershipWeakRef = ZR_NULL;
}

static ZR_FORCE_INLINE void object_store_plain_null_reuse(SZrTypeValue *destination) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(object_value_can_overwrite_without_release(destination));
    destination->type = ZR_VALUE_TYPE_NULL;
    destination->value.nativeObject.nativeUInt64 = 0;
    destination->isGarbageCollectable = ZR_FALSE;
    destination->isNative = ZR_TRUE;
    destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    destination->ownershipControl = ZR_NULL;
    destination->ownershipWeakRef = ZR_NULL;
}

static ZR_FORCE_INLINE void object_assign_primitive_value_or_copy(SZrState *state,
                                                                  SZrTypeValue *destination,
                                                                  const SZrTypeValue *source) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    ZR_ASSERT(object_value_is_plain_primitive(source));

    if (object_value_can_overwrite_without_release(destination)) {
        *destination = *source;
        return;
    }

    ZrCore_Value_Copy(state, destination, source);
}

static ZR_FORCE_INLINE void object_assign_int_value_or_copy(SZrState *state,
                                                            SZrTypeValue *destination,
                                                            TZrInt64 value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);

    if (object_value_can_overwrite_without_release(destination)) {
        object_store_plain_int_reuse(destination, value);
        return;
    }

    {
        SZrTypeValue tempValue;
        ZrCore_Value_InitAsInt(state, &tempValue, value);
        ZrCore_Value_Copy(state, destination, &tempValue);
    }
}

static ZR_FORCE_INLINE TZrBool object_ensure_node_map_ready(SZrState *state, SZrObject *object) {
    if (object_node_map_is_ready(object)) {
        return ZR_TRUE;
    }

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Object_Init(state, object);
    return object_node_map_is_ready(object);
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_find_string_key_pair(SZrObject *object,
                                                                        SZrString *stringObject) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (!object_node_map_is_ready(object) || stringObject == ZR_NULL) {
        return ZR_NULL;
    }

    hash = ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject)->hash;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        if (pair->key.type == ZR_VALUE_TYPE_STRING &&
            pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject)) {
            return pair;
        }
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_find_equivalent_string_key_pair(SZrState *state,
                                                                                    SZrObject *object,
                                                                                    SZrString *stringObject) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (state == ZR_NULL || !object_node_map_is_ready(object) || stringObject == ZR_NULL) {
        return ZR_NULL;
    }

    hash = ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject)->hash;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        SZrString *pairString;

        if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
            continue;
        }
        if (pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject)) {
            return pair;
        }

        pairString = ZR_CAST_STRING(state, pair->key.value.object);
        if (pairString != ZR_NULL && ZrCore_String_Equal(pairString, stringObject)) {
            return pair;
        }
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_find_int_key_pair(SZrObject *object, TZrInt64 indexValue) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (!object_node_map_is_ready(object)) {
        return ZR_NULL;
    }

    hash = (TZrUInt64)indexValue;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) && pair->key.value.nativeObject.nativeInt64 == indexValue) {
            return pair;
        }
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool object_literal_matches(const TZrChar *literal, const TZrChar *target) {
    return literal == target || (literal != ZR_NULL && target != ZR_NULL && strcmp(literal, target) == 0);
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_empty_pair_for_hash_assume_ready(SZrState *state,
                                                                                         SZrObject *object,
                                                                                         TZrUInt64 hash) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize bucketIndex;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(object));

    nodeMap = &object->nodeMap;
    if (nodeMap->elementCount + 1 > nodeMap->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, nodeMap, nodeMap->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    bucketIndex = ZR_HASH_MOD(hash, nodeMap->capacity);
    pair = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                         ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                         sizeof(SZrHashKeyValuePair));
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    pair->next = nodeMap->buckets[bucketIndex];
    nodeMap->buckets[bucketIndex] = pair;
    nodeMap->elementCount++;
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_empty_pair_for_hash_assume_reserved(SZrState *state,
                                                                                            SZrObject *object,
                                                                                            TZrUInt64 hash) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize bucketIndex;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(object));
    ZR_UNUSED_PARAMETER(state);

    nodeMap = &object->nodeMap;
    ZR_ASSERT(nodeMap->elementCount + 1 <= nodeMap->resizeThreshold);

    bucketIndex = ZR_HASH_MOD(hash, nodeMap->capacity);
    pair = ZrCore_HashSet_TakeReservedPairAssumeAvailable(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    pair->next = nodeMap->buckets[bucketIndex];
    nodeMap->buckets[bucketIndex] = pair;
    nodeMap->elementCount++;
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_empty_pair_for_hash(SZrState *state,
                                                                           SZrObject *object,
                                                                           TZrUInt64 hash) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || !object_ensure_node_map_ready(state, object)) {
        return ZR_NULL;
    }

    pair = object_add_empty_pair_for_hash_assume_ready(state, object, hash);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_ResetAsNull(&pair->key);
    ZrCore_Value_ResetAsNull(&pair->value);
    return pair;
}

static SZrHashKeyValuePair *object_find_or_add_string_key_pair(SZrState *state,
                                                               SZrObject *object,
                                                               SZrString *stringObject) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || stringObject == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object_find_string_key_pair(object, stringObject);
    if (pair == ZR_NULL) {
        pair = object_find_equivalent_string_key_pair(state, object, stringObject);
    }
    if (pair != ZR_NULL) {
        return pair;
    }

    pair = object_add_empty_pair_for_hash(state, object, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject)->hash);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &pair->key, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    pair->key.type = ZR_VALUE_TYPE_STRING;
    return pair;
}

static SZrHashKeyValuePair *object_find_or_add_int_key_pair(SZrState *state,
                                                            SZrObject *object,
                                                            TZrInt64 indexValue) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object_find_int_key_pair(object, indexValue);
    if (pair != ZR_NULL) {
        return pair;
    }

    hash = (TZrUInt64)indexValue;
    pair = object_add_empty_pair_for_hash(state, object, hash);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_int_int_pair_assuming_absent_ready(SZrState *state,
                                                                                           SZrObject *object,
                                                                                           TZrInt64 indexValue,
                                                                                           TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(object));

    pair = object_add_empty_pair_for_hash_assume_reserved(state, object, (TZrUInt64)indexValue);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_super_array_dense_pair_at(SZrObject *itemsObject,
                                                                              TZrInt64 indexValue) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    if (itemsObject == ZR_NULL || !object_node_map_is_ready(itemsObject) || indexValue < 0) {
        return ZR_NULL;
    }

    nodeMap = &itemsObject->nodeMap;
    if ((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->capacity) {
        return ZR_NULL;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair == ZR_NULL ||
              (pair->next == ZR_NULL &&
               ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
               pair->key.value.nativeObject.nativeInt64 == indexValue));
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_super_array_dense_pair_at_assume_fast(
        SZrObject *itemsObject,
        TZrInt64 indexValue) {
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(itemsObject));
    ZR_ASSERT(indexValue >= 0);
    ZR_ASSERT((TZrUInt64)indexValue < (TZrUInt64)itemsObject->nodeMap.capacity);

    pair = itemsObject->nodeMap.buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair == ZR_NULL ||
              (pair->next == ZR_NULL &&
               ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
               pair->key.value.nativeObject.nativeInt64 == indexValue));
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_int_int_pair_assuming_absent_dense_ready(
        SZrState *state,
        SZrObject *object,
        TZrInt64 indexValue,
        TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || !object_node_map_is_ready(object) || indexValue < 0) {
        return ZR_NULL;
    }

    nodeMap = &object->nodeMap;
    if ((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->capacity ||
        nodeMap->elementCount + 1 > nodeMap->resizeThreshold ||
        nodeMap->pairPoolUsed + 1 > nodeMap->pairPoolCapacity ||
        object_super_array_dense_pair_at(object, indexValue) != ZR_NULL) {
        return ZR_NULL;
    }

    pair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    nodeMap->buckets[(TZrSize)indexValue] = pair;
    nodeMap->elementCount++;
    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(
        SZrState *state,
        SZrObject *object,
        TZrInt64 indexValue,
        TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(object));
    ZR_ASSERT(indexValue >= 0);
    ZR_UNUSED_PARAMETER(state);

    nodeMap = &object->nodeMap;
    ZR_ASSERT((TZrUInt64)indexValue < (TZrUInt64)nodeMap->capacity);
    ZR_ASSERT(nodeMap->elementCount + 1 <= nodeMap->resizeThreshold);
    ZR_ASSERT(nodeMap->pairPoolUsed + 1 <= nodeMap->pairPoolCapacity);
    ZR_ASSERT(nodeMap->buckets[(TZrSize)indexValue] == ZR_NULL);

    pair = ZrCore_HashSet_TakeReservedPairAssumeAvailable(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    nodeMap->buckets[(TZrSize)indexValue] = pair;
    nodeMap->elementCount++;
    return pair;
}

static ZR_FORCE_INLINE void object_init_dense_int_int_pair_assume_fast(
        SZrHashKeyValuePair *pair,
        const SZrTypeValue *valueTemplate,
        TZrInt64 indexValue) {
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(valueTemplate != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(valueTemplate->type));
    ZR_ASSERT(object_value_is_plain_primitive(valueTemplate));

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    pair->value = *valueTemplate;
}

static ZR_FORCE_INLINE void object_init_dense_int_int_pair_and_bucket_assume_fast(
        SZrHashKeyValuePair *pair,
        SZrHashKeyValuePair **bucketSlot,
        const SZrTypeValue *valueTemplate,
        TZrInt64 indexValue) {
    ZR_ASSERT(bucketSlot != ZR_NULL);
    ZR_ASSERT(*bucketSlot == ZR_NULL);
    object_init_dense_int_int_pair_assume_fast(pair, valueTemplate, indexValue);
    *bucketSlot = pair;
}

static ZR_FORCE_INLINE TZrBool object_add_int_int_pairs_assuming_absent_dense_ready_assume_fast(
        SZrState *state,
        SZrObject *object,
        TZrSize startIndex,
        TZrSize pairCount,
        TZrInt64 value) {
    SZrHashSet *nodeMap;
    TZrSize remaining;
    TZrSize currentIndex;
    SZrTypeValue valueTemplate;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(object));
    ZR_ASSERT(pairCount > 0);
    ZR_UNUSED_PARAMETER(state);

    nodeMap = &object->nodeMap;
    ZR_ASSERT(startIndex + pairCount <= nodeMap->capacity);
    ZR_ASSERT(nodeMap->elementCount + pairCount <= nodeMap->resizeThreshold);
    ZR_ASSERT(nodeMap->pairPoolUsed + pairCount <= nodeMap->pairPoolCapacity);
    ZR_VALUE_FAST_SET(&valueTemplate, nativeInt64, value, ZR_VALUE_TYPE_INT64);

    remaining = pairCount;
    currentIndex = startIndex;
    while (remaining > 0) {
        SZrHashKeyValuePair *pairs;
        SZrHashKeyValuePair *pairCursor;
        SZrHashKeyValuePair *pairEnd;
        SZrHashKeyValuePair **bucketCursor;
        TZrSize spanCount = remaining;
        TZrInt64 currentIndexValue;

        pairs = ZrCore_HashSet_TakeReservedPairSpanAssumeAvailable(nodeMap, &spanCount);
        if (pairs == ZR_NULL || spanCount == 0) {
            return ZR_FALSE;
        }

        bucketCursor = &nodeMap->buckets[currentIndex];
        pairCursor = pairs;
        pairEnd = pairs + spanCount;
        currentIndexValue = (TZrInt64)currentIndex;
        while ((TZrSize)(pairEnd - pairCursor) >= 4) {
            ZR_ASSERT(bucketCursor[0] == ZR_NULL);
            ZR_ASSERT(bucketCursor[1] == ZR_NULL);
            ZR_ASSERT(bucketCursor[2] == ZR_NULL);
            ZR_ASSERT(bucketCursor[3] == ZR_NULL);

            pairCursor[0].next = ZR_NULL;
            ZR_VALUE_FAST_SET(&pairCursor[0].key, nativeInt64, currentIndexValue, ZR_VALUE_TYPE_INT64);
            pairCursor[0].value = valueTemplate;
            bucketCursor[0] = &pairCursor[0];

            pairCursor[1].next = ZR_NULL;
            ZR_VALUE_FAST_SET(&pairCursor[1].key, nativeInt64, currentIndexValue + 1, ZR_VALUE_TYPE_INT64);
            pairCursor[1].value = valueTemplate;
            bucketCursor[1] = &pairCursor[1];

            pairCursor[2].next = ZR_NULL;
            ZR_VALUE_FAST_SET(&pairCursor[2].key, nativeInt64, currentIndexValue + 2, ZR_VALUE_TYPE_INT64);
            pairCursor[2].value = valueTemplate;
            bucketCursor[2] = &pairCursor[2];

            pairCursor[3].next = ZR_NULL;
            ZR_VALUE_FAST_SET(&pairCursor[3].key, nativeInt64, currentIndexValue + 3, ZR_VALUE_TYPE_INT64);
            pairCursor[3].value = valueTemplate;
            bucketCursor[3] = &pairCursor[3];

            pairCursor += 4;
            bucketCursor += 4;
            currentIndexValue += 4;
        }

        for (; pairCursor != pairEnd; ++pairCursor, ++bucketCursor, ++currentIndexValue) {
            ZR_ASSERT(*bucketCursor == ZR_NULL);
            object_init_dense_int_int_pair_assume_fast(pairCursor, &valueTemplate, currentIndexValue);
            *bucketCursor = pairCursor;
        }

        nodeMap->elementCount += spanCount;
        currentIndex += spanCount;
        remaining -= spanCount;
    }

    return ZR_TRUE;
}

static SZrString *object_cached_string_literal(SZrState *state, const TZrChar *literal) {
    static const SZrGlobalState *cachedGlobal = ZR_NULL;
    static const void *cachedGarbageCollector = ZR_NULL;
    static const void *cachedStringTable = ZR_NULL;
    static SZrObjectLiteralStringCacheEntry cache[ZR_OBJECT_LITERAL_CACHE_CAPACITY];

    if (state == ZR_NULL || state->global == ZR_NULL || literal == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobal != state->global ||
        cachedGarbageCollector != state->global->garbageCollector ||
        cachedStringTable != state->global->stringTable) {
        cachedGlobal = state->global;
        cachedGarbageCollector = state->global->garbageCollector;
        cachedStringTable = state->global->stringTable;
        memset(cache, 0, sizeof(cache));
    }

    for (TZrSize index = 0; index < ZR_OBJECT_LITERAL_CACHE_CAPACITY; index++) {
        if (cache[index].literal == literal) {
            return cache[index].stringObject;
        }
        if (cache[index].literal != ZR_NULL && strcmp(cache[index].literal, literal) == 0) {
            cache[index].literal = literal;
            return cache[index].stringObject;
        }
    }

    for (TZrSize index = 0; index < ZR_OBJECT_LITERAL_CACHE_CAPACITY; index++) {
        SZrString *stringObject;

        if (cache[index].literal != ZR_NULL) {
            continue;
        }

        stringObject = ZrCore_String_Create(state, (TZrNativeString)literal, strlen(literal));
        if (stringObject == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));

        cache[index].literal = literal;
        cache[index].stringObject = stringObject;
        return stringObject;
    }

    ZR_ASSERT(ZR_FALSE);
    return ZR_NULL;
}

static void object_hot_literal_cache_reset_if_needed(SZrState *state, SZrObjectHotLiteralCache *cache) {
    if (state == ZR_NULL || state->global == ZR_NULL || cache == ZR_NULL) {
        return;
    }

    if (cache->cachedGlobal != state->global ||
        cache->cachedGarbageCollector != state->global->garbageCollector ||
        cache->cachedStringTable != state->global->stringTable) {
        memset(cache, 0, sizeof(*cache));
        cache->cachedGlobal = state->global;
        cache->cachedGarbageCollector = state->global->garbageCollector;
        cache->cachedStringTable = state->global->stringTable;
    }
}

static SZrString *object_cached_hot_literal_slot(SZrState *state, SZrString **slot, const TZrChar *literal) {
    if (state == ZR_NULL || slot == ZR_NULL || literal == ZR_NULL) {
        return ZR_NULL;
    }

    if (*slot == ZR_NULL) {
        *slot = object_cached_string_literal(state, literal);
    }
    return *slot;
}

SZrString *ZrCore_Object_CachedKnownFieldString(SZrState *state, const TZrChar *literal) {
    static SZrObjectHotLiteralCache cache;

    if (state == ZR_NULL || literal == ZR_NULL) {
        return ZR_NULL;
    }

    object_hot_literal_cache_reset_if_needed(state, &cache);
    if (object_literal_matches(literal, ZR_OBJECT_HIDDEN_ITEMS_FIELD)) {
        return object_cached_hot_literal_slot(state, &cache.itemsField, literal);
    }
    if (object_literal_matches(literal, ZR_OBJECT_ARRAY_LENGTH_FIELD)) {
        return object_cached_hot_literal_slot(state, &cache.lengthField, literal);
    }
    if (object_literal_matches(literal, ZR_OBJECT_ARRAY_CAPACITY_FIELD)) {
        return object_cached_hot_literal_slot(state, &cache.capacityField, literal);
    }
    if (object_literal_matches(literal, ZR_OBJECT_ARRAY_ADD_MEMBER)) {
        return object_cached_hot_literal_slot(state, &cache.addMember, literal);
    }

    return object_cached_string_literal(state, literal);
}

typedef enum EZrSuperArrayFieldKind {
    ZR_SUPER_ARRAY_FIELD_HIDDEN_ITEMS = 0,
    ZR_SUPER_ARRAY_FIELD_LENGTH,
    ZR_SUPER_ARRAY_FIELD_CAPACITY,
} EZrSuperArrayFieldKind;

static ZR_FORCE_INLINE SZrHashKeyValuePair **object_super_array_field_slot(SZrObject *object,
                                                                           EZrSuperArrayFieldKind fieldKind) {
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    switch (fieldKind) {
        case ZR_SUPER_ARRAY_FIELD_HIDDEN_ITEMS:
            return &object->cachedHiddenItemsPair;
        case ZR_SUPER_ARRAY_FIELD_LENGTH:
            return &object->cachedLengthPair;
        case ZR_SUPER_ARRAY_FIELD_CAPACITY:
            return &object->cachedCapacityPair;
        default:
            ZR_ASSERT(ZR_FALSE);
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE const TZrChar *object_super_array_field_literal(EZrSuperArrayFieldKind fieldKind) {
    switch (fieldKind) {
        case ZR_SUPER_ARRAY_FIELD_HIDDEN_ITEMS:
            return ZR_OBJECT_HIDDEN_ITEMS_FIELD;
        case ZR_SUPER_ARRAY_FIELD_LENGTH:
            return ZR_OBJECT_ARRAY_LENGTH_FIELD;
        case ZR_SUPER_ARRAY_FIELD_CAPACITY:
            return ZR_OBJECT_ARRAY_CAPACITY_FIELD;
        default:
            ZR_ASSERT(ZR_FALSE);
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_resolve_super_array_field_pair(SZrState *state,
                                                                                  SZrObject *object,
                                                                                  EZrSuperArrayFieldKind fieldKind,
                                                                                  TZrBool addIfMissing) {
    SZrHashKeyValuePair **slot;
    SZrHashKeyValuePair *pair;
    SZrString *keyString;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    slot = object_super_array_field_slot(object, fieldKind);
    if (slot == ZR_NULL) {
        return ZR_NULL;
    }

    pair = *slot;
    if (pair != ZR_NULL) {
        return pair;
    }

    keyString = ZrCore_Object_CachedKnownFieldString(state, object_super_array_field_literal(fieldKind));
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }

    pair = addIfMissing ? object_find_or_add_string_key_pair(state, object, keyString)
                        : object_find_string_key_pair(object, keyString);
    if (pair == ZR_NULL && !addIfMissing) {
        pair = object_find_equivalent_string_key_pair(state, object, keyString);
    }
    if (pair != ZR_NULL) {
        *slot = pair;
    }
    return pair;
}

static ZR_FORCE_INLINE TZrBool object_get_super_array_field_value_ref(SZrState *state,
                                                                      SZrObject *object,
                                                                      EZrSuperArrayFieldKind fieldKind,
                                                                      const SZrTypeValue **outValue) {
    SZrHashKeyValuePair *pair;

    if (outValue != ZR_NULL) {
        *outValue = ZR_NULL;
    }
    if (state == ZR_NULL || object == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = object_resolve_super_array_field_pair(state, object, fieldKind, ZR_FALSE);
    *outValue = pair != ZR_NULL ? &pair->value : ZR_NULL;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrInt64 object_super_array_pair_int_or_default(const SZrHashKeyValuePair *pair,
                                                                       TZrInt64 defaultValue) {
    const SZrTypeValue *value;

    if (pair == ZR_NULL) {
        return defaultValue;
    }

    value = &pair->value;
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

static ZR_FORCE_INLINE TZrInt64 object_get_super_array_int_field_or_default(SZrState *state,
                                                                            SZrObject *object,
                                                                            EZrSuperArrayFieldKind fieldKind,
                                                                            TZrInt64 defaultValue) {
    const SZrTypeValue *value = ZR_NULL;

    if (!object_get_super_array_field_value_ref(state, object, fieldKind, &value) || value == ZR_NULL) {
        return defaultValue;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

static ZR_FORCE_INLINE TZrBool object_try_super_array_set_cached_int_field(SZrState *state,
                                                                           SZrObject *object,
                                                                           EZrSuperArrayFieldKind fieldKind,
                                                                           TZrInt64 value) {
    SZrHashKeyValuePair **slot;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    slot = object_super_array_field_slot(object, fieldKind);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = *slot;
    if (pair == ZR_NULL) {
        pair = object_resolve_super_array_field_pair(state, object, fieldKind, ZR_TRUE);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    if (object_value_can_overwrite_without_release(&pair->value)) {
        ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
        pair->value.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
        pair->value.ownershipControl = ZR_NULL;
        pair->value.ownershipWeakRef = ZR_NULL;
    } else {
        object_assign_int_value_or_copy(state, &pair->value, value);
    }
    object->memberVersion++;
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_try_super_array_set_cached_int_field_assume_fast(
        SZrState *state,
        SZrObject *object,
        SZrHashKeyValuePair **cachedPairSlot,
        EZrSuperArrayFieldKind fieldKind,
        TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(cachedPairSlot != ZR_NULL);

    pair = *cachedPairSlot;
    if (pair == ZR_NULL) {
        pair = object_resolve_super_array_field_pair(state, object, fieldKind, ZR_TRUE);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    if (object_value_can_overwrite_without_release(&pair->value)) {
        object_store_plain_int_reuse(&pair->value, value);
    } else {
        object_assign_int_value_or_copy(state, &pair->value, value);
    }
    object->memberVersion++;
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static TZrBool object_try_resolve_super_array_items(SZrState *state,
                                                    SZrTypeValue *receiver,
                                                    SZrObject **outReceiverObject,
                                                    SZrObject **outItemsObject,
                                                    TZrBool *outApplicable) {
    SZrObject *receiverObject;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *itemsValue = ZR_NULL;
    SZrObject *itemsObject;

    if (outReceiverObject != ZR_NULL) {
        *outReceiverObject = ZR_NULL;
    }
    if (outItemsObject != ZR_NULL) {
        *outItemsObject = ZR_NULL;
    }
    if (outApplicable != ZR_NULL) {
        *outApplicable = ZR_FALSE;
    }

    if (state == ZR_NULL || receiver == ZR_NULL || outItemsObject == ZR_NULL || outApplicable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_TRUE;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiver->value.object);
    if (receiverObject == ZR_NULL) {
        return ZR_TRUE;
    }

    itemsObject = receiverObject->cachedHiddenItemsObject;
    if (itemsObject != ZR_NULL) {
        if (itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
            if (outReceiverObject != ZR_NULL) {
                *outReceiverObject = receiverObject;
            }
            *outItemsObject = itemsObject;
            *outApplicable = ZR_TRUE;
            return ZR_TRUE;
        }

        receiverObject->cachedHiddenItemsObject = ZR_NULL;
    }

    prototype = receiverObject->prototype;
    if (prototype == ZR_NULL && state->global != ZR_NULL) {
        prototype = state->global->basicTypeObjectPrototype[receiver->type];
    }
    if (prototype == ZR_NULL || (prototype->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE)) == 0) {
        return ZR_TRUE;
    }

    if (!object_get_super_array_field_value_ref(state,
                                                receiverObject,
                                                ZR_SUPER_ARRAY_FIELD_HIDDEN_ITEMS,
                                                &itemsValue)) {
        return ZR_FALSE;
    }
    if (itemsValue == ZR_NULL ||
        (itemsValue->type != ZR_VALUE_TYPE_OBJECT && itemsValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_TRUE;
    }

    itemsObject = ZR_CAST_OBJECT(state, itemsValue->value.object);
    if (itemsObject == ZR_NULL || itemsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_TRUE;
    }

    receiverObject->cachedHiddenItemsObject = itemsObject;
    if (outReceiverObject != ZR_NULL) {
        *outReceiverObject = receiverObject;
    }
    *outItemsObject = itemsObject;
    *outApplicable = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool object_try_resolve_super_array_storage(SZrState *state,
                                                      SZrTypeValue *receiver,
                                                      const SZrTypeValue *key,
                                                      SZrObject **outItemsObject,
                                                      TZrInt64 *outIndexValue,
                                                      TZrBool *outApplicable) {
    if (outItemsObject != ZR_NULL) {
        *outItemsObject = ZR_NULL;
    }
    if (outIndexValue != ZR_NULL) {
        *outIndexValue = 0;
    }
    if (outApplicable != ZR_NULL) {
        *outApplicable = ZR_FALSE;
    }

    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || outItemsObject == ZR_NULL ||
        outIndexValue == ZR_NULL || outApplicable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_INT(key->type)) {
        return ZR_TRUE;
    }
    if (!object_try_resolve_super_array_items(state, receiver, ZR_NULL, outItemsObject, outApplicable)) {
        return ZR_FALSE;
    }
    if (!*outApplicable) {
        return ZR_TRUE;
    }

    *outIndexValue = key->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

ZR_SUPER_ARRAY_NOINLINE TZrBool ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(SZrState *state,
                                                                                    SZrTypeValue *receiver,
                                                                                    SZrObject **outReceiverObject,
                                                                                    SZrObject **outItemsObject) {
    TZrBool applicable = ZR_FALSE;

    if (!object_try_resolve_super_array_items(state,
                                              receiver,
                                              outReceiverObject,
                                              outItemsObject,
                                              &applicable)) {
        return ZR_FALSE;
    }

    return applicable;
}

typedef struct ZrSuperArrayAppendPlan {
    SZrObject *receiverObject;
    SZrObject *itemsObject;
    TZrSize length;
    TZrSize requiredLength;
} ZrSuperArrayAppendPlan;

typedef struct ZrSuperArrayAppendBatchCursor {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair **bucketCursor;
    TZrInt64 currentIndexValue;
} ZrSuperArrayAppendBatchCursor;

static ZR_FORCE_INLINE TZrSize object_super_array_target_pair_pool_capacity(const SZrHashSet *nodeMap,
                                                                            TZrSize requiredAfterAppend,
                                                                            TZrSize targetCapacity) {
    TZrSize pairPoolTarget;
    TZrSize grownCapacity;

    ZR_ASSERT(nodeMap != ZR_NULL);
    ZR_ASSERT(requiredAfterAppend > 0);

    pairPoolTarget = requiredAfterAppend;
    if (nodeMap->pairPoolCapacity > 0) {
        grownCapacity = nodeMap->pairPoolCapacity;
        if (grownCapacity > ((TZrSize)-1) / ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR) {
            grownCapacity = (TZrSize)-1;
        } else {
            grownCapacity *= ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR;
        }

        if (grownCapacity > pairPoolTarget) {
            pairPoolTarget = grownCapacity;
        }
    }

    if (pairPoolTarget > targetCapacity) {
        pairPoolTarget = targetCapacity;
    }
    if (pairPoolTarget < requiredAfterAppend) {
        pairPoolTarget = requiredAfterAppend;
    }
    return pairPoolTarget;
}

static ZR_FORCE_INLINE TZrBool object_try_super_array_ensure_capacity_for_append(SZrState *state,
                                                                                 SZrObject *receiverObject,
                                                                                 SZrObject *itemsObject,
                                                                                 TZrSize requiredLength,
                                                                                 TZrSize appendCount) {
    SZrHashKeyValuePair *capacityPair;
    TZrInt64 capacity;
    TZrInt64 targetCapacity;
    TZrSize pairPoolUsedAfterAppend;
    TZrSize pairPoolTargetCapacity;

    if (state == ZR_NULL || receiverObject == ZR_NULL || itemsObject == ZR_NULL || appendCount == 0) {
        return ZR_FALSE;
    }

    capacityPair = receiverObject->cachedCapacityPair;
    if ((TZrUInt64)appendCount >
        (TZrUInt64)((TZrSize)-1) - (TZrUInt64)itemsObject->nodeMap.pairPoolUsed) {
        pairPoolUsedAfterAppend = (TZrSize)-1;
    } else {
        pairPoolUsedAfterAppend = itemsObject->nodeMap.pairPoolUsed + appendCount;
    }
    if (capacityPair != ZR_NULL &&
        requiredLength <= itemsObject->nodeMap.resizeThreshold &&
        pairPoolUsedAfterAppend <= itemsObject->nodeMap.pairPoolCapacity &&
        object_super_array_pair_int_or_default(capacityPair, 0) >= (TZrInt64)requiredLength) {
        return ZR_TRUE;
    }

    capacity = capacityPair != ZR_NULL
                       ? object_super_array_pair_int_or_default(capacityPair, 0)
                       : object_get_super_array_int_field_or_default(state,
                                                                     receiverObject,
                                                                     ZR_SUPER_ARRAY_FIELD_CAPACITY,
                                                                     0);
    targetCapacity = capacity;
    if (targetCapacity <= 0) {
        targetCapacity = ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY;
    }
    while ((TZrSize)targetCapacity < requiredLength) {
        targetCapacity *= ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR;
    }

    if (!object_ensure_node_map_ready(state, itemsObject)) {
        return ZR_FALSE;
    }

    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state,
                                                            &itemsObject->nodeMap,
                                                            (TZrSize)targetCapacity)) {
        return ZR_FALSE;
    }

    pairPoolTargetCapacity = object_super_array_target_pair_pool_capacity(&itemsObject->nodeMap,
                                                                          pairPoolUsedAfterAppend,
                                                                          (TZrSize)targetCapacity);
    if (itemsObject->nodeMap.pairPoolCapacity < pairPoolTargetCapacity &&
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state,
                                                      &itemsObject->nodeMap,
                                                      pairPoolTargetCapacity)) {
        return ZR_FALSE;
    }

    if (targetCapacity == capacity) {
        return ZR_TRUE;
    }

    return object_try_super_array_set_cached_int_field_assume_fast(state,
                                                                   receiverObject,
                                                                   &receiverObject->cachedCapacityPair,
                                                                   ZR_SUPER_ARRAY_FIELD_CAPACITY,
                                                                   targetCapacity);
}

static ZR_FORCE_INLINE TZrBool object_super_array_update_cached_length_assume_fast(SZrState *state,
                                                                                   SZrObject *receiverObject,
                                                                                   TZrSize requiredLength) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiverObject != ZR_NULL);

    if (receiverObject->cachedLengthPair != ZR_NULL) {
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(receiverObject->cachedLengthPair->value.type));
        ZR_ASSERT(object_value_is_plain_primitive(&receiverObject->cachedLengthPair->value));
        object_store_plain_int_reuse(&receiverObject->cachedLengthPair->value, (TZrInt64)requiredLength);
        receiverObject->memberVersion++;
        return ZR_TRUE;
    }

    return object_try_super_array_set_cached_int_field_assume_fast(state,
                                                                   receiverObject,
                                                                   &receiverObject->cachedLengthPair,
                                                                   ZR_SUPER_ARRAY_FIELD_LENGTH,
                                                                   (TZrInt64)requiredLength);
}

static ZR_FORCE_INLINE TZrBool object_super_array_can_append_dense_int_assume_fast(
        SZrObject *receiverObject,
        SZrObject *itemsObject,
        TZrSize length) {
    const SZrHashSet *nodeMap;
    const SZrHashKeyValuePair *capacityPair;
    TZrSize requiredLength;

    ZR_ASSERT(receiverObject != ZR_NULL);
    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(itemsObject));

    nodeMap = &itemsObject->nodeMap;
    capacityPair = receiverObject->cachedCapacityPair;
    requiredLength = length + 1;
    ZR_ASSERT(capacityPair == ZR_NULL || ZR_VALUE_IS_TYPE_SIGNED_INT(capacityPair->value.type));
    return capacityPair != ZR_NULL &&
           requiredLength <= nodeMap->resizeThreshold &&
           nodeMap->pairPoolUsed < nodeMap->pairPoolCapacity &&
           length < nodeMap->capacity &&
           capacityPair->value.value.nativeObject.nativeInt64 >= (TZrInt64)requiredLength;
}

static ZR_FORCE_INLINE TZrBool object_super_array_can_append_dense_int_batch_assume_fast(
        SZrObject *receiverObject,
        SZrObject *itemsObject,
        TZrSize length,
        TZrSize appendCount) {
    const SZrHashSet *nodeMap;
    const SZrHashKeyValuePair *capacityPair;
    TZrSize requiredLength;

    ZR_ASSERT(receiverObject != ZR_NULL);
    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(object_node_map_is_ready(itemsObject));

    nodeMap = &itemsObject->nodeMap;
    capacityPair = receiverObject->cachedCapacityPair;
    if ((TZrUInt64)appendCount > (TZrUInt64)((TZrSize)-1) - (TZrUInt64)length) {
        return ZR_FALSE;
    }

    requiredLength = length + appendCount;
    ZR_ASSERT(capacityPair == ZR_NULL || ZR_VALUE_IS_TYPE_SIGNED_INT(capacityPair->value.type));
    return capacityPair != ZR_NULL &&
           requiredLength <= nodeMap->resizeThreshold &&
           nodeMap->pairPoolUsed + appendCount <= nodeMap->pairPoolCapacity &&
           requiredLength <= nodeMap->capacity &&
           capacityPair->value.value.nativeObject.nativeInt64 >= (TZrInt64)requiredLength;
}

static ZR_FORCE_INLINE TZrBool object_super_array_prepare_append_plan_assume_fast(SZrState *state,
                                                                                   SZrTypeValue *receiver,
                                                                                   TZrSize appendCount,
                                                                                   ZrSuperArrayAppendPlan *plan) {
    SZrObject *receiverObject = ZR_NULL;
    SZrObject *itemsObject = ZR_NULL;
    TZrSize length;
    TZrSize requiredLength;

    if (plan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);

    if (!zr_super_array_resolve_items_cached_assume_fast(state, receiver, &receiverObject, &itemsObject)) {
        return ZR_FALSE;
    }

    length = itemsObject->nodeMap.elementCount;
    if ((TZrUInt64)appendCount > (TZrUInt64)((TZrSize)-1) - (TZrUInt64)length) {
        return ZR_FALSE;
    }

    requiredLength = length + appendCount;
    if (appendCount > 0 &&
        !object_super_array_can_append_dense_int_batch_assume_fast(receiverObject, itemsObject, length, appendCount) &&
        !object_try_super_array_ensure_capacity_for_append(state,
                                                           receiverObject,
                                                           itemsObject,
                                                           requiredLength,
                                                           appendCount)) {
        return ZR_FALSE;
    }

    plan->receiverObject = receiverObject;
    plan->itemsObject = itemsObject;
    plan->length = length;
    plan->requiredLength = requiredLength;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrSize object_super_array_peek_reserved_pair_span_assume_available(SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(set->pairPoolUsed < set->pairPoolCapacity);

    block = set->pairPoolActive;
    if (!(ZR_LIKELY(block != ZR_NULL && block->used < block->capacity))) {
        while (block != ZR_NULL && block->used >= block->capacity) {
            block = block->next;
        }
        ZR_ASSERT(block != ZR_NULL);
        if (block == ZR_NULL) {
            return 0;
        }
        set->pairPoolActive = block;
    }

    return block->capacity - block->used;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_super_array_take_reserved_pair_span_exact_assume_available(
        SZrHashSet *set,
        TZrSize count) {
    SZrHashKeyValuePair *pairs;
    TZrSize takeCount;

    ZR_ASSERT(set != ZR_NULL);
    ZR_ASSERT(count > 0);

    takeCount = count;
    pairs = ZrCore_HashSet_TakeReservedPairSpanAssumeAvailable(set, &takeCount);
    ZR_ASSERT(pairs != ZR_NULL);
    ZR_ASSERT(takeCount == count);
    if (pairs == ZR_NULL || takeCount != count) {
        return ZR_NULL;
    }

    return pairs;
}

static ZR_FORCE_INLINE TZrBool object_super_array_update_cached_lengths4_assume_fast(
        SZrState *state,
        const ZrSuperArrayAppendPlan *plans) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(plans != ZR_NULL);

    return object_super_array_update_cached_length_assume_fast(state,
                                                               plans[0].receiverObject,
                                                               plans[0].requiredLength) &&
           object_super_array_update_cached_length_assume_fast(state,
                                                               plans[1].receiverObject,
                                                               plans[1].requiredLength) &&
           object_super_array_update_cached_length_assume_fast(state,
                                                               plans[2].receiverObject,
                                                               plans[2].requiredLength) &&
           object_super_array_update_cached_length_assume_fast(state,
                                                               plans[3].receiverObject,
                                                               plans[3].requiredLength);
}

static ZR_FORCE_INLINE TZrBool object_add_int_int_pairs4_assuming_absent_dense_ready_assume_fast(
        SZrState *state,
        const ZrSuperArrayAppendPlan *plans,
        TZrSize pairCount,
        TZrInt64 value) {
    ZrSuperArrayAppendBatchCursor cursor0;
    ZrSuperArrayAppendBatchCursor cursor1;
    ZrSuperArrayAppendBatchCursor cursor2;
    ZrSuperArrayAppendBatchCursor cursor3;
    SZrTypeValue valueTemplate;
    TZrSize remaining;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(plans != ZR_NULL);
    ZR_ASSERT(pairCount > 0);
    ZR_UNUSED_PARAMETER(state);

    cursor0.nodeMap = &plans[0].itemsObject->nodeMap;
    cursor1.nodeMap = &plans[1].itemsObject->nodeMap;
    cursor2.nodeMap = &plans[2].itemsObject->nodeMap;
    cursor3.nodeMap = &plans[3].itemsObject->nodeMap;
    cursor0.bucketCursor = &cursor0.nodeMap->buckets[plans[0].length];
    cursor1.bucketCursor = &cursor1.nodeMap->buckets[plans[1].length];
    cursor2.bucketCursor = &cursor2.nodeMap->buckets[plans[2].length];
    cursor3.bucketCursor = &cursor3.nodeMap->buckets[plans[3].length];
    cursor0.currentIndexValue = (TZrInt64)plans[0].length;
    cursor1.currentIndexValue = (TZrInt64)plans[1].length;
    cursor2.currentIndexValue = (TZrInt64)plans[2].length;
    cursor3.currentIndexValue = (TZrInt64)plans[3].length;
    ZR_ASSERT(plans[0].length + pairCount <= cursor0.nodeMap->capacity);
    ZR_ASSERT(plans[1].length + pairCount <= cursor1.nodeMap->capacity);
    ZR_ASSERT(plans[2].length + pairCount <= cursor2.nodeMap->capacity);
    ZR_ASSERT(plans[3].length + pairCount <= cursor3.nodeMap->capacity);
    ZR_ASSERT(cursor0.nodeMap->elementCount + pairCount <= cursor0.nodeMap->resizeThreshold);
    ZR_ASSERT(cursor1.nodeMap->elementCount + pairCount <= cursor1.nodeMap->resizeThreshold);
    ZR_ASSERT(cursor2.nodeMap->elementCount + pairCount <= cursor2.nodeMap->resizeThreshold);
    ZR_ASSERT(cursor3.nodeMap->elementCount + pairCount <= cursor3.nodeMap->resizeThreshold);
    ZR_ASSERT(cursor0.nodeMap->pairPoolUsed + pairCount <= cursor0.nodeMap->pairPoolCapacity);
    ZR_ASSERT(cursor1.nodeMap->pairPoolUsed + pairCount <= cursor1.nodeMap->pairPoolCapacity);
    ZR_ASSERT(cursor2.nodeMap->pairPoolUsed + pairCount <= cursor2.nodeMap->pairPoolCapacity);
    ZR_ASSERT(cursor3.nodeMap->pairPoolUsed + pairCount <= cursor3.nodeMap->pairPoolCapacity);
    ZR_VALUE_FAST_SET(&valueTemplate, nativeInt64, value, ZR_VALUE_TYPE_INT64);

    remaining = pairCount;
    while (remaining > 0) {
        TZrSize spanCount;
        SZrHashKeyValuePair *pairs0;
        SZrHashKeyValuePair *pairs1;
        SZrHashKeyValuePair *pairs2;
        SZrHashKeyValuePair *pairs3;
        SZrHashKeyValuePair **bucket0;
        SZrHashKeyValuePair **bucket1;
        SZrHashKeyValuePair **bucket2;
        SZrHashKeyValuePair **bucket3;
        TZrInt64 index0;
        TZrInt64 index1;
        TZrInt64 index2;
        TZrInt64 index3;
        TZrSize offset;

        spanCount = remaining;
        {
            TZrSize available = object_super_array_peek_reserved_pair_span_assume_available(cursor0.nodeMap);
            if (available == 0) {
                return ZR_FALSE;
            }
            if (available < spanCount) {
                spanCount = available;
            }
        }
        {
            TZrSize available = object_super_array_peek_reserved_pair_span_assume_available(cursor1.nodeMap);
            if (available == 0) {
                return ZR_FALSE;
            }
            if (available < spanCount) {
                spanCount = available;
            }
        }
        {
            TZrSize available = object_super_array_peek_reserved_pair_span_assume_available(cursor2.nodeMap);
            if (available == 0) {
                return ZR_FALSE;
            }
            if (available < spanCount) {
                spanCount = available;
            }
        }
        {
            TZrSize available = object_super_array_peek_reserved_pair_span_assume_available(cursor3.nodeMap);
            if (available == 0) {
                return ZR_FALSE;
            }
            if (available < spanCount) {
                spanCount = available;
            }
        }

        pairs0 = object_super_array_take_reserved_pair_span_exact_assume_available(cursor0.nodeMap, spanCount);
        pairs1 = object_super_array_take_reserved_pair_span_exact_assume_available(cursor1.nodeMap, spanCount);
        pairs2 = object_super_array_take_reserved_pair_span_exact_assume_available(cursor2.nodeMap, spanCount);
        pairs3 = object_super_array_take_reserved_pair_span_exact_assume_available(cursor3.nodeMap, spanCount);
        if (pairs0 == ZR_NULL || pairs1 == ZR_NULL || pairs2 == ZR_NULL || pairs3 == ZR_NULL) {
            return ZR_FALSE;
        }

        bucket0 = cursor0.bucketCursor;
        bucket1 = cursor1.bucketCursor;
        bucket2 = cursor2.bucketCursor;
        bucket3 = cursor3.bucketCursor;
        index0 = cursor0.currentIndexValue;
        index1 = cursor1.currentIndexValue;
        index2 = cursor2.currentIndexValue;
        index3 = cursor3.currentIndexValue;

        offset = 0;
        while (offset + 4 <= spanCount) {
            TZrInt64 offsetValue = (TZrInt64)offset;
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs0[offset],
                                                                  &bucket0[offset],
                                                                  &valueTemplate,
                                                                  index0 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs1[offset],
                                                                  &bucket1[offset],
                                                                  &valueTemplate,
                                                                  index1 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs2[offset],
                                                                  &bucket2[offset],
                                                                  &valueTemplate,
                                                                  index2 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs3[offset],
                                                                  &bucket3[offset],
                                                                  &valueTemplate,
                                                                  index3 + offsetValue);

            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs0[offset + 1],
                                                                  &bucket0[offset + 1],
                                                                  &valueTemplate,
                                                                  index0 + offsetValue + 1);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs1[offset + 1],
                                                                  &bucket1[offset + 1],
                                                                  &valueTemplate,
                                                                  index1 + offsetValue + 1);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs2[offset + 1],
                                                                  &bucket2[offset + 1],
                                                                  &valueTemplate,
                                                                  index2 + offsetValue + 1);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs3[offset + 1],
                                                                  &bucket3[offset + 1],
                                                                  &valueTemplate,
                                                                  index3 + offsetValue + 1);

            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs0[offset + 2],
                                                                  &bucket0[offset + 2],
                                                                  &valueTemplate,
                                                                  index0 + offsetValue + 2);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs1[offset + 2],
                                                                  &bucket1[offset + 2],
                                                                  &valueTemplate,
                                                                  index1 + offsetValue + 2);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs2[offset + 2],
                                                                  &bucket2[offset + 2],
                                                                  &valueTemplate,
                                                                  index2 + offsetValue + 2);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs3[offset + 2],
                                                                  &bucket3[offset + 2],
                                                                  &valueTemplate,
                                                                  index3 + offsetValue + 2);

            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs0[offset + 3],
                                                                  &bucket0[offset + 3],
                                                                  &valueTemplate,
                                                                  index0 + offsetValue + 3);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs1[offset + 3],
                                                                  &bucket1[offset + 3],
                                                                  &valueTemplate,
                                                                  index1 + offsetValue + 3);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs2[offset + 3],
                                                                  &bucket2[offset + 3],
                                                                  &valueTemplate,
                                                                  index2 + offsetValue + 3);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs3[offset + 3],
                                                                  &bucket3[offset + 3],
                                                                  &valueTemplate,
                                                                  index3 + offsetValue + 3);
            offset += 4;
        }

        for (; offset < spanCount; ++offset) {
            TZrInt64 offsetValue = (TZrInt64)offset;
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs0[offset],
                                                                  &bucket0[offset],
                                                                  &valueTemplate,
                                                                  index0 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs1[offset],
                                                                  &bucket1[offset],
                                                                  &valueTemplate,
                                                                  index1 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs2[offset],
                                                                  &bucket2[offset],
                                                                  &valueTemplate,
                                                                  index2 + offsetValue);
            object_init_dense_int_int_pair_and_bucket_assume_fast(&pairs3[offset],
                                                                  &bucket3[offset],
                                                                  &valueTemplate,
                                                                  index3 + offsetValue);
        }

        cursor0.nodeMap->elementCount += spanCount;
        cursor1.nodeMap->elementCount += spanCount;
        cursor2.nodeMap->elementCount += spanCount;
        cursor3.nodeMap->elementCount += spanCount;
        cursor0.bucketCursor = bucket0 + spanCount;
        cursor1.bucketCursor = bucket1 + spanCount;
        cursor2.bucketCursor = bucket2 + spanCount;
        cursor3.bucketCursor = bucket3 + spanCount;
        cursor0.currentIndexValue = index0 + (TZrInt64)spanCount;
        cursor1.currentIndexValue = index1 + (TZrInt64)spanCount;
        cursor2.currentIndexValue = index2 + (TZrInt64)spanCount;
        cursor3.currentIndexValue = index3 + (TZrInt64)spanCount;
        remaining -= spanCount;
    }

    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_super_array_commit_append_plan_assume_fast(SZrState *state,
                                                                                 const ZrSuperArrayAppendPlan *plan,
                                                                                 TZrInt64 value) {
    TZrSize appendCount;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(plan != ZR_NULL);
    ZR_ASSERT(plan->receiverObject != ZR_NULL);
    ZR_ASSERT(plan->itemsObject != ZR_NULL);
    ZR_ASSERT(plan->requiredLength >= plan->length);

    appendCount = plan->requiredLength - plan->length;
    if (appendCount == 0) {
        return ZR_TRUE;
    }

    if (appendCount == 1) {
        pair = object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(state,
                                                                               plan->itemsObject,
                                                                               (TZrInt64)plan->length,
                                                                               value);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
    } else if (!object_add_int_int_pairs_assuming_absent_dense_ready_assume_fast(state,
                                                                                  plan->itemsObject,
                                                                                  plan->length,
                                                                                  appendCount,
                                                                                  value)) {
        return ZR_FALSE;
    }

    return object_super_array_update_cached_length_assume_fast(state,
                                                               plan->receiverObject,
                                                               plan->requiredLength);
}

static ZR_FORCE_INLINE TZrBool object_super_array_append_int_assume_fast(SZrState *state,
                                                                         SZrTypeValue *receiver,
                                                                         TZrInt64 value) {
    ZrSuperArrayAppendPlan plan;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);

    if (!object_super_array_prepare_append_plan_assume_fast(state, receiver, 1, &plan)) {
        return ZR_FALSE;
    }

    return object_super_array_commit_append_plan_assume_fast(state, &plan, value);
}

static ZR_FORCE_INLINE TZrBool object_super_array_prepare_append_plans4_from_stack_assume_fast(
        SZrState *state,
        TZrStackValuePointer receiverBase,
        TZrSize appendCount,
        ZrSuperArrayAppendPlan *plans) {
    SZrTypeValue *receiver0;
    SZrTypeValue *receiver1;
    SZrTypeValue *receiver2;
    SZrTypeValue *receiver3;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiverBase != ZR_NULL);
    ZR_ASSERT(plans != ZR_NULL);

    receiver0 = &receiverBase[0].value;
    receiver1 = &receiverBase[1].value;
    receiver2 = &receiverBase[2].value;
    receiver3 = &receiverBase[3].value;
    if (!object_super_array_prepare_append_plan_assume_fast(state, receiver0, appendCount, &plans[0])) {
        return ZR_FALSE;
    }
    if (!object_super_array_prepare_append_plan_assume_fast(state, receiver1, appendCount, &plans[1])) {
        return ZR_FALSE;
    }
    if (!object_super_array_prepare_append_plan_assume_fast(state, receiver2, appendCount, &plans[2])) {
        return ZR_FALSE;
    }
    if (!object_super_array_prepare_append_plan_assume_fast(state, receiver3, appendCount, &plans[3])) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_super_array_commit_append_plans4_assume_fast(
        SZrState *state,
        const ZrSuperArrayAppendPlan *plans,
        TZrInt64 value) {
    TZrSize appendCount0;
    TZrSize appendCount1;
    TZrSize appendCount2;
    TZrSize appendCount3;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(plans != ZR_NULL);

    appendCount0 = plans[0].requiredLength - plans[0].length;
    appendCount1 = plans[1].requiredLength - plans[1].length;
    appendCount2 = plans[2].requiredLength - plans[2].length;
    appendCount3 = plans[3].requiredLength - plans[3].length;
    if (appendCount0 != appendCount1 || appendCount0 != appendCount2 || appendCount0 != appendCount3) {
        if (!object_super_array_commit_append_plan_assume_fast(state, &plans[0], value)) {
            return ZR_FALSE;
        }
        if (!object_super_array_commit_append_plan_assume_fast(state, &plans[1], value)) {
            return ZR_FALSE;
        }
        if (!object_super_array_commit_append_plan_assume_fast(state, &plans[2], value)) {
            return ZR_FALSE;
        }
        if (!object_super_array_commit_append_plan_assume_fast(state, &plans[3], value)) {
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (appendCount0 == 0) {
        return ZR_TRUE;
    }
    if (appendCount0 == 1) {
        pair = object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(state,
                                                                               plans[0].itemsObject,
                                                                               (TZrInt64)plans[0].length,
                                                                               value);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        pair = object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(state,
                                                                               plans[1].itemsObject,
                                                                               (TZrInt64)plans[1].length,
                                                                               value);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        pair = object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(state,
                                                                               plans[2].itemsObject,
                                                                               (TZrInt64)plans[2].length,
                                                                               value);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        pair = object_add_int_int_pair_assuming_absent_dense_ready_assume_fast(state,
                                                                               plans[3].itemsObject,
                                                                               (TZrInt64)plans[3].length,
                                                                               value);
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        return object_super_array_update_cached_lengths4_assume_fast(state, plans);
    }

    if (!object_add_int_int_pairs4_assuming_absent_dense_ready_assume_fast(state, plans, appendCount0, value)) {
        return ZR_FALSE;
    }

    return object_super_array_update_cached_lengths4_assume_fast(state, plans);
}

TZrBool ZrCore_Object_SuperArrayTryGetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              SZrTypeValue *result,
                                              TZrBool *outApplicable) {
    SZrObject *itemsObject = ZR_NULL;
    TZrInt64 indexValue = 0;
    SZrHashKeyValuePair *pair;

    if (outApplicable != ZR_NULL) {
        *outApplicable = ZR_FALSE;
    }
    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || result == ZR_NULL || outApplicable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_try_resolve_super_array_storage(state, receiver, key, &itemsObject, &indexValue, outApplicable)) {
        return ZR_FALSE;
    }
    if (!*outApplicable) {
        return ZR_TRUE;
    }

    if (indexValue < 0 || (TZrUInt64)indexValue >= (TZrUInt64)itemsObject->nodeMap.elementCount) {
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    pair = object_find_int_key_pair(itemsObject, indexValue);
    if (pair == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else if (object_value_is_plain_primitive(&pair->value)) {
        object_assign_primitive_value_or_copy(state, result, &pair->value);
    } else {
        ZrCore_Value_Copy(state, result, &pair->value);
    }
    return ZR_TRUE;
}

static TZrBool object_try_super_array_add_int_fast(SZrState *state,
                                                   SZrTypeValue *receiver,
                                                   const SZrTypeValue *value,
                                                   SZrTypeValue *result,
                                                   TZrBool *outApplicable) {
    SZrObject *receiverObject = ZR_NULL;
    SZrObject *itemsObject = ZR_NULL;
    SZrHashKeyValuePair *pair;
    TZrSize length;

    if (outApplicable != ZR_NULL) {
        *outApplicable = ZR_FALSE;
    }
    if (state == ZR_NULL || receiver == ZR_NULL || value == ZR_NULL || result == ZR_NULL || outApplicable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_TRUE;
    }
    if (!object_try_resolve_super_array_items(state, receiver, &receiverObject, &itemsObject, outApplicable)) {
        return ZR_FALSE;
    }
    if (!*outApplicable) {
        return ZR_TRUE;
    }

    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(value->type));
    length = itemsObject->nodeMap.elementCount;
    if (!object_try_super_array_ensure_capacity_for_append(state,
                                                           receiverObject,
                                                           itemsObject,
                                                           length + 1,
                                                           1)) {
        return ZR_FALSE;
    }

    ZR_ASSERT(object_find_int_key_pair(itemsObject, (TZrInt64)length) == ZR_NULL);
    pair = object_add_int_int_pair_assuming_absent_dense_ready(state,
                                                               itemsObject,
                                                               (TZrInt64)length,
                                                               value->value.nativeObject.nativeInt64);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE ||
        !object_try_super_array_set_cached_int_field(state,
                                                     receiverObject,
                                                     ZR_SUPER_ARRAY_FIELD_LENGTH,
                                                     (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SuperArrayTrySetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              const SZrTypeValue *value,
                                              TZrBool *outApplicable) {
    SZrObject *itemsObject = ZR_NULL;
    TZrInt64 indexValue = 0;
    SZrHashKeyValuePair *pair;

    if (outApplicable != ZR_NULL) {
        *outApplicable = ZR_FALSE;
    }
    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || value == ZR_NULL || outApplicable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_try_resolve_super_array_storage(state, receiver, key, &itemsObject, &indexValue, outApplicable)) {
        return ZR_FALSE;
    }
    if (!*outApplicable) {
        return ZR_TRUE;
    }

    if (indexValue < 0 || (TZrUInt64)indexValue >= (TZrUInt64)itemsObject->nodeMap.elementCount) {
        ZrCore_Debug_RunError(state, "Array index out of range");
    }

    pair = object_find_or_add_int_key_pair(state, itemsObject, indexValue);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object_value_is_plain_primitive(value)) {
        object_assign_primitive_value_or_copy(state, &pair->value, value);
    } else {
        ZrCore_Value_Copy(state, &pair->value, value);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SuperArrayGetIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *key,
                                                 SZrTypeValue *result) {
    return ZrCore_Object_SuperArrayGetIntInlineAssumeFast(state, receiver, key, result);
}

TZrBool ZrCore_Object_SuperArraySetIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *key,
                                                 const SZrTypeValue *value) {
    return ZrCore_Object_SuperArraySetIntInlineAssumeFast(state, receiver, key, value);
}

TZrBool ZrCore_Object_SuperArrayAddIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *value,
                                                 SZrTypeValue *result) {
    TZrInt64 intValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(value->type));
    intValue = value->value.nativeObject.nativeInt64;
    if (!object_super_array_append_int_assume_fast(state, receiver, intValue)) {
        return ZR_FALSE;
    }

    if (object_value_is_plain_primitive(result)) {
        object_store_plain_null_reuse(result);
    } else {
        ZrCore_Value_ResetAsNull(result);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(SZrState *state,
                                                       TZrStackValuePointer receiverBase,
                                                       TZrInt64 intValue) {
    ZrSuperArrayAppendPlan plans[4];

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiverBase != ZR_NULL);

    if (!object_super_array_prepare_append_plans4_from_stack_assume_fast(state, receiverBase, 1, plans)) {
        return ZR_FALSE;
    }

    return object_super_array_commit_append_plans4_assume_fast(state, plans, intValue);
}

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(SZrState *state,
                                                                    TZrStackValuePointer receiverBase,
                                                                    TZrInt64 repeatCount,
                                                                    TZrInt64 value) {
    ZrSuperArrayAppendPlan plans[4];
    TZrSize appendCount;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiverBase != ZR_NULL);

    if (repeatCount <= 0) {
        return ZR_TRUE;
    }

    if ((TZrUInt64)repeatCount > (TZrUInt64)((TZrSize)-1)) {
        return ZR_FALSE;
    }
    appendCount = (TZrSize)repeatCount;

    if (!object_super_array_prepare_append_plans4_from_stack_assume_fast(state, receiverBase, appendCount, plans)) {
        return ZR_FALSE;
    }

    return object_super_array_commit_append_plans4_assume_fast(state, plans, value);
}

TZrBool ZrCore_Object_SuperArrayGetInt(struct SZrState *state,
                                       SZrTypeValue *receiver,
                                       const SZrTypeValue *key,
                                       SZrTypeValue *result) {
    TZrBool applicable = ZR_FALSE;

    if (!ZrCore_Object_SuperArrayTryGetIntFast(state, receiver, key, result, &applicable)) {
        return ZR_FALSE;
    }

    return applicable ? ZR_TRUE : ZrCore_Object_GetByIndex(state, receiver, key, result);
}

TZrBool ZrCore_Object_SuperArraySetInt(struct SZrState *state,
                                       SZrTypeValue *receiver,
                                       const SZrTypeValue *key,
                                       const SZrTypeValue *value) {
    TZrBool applicable = ZR_FALSE;

    if (!ZrCore_Object_SuperArrayTrySetIntFast(state, receiver, key, value, &applicable)) {
        return ZR_FALSE;
    }

    return applicable ? ZR_TRUE : ZrCore_Object_SetByIndex(state, receiver, key, value);
}

TZrBool ZrCore_Object_SuperArrayAddInt(struct SZrState *state,
                                       SZrTypeValue *receiver,
                                       const SZrTypeValue *value,
                                       SZrTypeValue *result) {
    TZrBool applicable = ZR_FALSE;
    SZrString *memberName;

    if (!object_try_super_array_add_int_fast(state, receiver, value, result, &applicable)) {
        return ZR_FALSE;
    }
    if (applicable) {
        return ZR_TRUE;
    }

    memberName = ZrCore_Object_CachedKnownFieldString(state, ZR_OBJECT_ARRAY_ADD_MEMBER);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_Object_InvokeMember(state, receiver, memberName, value, 1, result);
}
