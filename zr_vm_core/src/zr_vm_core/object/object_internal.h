#ifndef ZR_VM_CORE_OBJECT_INTERNAL_H
#define ZR_VM_CORE_OBJECT_INTERNAL_H

#include "zr_vm_core/object.h"
#include "zr_vm_core/gc.h"

static ZR_FORCE_INLINE TZrBool object_node_map_is_ready(const SZrObject *object) {
    return object != ZR_NULL && object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL &&
           object->nodeMap.capacity > 0;
}

static ZR_FORCE_INLINE void object_reset_hot_field_pair_cache(SZrObject *object) {
    if (object == ZR_NULL) {
        return;
    }

    object->cachedHiddenItemsPair = ZR_NULL;
    object->cachedHiddenItemsObject = ZR_NULL;
    object->cachedLengthPair = ZR_NULL;
    object->cachedCapacityPair = ZR_NULL;
    object->cachedStringLookupPair = ZR_NULL;
    object->cachedStringLookupPair2 = ZR_NULL;
    object->cachedIteratorSourcePair = ZR_NULL;
    object->cachedIteratorCurrentPair = ZR_NULL;
    object->cachedIteratorIndexPair = ZR_NULL;
    object->cachedIteratorNextNodePair = ZR_NULL;
}

static ZR_FORCE_INLINE void object_cache_string_lookup_pair_mru(SZrObject *object, SZrHashKeyValuePair *pair) {
    if (object == ZR_NULL || pair == ZR_NULL) {
        return;
    }

    if (object->cachedStringLookupPair == pair) {
        return;
    }
    if (object->cachedStringLookupPair2 == pair) {
        object->cachedStringLookupPair2 = object->cachedStringLookupPair;
        object->cachedStringLookupPair = pair;
        return;
    }
    object->cachedStringLookupPair2 = object->cachedStringLookupPair;
    object->cachedStringLookupPair = pair;
}

static ZR_FORCE_INLINE void object_make_string_key_cached_unchecked(SZrState *state,
                                                                    struct SZrString *name,
                                                                    SZrTypeValue *outKey) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(name != ZR_NULL);
    ZR_ASSERT(outKey != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    outKey->type = ZR_VALUE_TYPE_STRING;
}

static ZR_FORCE_INLINE const SZrTypeValue *object_try_get_direct_storage_key_unchecked(const SZrTypeValue *key) {
    const SZrString *keyString;

    if (key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING || key->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    keyString = ZR_CAST(const SZrString *, key->value.object);
    if (!ZrCore_String_IsShort(keyString) ||
        ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER((SZrString *)keyString))) {
        return ZR_NULL;
    }

    return key;
}

static ZR_FORCE_INLINE const SZrTypeValue *object_try_get_cached_string_value_by_name_pointer_unchecked(
        SZrObject *object,
        struct SZrString *memberName) {
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);

    pair = object->cachedStringLookupPair;
    if (pair != ZR_NULL &&
        pair->key.type == ZR_VALUE_TYPE_STRING &&
        pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)) {
        return &pair->value;
    }

    pair = object->cachedStringLookupPair2;
    if (pair != ZR_NULL &&
        pair->key.type == ZR_VALUE_TYPE_STRING &&
        pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)) {
        object_cache_string_lookup_pair_mru(object, pair);
        return &pair->value;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_try_match_cached_string_pair_by_name_unchecked(
        SZrState *state,
        SZrHashKeyValuePair *pair,
        struct SZrString *memberName) {
    SZrString *cachedKeyString;

    if (state == ZR_NULL || pair == ZR_NULL || memberName == ZR_NULL ||
        pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(memberName)) {
        return pair;
    }

    cachedKeyString = ZR_CAST(SZrString *, pair->key.value.object);
    if (memberName->shortStringLength < ZR_VM_LONG_STRING_FLAG ||
        cachedKeyString->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZR_NULL;
    }

    return ZrCore_String_Equal(memberName, cachedKeyString) ? pair : ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_try_get_cached_string_pair_by_name_unchecked(
        SZrState *state,
        SZrObject *object,
        struct SZrString *memberName) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object->cachedStringLookupPair;
    pair = object_try_match_cached_string_pair_by_name_unchecked(state, pair, memberName);
    if (pair != ZR_NULL) {
        return pair;
    }

    pair = object->cachedStringLookupPair2;
    pair = object_try_match_cached_string_pair_by_name_unchecked(state, pair, memberName);
    if (pair != ZR_NULL) {
        object_cache_string_lookup_pair_mru(object, pair);
        return pair;
    }

    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_try_match_cached_string_pair_unchecked(
        SZrState *state,
        SZrHashKeyValuePair *pair,
        const SZrTypeValue *key) {
    SZrString *keyString;
    SZrString *cachedKeyString;
    if (state == ZR_NULL || pair == ZR_NULL || key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (pair == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (pair->key.value.object == key->value.object) {
        return pair;
    }

    keyString = ZR_CAST(SZrString *, key->value.object);
    cachedKeyString = ZR_CAST(SZrString *, pair->key.value.object);
    if (keyString->shortStringLength < ZR_VM_LONG_STRING_FLAG ||
        cachedKeyString->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZR_NULL;
    }

    return ZrCore_String_Equal(keyString, cachedKeyString) ? pair : ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_try_get_cached_string_pair_unchecked(
        SZrState *state,
        SZrObject *object,
        const SZrTypeValue *key) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    pair = object_try_match_cached_string_pair_unchecked(state, object->cachedStringLookupPair, key);
    if (pair != ZR_NULL) {
        return pair;
    }

    pair = object_try_match_cached_string_pair_unchecked(state, object->cachedStringLookupPair2, key);
    if (pair != ZR_NULL) {
        object_cache_string_lookup_pair_mru(object, pair);
        return pair;
    }

    return pair;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *object_get_own_string_pair_by_name_cached_unchecked(
        SZrState *state,
        SZrObject *object,
        struct SZrString *memberName) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue memberKey;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);

    if (!object_node_map_is_ready(object)) {
        return ZR_NULL;
    }

    pair = object_try_get_cached_string_pair_by_name_unchecked(state, object, memberName);
    if (pair != ZR_NULL) {
        return pair;
    }

    object_make_string_key_cached_unchecked(state, memberName, &memberKey);
    pair = ZrCore_HashSet_Find(state, &object->nodeMap, &memberKey);
    if (pair != ZR_NULL && pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
        object_cache_string_lookup_pair_mru(object, pair);
    }
    return pair;
}

static ZR_FORCE_INLINE const SZrTypeValue *object_get_own_string_value_by_name_cached_unchecked(
        SZrState *state,
        SZrObject *object,
        struct SZrString *memberName) {
    SZrHashKeyValuePair *pair;

    pair = object_get_own_string_pair_by_name_cached_unchecked(state, object, memberName);
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

static ZR_FORCE_INLINE const SZrTypeValue *object_get_prototype_string_value_by_name_cached_unchecked(
        SZrState *state,
        SZrObjectPrototype *prototype,
        struct SZrString *memberName,
        TZrBool includeInherited) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(memberName != ZR_NULL);

    while (prototype != ZR_NULL) {
        const SZrTypeValue *value =
                object_get_own_string_value_by_name_cached_unchecked(state, &prototype->super, memberName);

        if (value != ZR_NULL) {
            return value;
        }
        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(
        SZrState *state,
        SZrObject *object,
        SZrHashKeyValuePair *pair,
        const SZrTypeValue *value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL);

    if (!ZrCore_Value_HasNormalizedNoOwnership(&pair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(value)) {
        return ZR_FALSE;
    }

    if (&pair->value != value) {
        pair->value = *value;
    }
    if (value->isGarbageCollectable) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    }
    object_cache_string_lookup_pair_mru(object, pair);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_try_set_existing_pair_plain_value_fast_unchecked(
        SZrState *state,
        SZrObject *object,
        SZrHashKeyValuePair *pair,
        const SZrTypeValue *value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);

    if (object->cachedHiddenItemsPair != ZR_NULL ||
        object->cachedHiddenItemsObject != ZR_NULL ||
        pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(state, object, pair, value);
}

ZR_CORE_API TZrBool ZrCore_Object_GetMemberWithKeyUnchecked(SZrState *state,
                                                            SZrTypeValue *receiver,
                                                            struct SZrString *memberName,
                                                            const SZrTypeValue *memberKey,
                                                            SZrTypeValue *result);

struct SZrFunction *ZrCore_Object_GetMemberCachedCallableTargetUnchecked(SZrState *state,
                                                                         struct SZrObjectPrototype *ownerPrototype,
                                                                         TZrUInt32 descriptorIndex);

TZrBool ZrCore_Object_GetMemberCachedCallableUnchecked(SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       struct SZrObjectPrototype *ownerPrototype,
                                                       TZrUInt32 descriptorIndex,
                                                       struct SZrFunction *cachedCallable,
                                                       SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_TryGetMemberWithKeyFastUnchecked(SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   struct SZrString *memberName,
                                                                   const SZrTypeValue *memberKey,
                                                                   SZrTypeValue *result,
                                                                   TZrBool *outHandled);

TZrBool ZrCore_Object_SetMemberWithKeyUnchecked(SZrState *state,
                                                SZrTypeValue *receiver,
                                                struct SZrString *memberName,
                                                const SZrTypeValue *memberKey,
                                                const SZrTypeValue *value);

TZrBool ZrCore_Object_SetMemberWithKeyUncheckedStackOperands(SZrState *state,
                                                             SZrTypeValue *receiver,
                                                             struct SZrString *memberName,
                                                             const SZrTypeValue *memberKey,
                                                             const SZrTypeValue *value);

TZrBool ZrCore_Object_SetMemberCachedDescriptorUncheckedStackOperands(SZrState *state,
                                                                      SZrTypeValue *receiver,
                                                                      struct SZrObjectPrototype *ownerPrototype,
                                                                      TZrUInt32 descriptorIndex,
                                                                      const SZrTypeValue *value);

void ZrCore_Object_SetExistingPairValueUnchecked(SZrState *state,
                                                 SZrObject *object,
                                                 SZrHashKeyValuePair *pair,
                                                 const SZrTypeValue *value);

ZR_CORE_API void ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(SZrState *state,
                                                                          SZrObject *object,
                                                                          SZrHashKeyValuePair *pair,
                                                                          const SZrTypeValue *value);

TZrBool ZrCore_Object_TrySetMemberWithKeyFastUnchecked(SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       struct SZrString *memberName,
                                                       const SZrTypeValue *memberKey,
                                                       const SZrTypeValue *value,
                                                       TZrBool *outHandled);

TZrBool ZrCore_Object_TrySetMemberWithKeyFastUncheckedStackOperands(SZrState *state,
                                                                    SZrTypeValue *receiver,
                                                                    struct SZrString *memberName,
                                                                    const SZrTypeValue *memberKey,
                                                                    const SZrTypeValue *value,
                                                                    TZrBool *outHandled);

ZR_CORE_API TZrBool ZrCore_Object_GetByIndexUnchecked(SZrState *state,
                                                      SZrTypeValue *receiver,
                                                      const SZrTypeValue *key,
                                                      SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_GetByIndexUncheckedStackOperands(SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   SZrTypeValue *result);

TZrBool ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands(SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_SetByIndexUnchecked(SZrState *state,
                                                      SZrTypeValue *receiver,
                                                      const SZrTypeValue *key,
                                                      const SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Object_SetByIndexUncheckedStackOperands(SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   const SZrTypeValue *value);

TZrBool ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperands(SZrState *state,
                                                                   SZrTypeValue *receiver,
                                                                   const SZrTypeValue *key,
                                                                   const SZrTypeValue *value);

#endif
