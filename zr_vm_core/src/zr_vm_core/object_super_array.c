//
// Extracted super-array fast paths and hot object-field caches.
//

#include "object_super_array_internal.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include <string.h>

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
    return !value->isGarbageCollectable &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           value->ownershipControl == ZR_NULL &&
           value->ownershipWeakRef == ZR_NULL;
}

static ZR_FORCE_INLINE void object_assign_primitive_value_or_copy(SZrState *state,
                                                                  SZrTypeValue *destination,
                                                                  const SZrTypeValue *source) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    ZR_ASSERT(object_value_is_plain_primitive(source));

    if (object_value_is_plain_primitive(destination)) {
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

    if (object_value_is_plain_primitive(destination)) {
        ZR_VALUE_FAST_SET(destination, nativeInt64, value, ZR_VALUE_TYPE_INT64);
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

static SZrHashKeyValuePair *object_add_empty_pair_for_hash(SZrState *state, SZrObject *object, TZrUInt64 hash) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || !object_ensure_node_map_ready(state, object)) {
        return ZR_NULL;
    }

    nodeMap = &object->nodeMap;
    if (nodeMap->elementCount + 1 > nodeMap->resizeThreshold &&
        !ZrCore_HashSet_Rehash(state, nodeMap, nodeMap->capacity * ZR_HASH_SET_CAPACITY_GROWTH_FACTOR)) {
        return ZR_NULL;
    }

    pair = (SZrHashKeyValuePair *)ZrCore_Memory_GcMalloc(state,
                                                         ZR_MEMORY_NATIVE_TYPE_HASH_PAIR,
                                                         sizeof(SZrHashKeyValuePair));
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_ResetAsNull(&pair->key);
    ZrCore_Value_ResetAsNull(&pair->value);
    pair->next = ZrCore_HashSet_GetBucket(nodeMap, hash);
    nodeMap->buckets[ZR_HASH_MOD(hash, nodeMap->capacity)] = pair;
    nodeMap->elementCount++;
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

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_add_int_key_pair_assuming_absent(SZrState *state,
                                                                                     SZrObject *object,
                                                                                     TZrInt64 indexValue) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object_add_empty_pair_for_hash(state, object, (TZrUInt64)indexValue);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }

    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, indexValue, ZR_VALUE_TYPE_INT64);
    return pair;
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

static ZR_FORCE_INLINE TZrBool object_set_super_array_int_field(SZrState *state,
                                                                SZrObject *object,
                                                                EZrSuperArrayFieldKind fieldKind,
                                                                TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = object_resolve_super_array_field_pair(state, object, fieldKind, ZR_TRUE);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    object_assign_int_value_or_copy(state, &pair->value, value);
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

static TZrBool object_try_super_array_ensure_capacity(SZrState *state,
                                                      SZrObject *receiverObject,
                                                      TZrSize requiredLength) {
    TZrInt64 capacity;

    if (state == ZR_NULL || receiverObject == ZR_NULL) {
        return ZR_FALSE;
    }

    capacity = object_get_super_array_int_field_or_default(state,
                                                           receiverObject,
                                                           ZR_SUPER_ARRAY_FIELD_CAPACITY,
                                                           0);
    if ((TZrSize)capacity >= requiredLength) {
        return ZR_TRUE;
    }

    if (capacity <= 0) {
        capacity = ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY;
    }
    while ((TZrSize)capacity < requiredLength) {
        capacity *= ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR;
    }

    return object_set_super_array_int_field(state,
                                            receiverObject,
                                            ZR_SUPER_ARRAY_FIELD_CAPACITY,
                                            capacity);
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
    if (!object_try_super_array_ensure_capacity(state, receiverObject, length + 1)) {
        return ZR_FALSE;
    }

    ZR_ASSERT(object_find_int_key_pair(itemsObject, (TZrInt64)length) == ZR_NULL);
    pair = object_add_int_key_pair_assuming_absent(state, itemsObject, (TZrInt64)length);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    object_assign_int_value_or_copy(state, &pair->value, value->value.nativeObject.nativeInt64);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE ||
        !object_set_super_array_int_field(state,
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
