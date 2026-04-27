//
// Built-in zr.container module and runtime callbacks.
//

#include "zr_vm_lib_container/module.h"

#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *kContainerItemsField = "__zr_items";
static const TZrChar *kContainerEntriesField = "__zr_entries";
static const TZrChar *kContainerSourceField = "__zr_source";
static const TZrChar *kContainerIndexField = "__zr_index";
static const TZrChar *kContainerNextNodeField = "__zr_nextNode";
static const TZrChar *kContainerCountField = "count";
static const TZrChar *kContainerLengthField = "length";
static const TZrChar *kContainerCapacityField = "capacity";
static const TZrChar *kContainerCurrentField = "current";
static const TZrChar *kContainerValueField = "value";
static const TZrChar *kContainerNextField = "next";
static const TZrChar *kContainerPreviousField = "previous";
static const TZrChar *kContainerFirstField = "first";
static const TZrChar *kContainerLastField = "last";
static const TZrChar *kContainerPairFirstField = "first";
static const TZrChar *kContainerPairSecondField = "second";

enum {
    ZR_CONTAINER_FIELD_CACHE_CAPACITY = 48,
    ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT = 4
};

typedef struct ZrContainerFieldStringCacheEntry {
    const TZrChar *fieldName;
    SZrString *fieldString;
} ZrContainerFieldStringCacheEntry;

typedef struct ZrContainerHotFieldStringCache {
    TZrUInt64 cacheIdentity;
    SZrString *itemsFieldString;
    SZrString *entriesFieldString;
    SZrString *pairFirstFieldString;
    SZrString *pairSecondFieldString;
    SZrString *iteratorSourceFieldString;
    SZrString *iteratorCurrentFieldString;
    SZrString *iteratorIndexFieldString;
    SZrString *iteratorNextNodeFieldString;
} ZrContainerHotFieldStringCache;

typedef struct ZrContainerHotMapLookupCacheSlot {
    SZrRawObject *keyObject;
    TZrUInt32 entriesMemberVersion;
    TZrSize index;
    SZrObject *entryObject;
} ZrContainerHotMapLookupCacheSlot;

typedef struct ZrContainerHotMapLookupCache {
    TZrUInt64 cacheIdentity;
    SZrObject *entries;
    SZrRawObject *lastKeyObject;
    TZrUInt32 lastEntriesMemberVersion;
    TZrSize lastIndex;
    SZrObject *lastEntryObject;
    TZrUInt8 nextSlotIndex;
    ZrContainerHotMapLookupCacheSlot slots[ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT];
} ZrContainerHotMapLookupCache;

#if defined(ZR_DEBUG)
static ZrVmLibContainerDebugHotMapLookupStats gZrContainerDebugHotMapLookupStats = {0};
#endif

static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state);
static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state);
static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index);
static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName);
static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value);
static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot);
static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                       SZrHashKeyValuePair *pair,
                                                                       SZrString *fieldString);
static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object,
                                                                       SZrHashKeyValuePair *pair);

static ZR_FORCE_INLINE TZrBool zr_container_field_name_equals(const TZrChar *fieldName, const TZrChar *expectedFieldName) {
    return fieldName == expectedFieldName ||
           (fieldName != ZR_NULL && expectedFieldName != ZR_NULL && strcmp(fieldName, expectedFieldName) == 0);
}

void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    memset(&gZrContainerDebugHotMapLookupStats, 0, sizeof(gZrContainerDebugHotMapLookupStats));
#endif
}

ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    return gZrContainerDebugHotMapLookupStats;
#else
    ZrVmLibContainerDebugHotMapLookupStats stats = {0};
    return stats;
#endif
}

static SZrString *zr_container_cache_field_string_once(SZrState *state,
                                                       const TZrChar *fieldName,
                                                       SZrString **slot) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || slot == ZR_NULL) {
        return ZR_NULL;
    }

    if (*slot != ZR_NULL) {
        return *slot;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    *slot = fieldString;
    return fieldString;
}

static SZrString *zr_container_cached_field_string(SZrState *state, const TZrChar *fieldName) {
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    static ZrContainerFieldStringCacheEntry cache[ZR_CONTAINER_FIELD_CACHE_CAPACITY];
    static SZrString *cachedItemsFieldString = ZR_NULL;
    static SZrString *cachedEntriesFieldString = ZR_NULL;
    static SZrString *cachedCountFieldString = ZR_NULL;
    static SZrString *cachedLengthFieldString = ZR_NULL;
    static SZrString *cachedCapacityFieldString = ZR_NULL;
    static SZrString *cachedValueFieldString = ZR_NULL;
    static SZrString *cachedNextFieldString = ZR_NULL;
    static SZrString *cachedPreviousFieldString = ZR_NULL;
    static SZrString *cachedLastFieldString = ZR_NULL;
    static SZrString *cachedPairFirstFieldString = ZR_NULL;
    static SZrString *cachedPairSecondFieldString = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        memset(cache, 0, sizeof(cache));
        cachedItemsFieldString = ZR_NULL;
        cachedEntriesFieldString = ZR_NULL;
        cachedCountFieldString = ZR_NULL;
        cachedLengthFieldString = ZR_NULL;
        cachedCapacityFieldString = ZR_NULL;
        cachedValueFieldString = ZR_NULL;
        cachedNextFieldString = ZR_NULL;
        cachedPreviousFieldString = ZR_NULL;
        cachedLastFieldString = ZR_NULL;
        cachedPairFirstFieldString = ZR_NULL;
        cachedPairSecondFieldString = ZR_NULL;
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField)) {
        return zr_container_cache_field_string_once(state, kContainerItemsField, &cachedItemsFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return zr_container_cache_field_string_once(state, kContainerEntriesField, &cachedEntriesFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCountField)) {
        return zr_container_cache_field_string_once(state, kContainerCountField, &cachedCountFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField)) {
        return zr_container_cache_field_string_once(state, kContainerLengthField, &cachedLengthFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField)) {
        return zr_container_cache_field_string_once(state, kContainerCapacityField, &cachedCapacityFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerValueField)) {
        return zr_container_cache_field_string_once(state, kContainerValueField, &cachedValueFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextField)) {
        return zr_container_cache_field_string_once(state, kContainerNextField, &cachedNextFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPreviousField)) {
        return zr_container_cache_field_string_once(state, kContainerPreviousField, &cachedPreviousFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLastField)) {
        return zr_container_cache_field_string_once(state, kContainerLastField, &cachedLastFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return zr_container_cache_field_string_once(state, kContainerPairSecondField, &cachedPairSecondFieldString);
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        if (cache[index].fieldName != ZR_NULL && strcmp(cache[index].fieldName, fieldName) == 0) {
            return cache[index].fieldString;
        }
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        SZrString *fieldString;

        if (cache[index].fieldName != ZR_NULL) {
            continue;
        }

        fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
        if (fieldString == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));

        cache[index].fieldName = fieldName;
        cache[index].fieldString = fieldString;
        return fieldString;
    }

    ZR_ASSERT(ZR_FALSE);
    return ZR_NULL;
}

static TZrBool zr_container_make_field_key(SZrState *state, const TZrChar *fieldName, SZrTypeValue *outKey) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE ZrContainerHotFieldStringCache *zr_container_hot_field_string_cache(SZrState *state) {
    static ZrContainerHotFieldStringCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

static ZR_FORCE_INLINE ZrContainerHotMapLookupCache *zr_container_hot_map_lookup_cache(SZrState *state) {
    static ZrContainerHotMapLookupCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

static ZR_FORCE_INLINE void zr_container_reset_hot_map_lookup_cache_state(ZrContainerHotMapLookupCache *cache,
                                                                          SZrObject *entries) {
    if (cache == ZR_NULL) {
        return;
    }

    cache->entries = entries;
    cache->lastKeyObject = ZR_NULL;
    cache->lastEntriesMemberVersion = 0u;
    cache->lastIndex = 0u;
    cache->lastEntryObject = ZR_NULL;
    cache->nextSlotIndex = 0u;
    memset(cache->slots, 0, sizeof(cache->slots));
}

static ZR_FORCE_INLINE void zr_container_commit_hot_map_lookup_cache_hit(ZrContainerHotMapLookupCache *cache,
                                                                         const ZrContainerHotMapLookupCacheSlot *slot,
                                                                         TZrSize *outIndex,
                                                                         SZrObject **outEntryObject) {
    ZR_ASSERT(cache != ZR_NULL);
    ZR_ASSERT(slot != ZR_NULL);

    cache->lastKeyObject = slot->keyObject;
    cache->lastEntriesMemberVersion = slot->entriesMemberVersion;
    cache->lastIndex = slot->index;
    cache->lastEntryObject = slot->entryObject;
#if defined(ZR_DEBUG)
    gZrContainerDebugHotMapLookupStats.hotHitCount++;
    gZrContainerDebugHotMapLookupStats.memberVersionHitCount++;
#endif
    if (outIndex != ZR_NULL) {
        *outIndex = slot->index;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = slot->entryObject;
    }
}

static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache_slot(
        ZrContainerHotMapLookupCache *cache,
        const ZrContainerHotMapLookupCacheSlot *slot,
        SZrRawObject *keyObject,
        TZrUInt32 entriesMemberVersion,
        TZrSize *outIndex,
        SZrObject **outEntryObject) {
    if (cache == ZR_NULL || slot == ZR_NULL || slot->keyObject != keyObject ||
        slot->entriesMemberVersion != entriesMemberVersion || slot->entryObject == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_commit_hot_map_lookup_cache_hit(cache, slot, outIndex, outEntryObject);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void zr_container_update_hot_map_lookup_cache(SZrState *state,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize index,
                                                                     SZrObject *entryObject) {
    ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);
    ZrContainerHotMapLookupCacheSlot slotValue = {0};
    TZrSize insertIndex;
    TZrUInt32 entriesMemberVersion;

    if (cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL || entryObject == ZR_NULL) {
        return;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
    }

    cache->lastKeyObject = keyObject;
    cache->lastEntriesMemberVersion = entriesMemberVersion;
    cache->lastIndex = index;
    cache->lastEntryObject = entryObject;

    slotValue.keyObject = keyObject;
    slotValue.entriesMemberVersion = entriesMemberVersion;
    slotValue.index = index;
    slotValue.entryObject = entryObject;

    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == keyObject &&
            cache->slots[slotIndex].entriesMemberVersion == entriesMemberVersion) {
            cache->slots[slotIndex] = slotValue;
            return;
        }
    }

    insertIndex = ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == ZR_NULL ||
            cache->slots[slotIndex].entryObject == ZR_NULL) {
            insertIndex = slotIndex;
            break;
        }
    }

    if (insertIndex == ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT) {
        insertIndex = cache->nextSlotIndex % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    }
    cache->slots[insertIndex] = slotValue;
    cache->nextSlotIndex = (TZrUInt8)((insertIndex + 1u) % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT);
}

static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache(SZrState *state,
                                                                     ZrContainerHotMapLookupCache *cache,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize *outIndex,
                                                                     SZrObject **outEntryObject) {
    TZrUInt32 entriesMemberVersion;

    if (state == ZR_NULL || cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL) {
        return ZR_FALSE;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
        return ZR_FALSE;
    }

    if (cache->lastKeyObject == keyObject &&
        cache->lastEntryObject != ZR_NULL &&
        cache->lastEntriesMemberVersion == entriesMemberVersion) {
        ZrContainerHotMapLookupCacheSlot lastSlot = {
                .keyObject = cache->lastKeyObject,
                .entriesMemberVersion = cache->lastEntriesMemberVersion,
                .index = cache->lastIndex,
                .entryObject = cache->lastEntryObject};

        zr_container_commit_hot_map_lookup_cache_hit(cache, &lastSlot, outIndex, outEntryObject);
        return ZR_TRUE;
    }

    if (zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[0], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[1], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[2], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[3], keyObject, entriesMemberVersion, outIndex, outEntryObject)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE SZrString *zr_container_entries_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerEntriesField, &cache->entriesFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_items_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerItemsField, &cache->itemsFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_pair_first_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairFirstField, &cache->pairFirstFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_pair_second_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairSecondField, &cache->pairSecondFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_iterator_source_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerSourceField, &cache->iteratorSourceFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_iterator_current_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerCurrentField, &cache->iteratorCurrentFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_iterator_index_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerIndexField, &cache->iteratorIndexFieldString)
                            : ZR_NULL;
}

static ZR_FORCE_INLINE SZrString *zr_container_iterator_next_node_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state,
                                                                  kContainerNextNodeField,
                                                                  &cache->iteratorNextNodeFieldString)
                            : ZR_NULL;
}

static SZrObjectPrototype *zr_container_iterator_runtime_prototype(SZrState *state,
                                                                   FZrNativeFunction moveNextFunction) {
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    static SZrObjectPrototype *arrayIteratorPrototype = ZR_NULL;
    static SZrObjectPrototype *linkedIteratorPrototype = ZR_NULL;
    static SZrString *currentMemberName = ZR_NULL;
    SZrObjectPrototype **slot;

    if (state == ZR_NULL || state->global == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        arrayIteratorPrototype = ZR_NULL;
        linkedIteratorPrototype = ZR_NULL;
        currentMemberName = ZR_NULL;
    }

    if (moveNextFunction == zr_container_array_iterator_move_next_native) {
        slot = &arrayIteratorPrototype;
    } else if (moveNextFunction == zr_container_linked_list_iterator_move_next_native) {
        slot = &linkedIteratorPrototype;
    } else {
        return ZR_NULL;
    }

    if (*slot == ZR_NULL) {
        SZrString *prototypeName;
        SZrClosureNative *closure;
        SZrIteratorContract contract;

        prototypeName = ZrCore_String_CreateFromNative(state,
                                                       moveNextFunction == zr_container_array_iterator_move_next_native
                                                               ? "__zr_container_array_iterator"
                                                               : "__zr_container_linked_iterator");
        if (prototypeName == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototypeName));

        if (currentMemberName == ZR_NULL) {
            currentMemberName = ZrCore_String_CreateFromNative(state, "current");
            if (currentMemberName == ZR_NULL) {
                return ZR_NULL;
            }
            ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentMemberName));
        }

        *slot = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        if (*slot == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(*slot));

        closure = ZrCore_ClosureNative_New(state, 0);
        if (closure == ZR_NULL) {
            return ZR_NULL;
        }
        closure->nativeFunction = moveNextFunction;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

        memset(&contract, 0, sizeof(contract));
        contract.moveNextFunction = ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        contract.currentMemberName = currentMemberName;
        ZrCore_ObjectPrototype_SetIteratorContract(*slot, &contract);
        ZrCore_ObjectPrototype_AddProtocol(*slot, ZR_PROTOCOL_ID_ITERATOR);
    }

    return *slot;
}

static SZrObject *zr_container_self_object(ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static TZrBool zr_container_object_is_owner_instance(const ZrLibCallContext *context, SZrObject *object) {
    SZrObjectPrototype *ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    return ownerPrototype != ZR_NULL && object != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(object, ownerPrototype);
}

static SZrObject *zr_container_resolve_construct_target(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    self = zr_container_self_object(context);
    if (self != ZR_NULL &&
        ((targetPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, targetPrototype)) ||
         zr_container_object_is_owner_instance(context, self))) {
        return self;
    }

    if (targetPrototype == ZR_NULL) {
        targetPrototype = ZrLib_CallContext_OwnerPrototype(context);
    }
    return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
}

static EZrValueType zr_container_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT;
}

static ZR_FORCE_INLINE void zr_container_value_set_object_fast(SZrState *state,
                                                               SZrTypeValue *value,
                                                               SZrObject *object,
                                                               EZrValueType type) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

static TZrBool zr_container_finish_object(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object) {
    if (context == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(context->state, result, object, zr_container_value_type_for_object(object));
    return ZR_TRUE;
}

static TZrBool zr_container_set_value_field_fast(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    TZrBool isHiddenItemsField;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_FALSE;
    }

    fieldString = ZR_CAST_STRING(state, key.value.object);
    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (hotPairSlot != ZR_NULL && fieldString != ZR_NULL) {
        zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    }

    isHiddenItemsField = zr_container_field_name_equals(fieldName, kContainerItemsField) ||
                         zr_container_field_name_equals(fieldName, kContainerEntriesField);
    if (isHiddenItemsField) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_container_set_int_field_fast(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrInt64 value) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
        ZR_VALUE_FAST_SET(&(*hotPairSlot)->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

static TZrBool zr_container_set_null_field_fast(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
        ZrCore_Value_ResetAsNullNoProfile(&(*hotPairSlot)->value);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

static TZrBool zr_container_set_object_field_fast(SZrState *state,
                                                  SZrObject *object,
                                                  const TZrChar *fieldName,
                                                  SZrObject *valueObject) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (valueObject == ZR_NULL) {
        return zr_container_set_null_field_fast(state, object, fieldName);
    }

    zr_container_value_set_object_fast(state, &fieldValue, valueObject, zr_container_value_type_for_object(valueObject));
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

static const SZrTypeValue *zr_container_get_field_value(SZrState *state,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_NULL;
    }
    return ZrCore_Object_GetValue(state, object, &key);
}

static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName) {
    if (object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField) ||
        zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return &object->cachedHiddenItemsPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField) ||
        zr_container_field_name_equals(fieldName, kContainerCountField) ||
        zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return &object->cachedLengthPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField) ||
        zr_container_field_name_equals(fieldName, kContainerLastField) ||
        zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return &object->cachedCapacityPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerSourceField)) {
        return &object->cachedIteratorSourcePair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCurrentField)) {
        return &object->cachedIteratorCurrentPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerIndexField)) {
        return &object->cachedIteratorIndexPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextNodeField)) {
        return &object->cachedIteratorNextNodePair;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_string_pair_exact(SZrObject *object,
                                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL ||
        object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    hash = ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)->hash;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        if (pair->key.type == ZR_VALUE_TYPE_STRING &&
            pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
            return pair;
        }
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                      SZrHashKeyValuePair *pair,
                                                                      SZrString *fieldString) {
    SZrString *pairKeyString;

    if (state == ZR_NULL || pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_TRUE;
    }

    pairKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    return pairKeyString != ZR_NULL && ZrCore_String_Equal(pairKeyString, fieldString);
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_try_match_cached_field_pair_exact(SZrHashKeyValuePair *pair,
                                                                                            SZrString *fieldString) {
    if (pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_NULL;
    }

    return pair;
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_try_get_hot_field_value_exact(SZrState *state,
                                                                                      SZrObject *object,
                                                                                      const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    pair = hotPairSlot != ZR_NULL ? zr_container_try_match_cached_field_pair_exact(*hotPairSlot, fieldString) : ZR_NULL;
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object, SZrHashKeyValuePair *pair) {
    if (object == ZR_NULL) {
        return;
    }

    if (pair == ZR_NULL) {
        object->cachedStringLookupPair = ZR_NULL;
        object->cachedStringLookupPair2 = ZR_NULL;
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

static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || pair == ZR_NULL || value == ZR_NULL ||
        !ZrCore_Value_HasNormalizedNoOwnership(&pair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(value)) {
        return ZR_FALSE;
    }

    if (&pair->value != value) {
        pair->value = *value;
    }
    if (value->isGarbageCollectable) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    }
    zr_container_cache_string_lookup_pair_mru(object, pair);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_cached_field_pair_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    pair = (hotPairSlot != ZR_NULL) ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = object->cachedStringLookupPair2;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        zr_container_cache_string_lookup_pair_mru(object, pair);
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair == ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
        key.type = ZR_VALUE_TYPE_STRING;
        pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    }

    zr_container_cache_string_lookup_pair_mru(object, pair);
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_cached_field_value_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);

    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || hotPairSlot == ZR_NULL) {
        return;
    }

    pair = object->cachedStringLookupPair;
    if (!zr_container_pair_matches_field_string(state, pair, fieldString)) {
        pair = object->cachedStringLookupPair2;
        if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
            zr_container_cache_string_lookup_pair_mru(object, pair);
        } else {
            pair = zr_container_find_own_string_pair_exact(object, fieldString);
            zr_container_cache_string_lookup_pair_mru(object, pair);
        }
    }
    *hotPairSlot = pair;
}

static ZR_FORCE_INLINE TZrBool zr_container_set_cached_field_value_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString,
                                                                        SZrHashKeyValuePair **hotPairSlot,
                                                                        const SZrTypeValue *value,
                                                                        TZrBool refreshHiddenItemsObject) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);
    if (pair != ZR_NULL) {
        if (!refreshHiddenItemsObject &&
            object->cachedHiddenItemsPair == ZR_NULL &&
            object->cachedHiddenItemsObject == ZR_NULL &&
            zr_container_try_set_existing_pair_value_plain_fast(state, object, pair, value)) {
            object->memberVersion++;
            return ZR_TRUE;
        }

        ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        object->memberVersion++;
        if (refreshHiddenItemsObject) {
            object->cachedHiddenItemsObject = ZR_NULL;
            if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
                SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

                if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                    object->cachedHiddenItemsObject = hiddenItemsObject;
                }
            }
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    if (refreshHiddenItemsObject) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_value_fast(SZrState *state,
                                                                           SZrObject *iterator,
                                                                           SZrString *fieldString,
                                                                           SZrHashKeyValuePair **hotPairSlot,
                                                                           const SZrTypeValue *value) {
    return fieldString != ZR_NULL &&
           zr_container_set_cached_field_value_fast(state, iterator, fieldString, hotPairSlot, value, ZR_FALSE);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_source_object_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            SZrObject *source,
                                                                            EZrValueType sourceType) {
    SZrTypeValue fieldValue;

    if (source == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, source, sourceType);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_source_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorSourcePair : ZR_NULL,
                                                       &fieldValue);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_value_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            const SZrTypeValue *value) {
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_current_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_int_pair_fast(SZrObject *iterator,
                                                                              SZrHashKeyValuePair **hotPairSlot,
                                                                              TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    if (iterator == ZR_NULL || hotPairSlot == ZR_NULL || *hotPairSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = *hotPairSlot;
    if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    iterator->memberVersion++;
    iterator->cachedStringLookupPair = pair;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_int_fast(SZrState *state,
                                                                          SZrObject *iterator,
                                                                          TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_null_fast(SZrState *state, SZrObject *iterator) {
    SZrTypeValue fieldValue;

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_index_fast(SZrState *state,
                                                                    SZrObject *iterator,
                                                                    TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_index_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       &fieldValue);
}

static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_next_node_fast(SZrState *state,
                                                                        SZrObject *iterator,
                                                                        SZrObject *nextNode) {
    SZrTypeValue fieldValue;

    if (nextNode == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, nextNode, ZR_VALUE_TYPE_OBJECT);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_next_node_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorNextNodePair : ZR_NULL,
                                                       &fieldValue);
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_field_pair_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair *pair;
    SZrHashKeyValuePair **hotPairSlot;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    pair = hotPairSlot != ZR_NULL ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (pair != ZR_NULL &&
        pair->key.type == ZR_VALUE_TYPE_STRING &&
        pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair != ZR_NULL) {
        object->cachedStringLookupPair = pair;
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    object->cachedStringLookupPair = pair;
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_own_field_value_fast(SZrState *state,
                                                                                  SZrObject *object,
                                                                                  const TZrChar *fieldName) {
    SZrHashKeyValuePair *pair;
    const SZrTypeValue *hotValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    hotValue = zr_container_try_get_hot_field_value_exact(state, object, fieldName);
    if (hotValue != ZR_NULL) {
        return hotValue;
    }

    pair = zr_container_find_own_field_pair_fast(state, object, fieldName);
    if (pair != ZR_NULL) {
        return &pair->value;
    }

    return zr_container_get_field_value(state, object, fieldName);
}

static ZR_FORCE_INLINE SZrObject *zr_container_get_object_field_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_first_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_first_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedLengthPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedLengthPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairFirstField);
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_second_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedCapacityPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairSecondField);
}

static ZR_FORCE_INLINE TZrBool zr_container_pair_set_second_fast(SZrState *state,
                                                                 SZrObject *pair,
                                                                 const SZrTypeValue *value) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || pair == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = zr_container_try_match_cached_field_pair_exact(pair->cachedCapacityPair, fieldString);
    if (cachedPair != ZR_NULL) {
        if (!zr_container_try_set_existing_pair_value_plain_fast(state, pair, cachedPair, value)) {
            ZrCore_Object_SetExistingPairValueUnchecked(state, pair, cachedPair, value);
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }

            pair->cachedStringLookupPair = cachedPair;
        }

        pair->memberVersion++;
        return ZR_TRUE;
    }

    if (fieldString != ZR_NULL &&
        zr_container_set_cached_field_value_fast(state,
                                                 pair,
                                                 fieldString,
                                                 pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL,
                                                 value,
                                                 ZR_FALSE)) {
        return ZR_TRUE;
    }
    return zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, value);
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_first_value_fast(SZrState *state,
                                                                                        SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedLengthPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_first_fast(state, entryObject);
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_second_value_fast(SZrState *state,
                                                                                         SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_second_fast(state, entryObject);
}

static ZR_FORCE_INLINE TZrBool zr_container_map_entry_set_second_value_fast(SZrState *state,
                                                                            SZrObject *entryObject,
                                                                            const SZrTypeValue *value) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair == ZR_NULL) {
        return zr_container_pair_set_second_fast(state, entryObject, value);
    }

    if (!zr_container_try_set_existing_pair_value_plain_fast(state, entryObject, cachedPair, value)) {
        ZrCore_Object_SetExistingPairValueUnchecked(state, entryObject, cachedPair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        entryObject->cachedStringLookupPair = cachedPair;
    }

    entryObject->memberVersion++;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE const SZrTypeValue *zr_container_array_get_value_fast(SZrState *state,
                                                                             SZrObject *array,
                                                                             TZrSize index) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    if (array->nodeMap.isValid && array->nodeMap.buckets != ZR_NULL && index < array->nodeMap.elementCount &&
        index < array->nodeMap.capacity) {
        pair = array->nodeMap.buckets[index];
        if (pair != ZR_NULL &&
            ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
            pair->key.value.nativeObject.nativeInt64 == (TZrInt64)index) {
            return &pair->value;
        }
    }

    return ZrLib_Array_Get(state, array, index);
}

static ZR_FORCE_INLINE TZrSize zr_container_array_length_fast(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0u;
    }

    return (array->superArrayRawIntData != ZR_NULL &&
            array->superArrayRawIntLength <= array->superArrayRawIntCapacity)
                   ? array->superArrayRawIntLength
                   : array->nodeMap.elementCount;
}

static ZR_FORCE_INLINE TZrBool zr_container_array_raw_int_active(const SZrObject *array) {
    return array != ZR_NULL &&
           array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
           array->superArrayRawIntData != ZR_NULL &&
           array->superArrayRawIntLength <= array->superArrayRawIntCapacity;
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_array_dense_int_pair_at(SZrObject *array, TZrSize index) {
    SZrHashKeyValuePair *pair;

    if (!zr_container_array_raw_int_active(array) ||
        !array->nodeMap.isValid ||
        array->nodeMap.buckets == ZR_NULL ||
        index >= array->nodeMap.capacity) {
        return ZR_NULL;
    }

    pair = array->nodeMap.buckets[index];
    if (pair == ZR_NULL ||
        pair->next != ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) ||
        pair->key.value.nativeObject.nativeInt64 != (TZrInt64)index) {
        return ZR_NULL;
    }
    return pair;
}

static ZR_FORCE_INLINE void zr_container_array_store_dense_int_pair_value(SZrHashKeyValuePair *pair, TZrInt64 value) {
    ZR_ASSERT(pair != ZR_NULL);
    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
}

static void zr_container_hash_set_clear_reuse_storage(SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return;
    }

    memset(set->buckets, 0, set->capacity * sizeof(set->buckets[0]));
    set->elementCount = 0;
    set->pairPoolUsed = 0;
    set->pairPoolActive = set->pairPoolHead;
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        block->used = 0;
    }
}

static void zr_container_array_clear_items_reuse_storage(SZrObject *items) {
    if (items == ZR_NULL || items->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return;
    }

    zr_container_hash_set_clear_reuse_storage(&items->nodeMap);
    items->cachedHiddenItemsPair = ZR_NULL;
    items->cachedHiddenItemsObject = ZR_NULL;
    items->cachedLengthPair = ZR_NULL;
    items->cachedCapacityPair = ZR_NULL;
    items->cachedStringLookupPair = ZR_NULL;
    items->cachedStringLookupPair2 = ZR_NULL;
    items->cachedIteratorSourcePair = ZR_NULL;
    items->cachedIteratorCurrentPair = ZR_NULL;
    items->cachedIteratorIndexPair = ZR_NULL;
    items->cachedIteratorNextNodePair = ZR_NULL;
    items->superArrayRawIntLength = 0;
    items->superArrayRawIntDirty = ZR_FALSE;
    items->memberVersion++;
}

static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index) {
    const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, array, index);

    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static ZR_FORCE_INLINE void zr_container_array_make_receiver_value(SZrState *state,
                                                                   SZrObject *self,
                                                                   SZrTypeValue *outReceiver) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(self != ZR_NULL);
    ZR_ASSERT(outReceiver != ZR_NULL);

    zr_container_value_set_object_fast(state, outReceiver, self, zr_container_value_type_for_object(self));
}

static TZrInt64 zr_container_get_int_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 defaultValue) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL) {
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

static TZrBool zr_container_call_method(SZrState *state,
                                        SZrObject *receiver,
                                        const TZrChar *methodName,
                                        const SZrTypeValue *arguments,
                                        TZrSize argumentCount,
                                        SZrTypeValue *result) {
    const SZrTypeValue *callable;
    SZrTypeValue receiverValue;

    if (state == ZR_NULL || receiver == ZR_NULL || methodName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = zr_container_get_field_value(state, receiver, methodName);
    if (callable == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(state, &receiverValue, receiver, zr_container_value_type_for_object(receiver));
    return ZrLib_CallValue(state, callable, &receiverValue, arguments, argumentCount, result);
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value);

static TZrBool zr_container_values_equal(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue lhsCopy;
    SZrTypeValue rhsCopy;
    SZrTypeValue result;

    if (state == ZR_NULL || lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    lhsCopy = *lhs;
    rhsCopy = *rhs;
    if (ZrCore_Value_Equal(state, &lhsCopy, &rhsCopy) || ZrCore_Value_CompareDirectly(state, &lhsCopy, &rhsCopy)) {
        return ZR_TRUE;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "equals", rhs, 1, &result) && result.type == ZR_VALUE_TYPE_BOOL) {
            return (TZrBool)(result.value.nativeObject.nativeBool != 0);
        }
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool zr_container_int_values_equal_fast(const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    TZrBool lhsSigned;
    TZrBool rhsSigned;

    if (lhs == ZR_NULL || rhs == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(lhs->type) || !ZR_VALUE_IS_TYPE_INT(rhs->type)) {
        return ZR_FALSE;
    }

    lhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(lhs->type);
    rhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(rhs->type);
    if (lhsSigned && rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeInt64);
    }
    if (!lhsSigned && !rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeUInt64 == rhs->value.nativeObject.nativeUInt64);
    }
    if (lhsSigned) {
        return lhs->value.nativeObject.nativeInt64 >= 0 &&
               (TZrUInt64)lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeUInt64;
    }
    return rhs->value.nativeObject.nativeInt64 >= 0 &&
           lhs->value.nativeObject.nativeUInt64 == (TZrUInt64)rhs->value.nativeObject.nativeInt64;
}

static ZR_FORCE_INLINE TZrBool zr_container_try_read_int64_needle_fast(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void zr_container_result_set_int_fast(SZrTypeValue *result, TZrInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    }
}

static ZR_FORCE_INLINE void zr_container_result_set_bool_fast(SZrTypeValue *result, TZrBool value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeBool, value, ZR_VALUE_TYPE_BOOL);
    }
}

static ZR_FORCE_INLINE void zr_container_result_set_uint_fast(SZrTypeValue *result, TZrUInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeUInt64, value, ZR_VALUE_TYPE_UINT64);
    }
}

static ZR_FORCE_INLINE void zr_container_result_set_double_fast(SZrTypeValue *result, TZrDouble value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeDouble, value, ZR_VALUE_TYPE_DOUBLE);
    }
}

static ZR_FORCE_INLINE void zr_container_result_set_null_fast(SZrTypeValue *result) {
    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
    }
}

static ZR_FORCE_INLINE TZrBool zr_container_result_copy_no_profile(SZrState *state,
                                                                    SZrTypeValue *result,
                                                                    const SZrTypeValue *value) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_container_result_set_null_fast(result);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL:
            zr_container_result_set_bool_fast(result, value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT64:
            zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            zr_container_result_set_uint_fast(result, value->value.nativeObject.nativeUInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_DOUBLE:
            zr_container_result_set_double_fast(result, value->value.nativeObject.nativeDouble);
            return ZR_TRUE;
        default:
            break;
    }

    ZrCore_Value_CopyNoProfile(state, result, value);
    return state == ZR_NULL || state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static TZrInt64 zr_container_values_compare(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue result;
    const TZrChar *lhsText;
    const TZrChar *rhsText;
    TZrDouble lhsNumber;
    TZrDouble rhsNumber;
    TZrUInt64 lhsHash;
    TZrUInt64 rhsHash;

    if (lhs == ZR_NULL && rhs == ZR_NULL) {
        return 0;
    }
    if (lhs == ZR_NULL) {
        return -1;
    }
    if (rhs == ZR_NULL) {
        return 1;
    }
    if (zr_container_values_equal(state, lhs, rhs)) {
        return 0;
    }

    if (lhs->type == ZR_VALUE_TYPE_STRING && rhs->type == ZR_VALUE_TYPE_STRING) {
        lhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, lhs->value.object));
        rhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, rhs->value.object));
        if (lhsText == ZR_NULL) {
            return rhsText == ZR_NULL ? 0 : -1;
        }
        if (rhsText == ZR_NULL) {
            return 1;
        }
        return strcmp(lhsText, rhsText);
    }

    if ((ZR_VALUE_IS_TYPE_INT(lhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) || ZR_VALUE_IS_TYPE_FLOAT(lhs->type)) &&
        (ZR_VALUE_IS_TYPE_INT(rhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type) || ZR_VALUE_IS_TYPE_FLOAT(rhs->type))) {
        lhsNumber = ZR_VALUE_IS_TYPE_FLOAT(lhs->type) ? lhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)lhs->value.nativeObject.nativeInt64;
        rhsNumber = ZR_VALUE_IS_TYPE_FLOAT(rhs->type) ? rhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)rhs->value.nativeObject.nativeInt64;
        return lhsNumber < rhsNumber ? -1 : 1;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "compareTo", rhs, 1, &result) &&
            (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type))) {
            return ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)
                           ? result.value.nativeObject.nativeInt64
                           : (TZrInt64)result.value.nativeObject.nativeUInt64;
        }
    }

    lhsHash = zr_container_value_hash(state, lhs);
    rhsHash = zr_container_value_hash(state, rhs);
    if (lhsHash == rhsHash) {
        return (TZrInt64)lhs->type - (TZrInt64)rhs->type;
    }
    return lhsHash < rhsHash ? -1 : 1;
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value) {
    SZrTypeValue result;

    if (state == ZR_NULL || value == ZR_NULL) {
        return 0;
    }

    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, value->value.object);
        if (zr_container_call_method(state, receiver, "hashCode", ZR_NULL, 0, &result)) {
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
                return (TZrUInt64)result.value.nativeObject.nativeInt64;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
                return result.value.nativeObject.nativeUInt64;
            }
        }
    }

    return ZrCore_Value_GetHash(state, value);
}

static TZrBool zr_container_storage_sync_raw_int_pairs_fast(SZrObject *array);

static TZrBool zr_container_storage_set(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static TZrBool zr_container_storage_push_raw_int_fast(SZrState *state, SZrObject *array, TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize length;
    TZrBool wasDirty = ZR_FALSE;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        length = array->superArrayRawIntLength;
        if (array->nodeMap.elementCount != length) {
            return ZR_FALSE;
        }
        wasDirty = array->superArrayRawIntDirty;
    } else {
        length = array->nodeMap.elementCount;
        if (length != 0) {
            return ZR_FALSE;
        }
    }

    nodeMap = &array->nodeMap;
    if (!ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1) ||
        !ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, nodeMap, length + 1) ||
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state, nodeMap, nodeMap->pairPoolUsed + 1) ||
        length >= nodeMap->capacity ||
        nodeMap->buckets[length] != ZR_NULL) {
        return ZR_FALSE;
    }

    pair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)length, ZR_VALUE_TYPE_INT64);
    zr_container_array_store_dense_int_pair_value(pair, value);
    nodeMap->buckets[length] = pair;
    nodeMap->elementCount++;
    array->superArrayRawIntData[length] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayRawIntDirty = wasDirty;
    return ZR_TRUE;
}

static TZrBool zr_container_storage_push(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
        zr_container_storage_push_raw_int_fast(state, array, value->value.nativeObject.nativeInt64)) {
        return ZR_TRUE;
    }

    return zr_container_storage_set(state, array, zr_container_array_length_fast(array), value);
}

static TZrBool zr_container_storage_remove_last(SZrState *state, SZrObject *array) {
    SZrTypeValue key;
    TZrSize length;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (length == 0) {
        return ZR_FALSE;
    }

    if (array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        array->nodeMap.isValid &&
        array->nodeMap.buckets != ZR_NULL &&
        length - 1 < array->nodeMap.capacity) {
        pair = array->nodeMap.buckets[length - 1];
        if (pair != ZR_NULL &&
            pair->next == ZR_NULL &&
            ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
            pair->key.value.nativeObject.nativeInt64 == (TZrInt64)(length - 1)) {
            array->nodeMap.buckets[length - 1] = ZR_NULL;
            array->nodeMap.elementCount--;
            return ZR_TRUE;
        }
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)(length - 1));
    ZrCore_HashSet_Remove(state, &array->nodeMap, &key);
    return ZR_TRUE;
}

static TZrBool zr_container_storage_sync_raw_int_pairs_fast(SZrObject *array) {
    TZrSize length;

    if (!zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (array->nodeMap.elementCount != length) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length; index++) {
        SZrHashKeyValuePair *pair = zr_container_array_dense_int_pair_at(array, index);

        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }
        zr_container_array_store_dense_int_pair_value(pair, array->superArrayRawIntData[index]);
    }
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool zr_container_storage_remove_at_raw_int_fast(SZrState *state, SZrObject *array, TZrSize index) {
    SZrHashSet *nodeMap;
    TZrSize length;
    TZrBool wasDirty;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index >= length || array->nodeMap.elementCount != length) {
        return ZR_FALSE;
    }

    nodeMap = &array->nodeMap;
    if (!nodeMap->isValid || nodeMap->buckets == ZR_NULL || length > nodeMap->capacity) {
        return ZR_FALSE;
    }
    for (TZrSize cursor = index; cursor < length; cursor++) {
        if (zr_container_array_dense_int_pair_at(array, cursor) == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    wasDirty = array->superArrayRawIntDirty;

    if (index + 1 < length) {
        TZrSize movedCount = length - index - 1;

        memmove(array->superArrayRawIntData + index,
                array->superArrayRawIntData + index + 1,
                movedCount * sizeof(array->superArrayRawIntData[0]));
        memmove(nodeMap->buckets + index,
                nodeMap->buckets + index + 1,
                movedCount * sizeof(nodeMap->buckets[0]));

        for (TZrSize cursor = index; cursor + 1 < length; cursor++) {
            SZrHashKeyValuePair *pair = nodeMap->buckets[cursor];

            ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)cursor, ZR_VALUE_TYPE_INT64);
            if (!wasDirty) {
                zr_container_array_store_dense_int_pair_value(pair, array->superArrayRawIntData[cursor]);
            }
        }
    }
    nodeMap->buckets[length - 1] = ZR_NULL;
    nodeMap->elementCount--;
    array->superArrayRawIntLength = length - 1;
    array->superArrayRawIntDirty = wasDirty;
    return ZR_TRUE;
}

static TZrBool zr_container_storage_insert_raw_int_fast(SZrState *state,
                                                        SZrObject *array,
                                                        TZrSize index,
                                                        TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *newPair;
    TZrSize length;
    TZrBool wasDirty;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index > length || array->nodeMap.elementCount != length ||
        !ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1)) {
        return ZR_FALSE;
    }

    nodeMap = &array->nodeMap;
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, nodeMap, length + 1) ||
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state, nodeMap, nodeMap->pairPoolUsed + 1)) {
        return ZR_FALSE;
    }

    if (length >= nodeMap->capacity || nodeMap->buckets[length] != ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize cursor = index; cursor < length; cursor++) {
        if (zr_container_array_dense_int_pair_at(array, cursor) == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    wasDirty = array->superArrayRawIntDirty;

    newPair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (newPair == ZR_NULL) {
        return ZR_FALSE;
    }
    nodeMap->elementCount++;

    if (index < length) {
        TZrSize movedCount = length - index;

        memmove(array->superArrayRawIntData + index + 1,
                array->superArrayRawIntData + index,
                movedCount * sizeof(array->superArrayRawIntData[0]));
        memmove(nodeMap->buckets + index + 1,
                nodeMap->buckets + index,
                movedCount * sizeof(nodeMap->buckets[0]));

        for (TZrSize cursor = index + 1; cursor <= length; cursor++) {
            SZrHashKeyValuePair *pair = nodeMap->buckets[cursor];

            ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)cursor, ZR_VALUE_TYPE_INT64);
            if (!wasDirty) {
                zr_container_array_store_dense_int_pair_value(pair, array->superArrayRawIntData[cursor]);
            }
        }
    }

    newPair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&newPair->key, nativeInt64, (TZrInt64)index, ZR_VALUE_TYPE_INT64);
    zr_container_array_store_dense_int_pair_value(newPair, value);
    nodeMap->buckets[index] = newPair;
    array->superArrayRawIntData[index] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayRawIntDirty = wasDirty;
    return ZR_TRUE;
}

static TZrBool zr_container_storage_insert(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index > length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
            zr_container_storage_insert_raw_int_fast(state, array, index, value->value.nativeObject.nativeInt64)) {
            return ZR_TRUE;
        }
        if (array->superArrayRawIntDirty && !zr_container_storage_sync_raw_int_pairs_fast(array)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize cursor = length; cursor > index; cursor--) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor - 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_set(state, array, index, value);
}

static TZrBool zr_container_storage_remove_at(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index >= length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        return zr_container_storage_remove_at_raw_int_fast(state, array, index);
    }

    for (TZrSize cursor = index; cursor + 1 < length; cursor++) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor + 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_remove_last(state, array);
}

static ZR_FORCE_INLINE SZrObject *zr_container_get_hidden_array_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    SZrObject *array;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, &object->cachedHiddenItemsPair);
    if (pair == ZR_NULL || (pair->value.type != ZR_VALUE_TYPE_OBJECT && pair->value.type != ZR_VALUE_TYPE_ARRAY) ||
        pair->value.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZR_CAST_OBJECT(state, pair->value.value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        object->cachedHiddenItemsObject = ZR_NULL;
        return ZR_NULL;
    }

    object->cachedHiddenItemsObject = array;
    return array;
}

static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_get_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_cached_fast(SZrState *state, SZrObject *object) {
    SZrObject *array;

    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    return zr_container_get_entries_array_fast(state, object);
}

static ZR_FORCE_INLINE SZrObject *zr_container_ensure_hidden_array_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString) {
    SZrObject *array = zr_container_get_hidden_array_fast(state, object, fieldString);

    if (array != ZR_NULL) {
        return array;
    }

    array = ZrLib_Array_New(state);
    if (array != ZR_NULL) {
        SZrTypeValue fieldValue;

        zr_container_value_set_object_fast(state, &fieldValue, array, ZR_VALUE_TYPE_ARRAY);
        if (fieldString == ZR_NULL ||
            !zr_container_set_cached_field_value_fast(state,
                                                      object,
                                                      fieldString,
                                                      &object->cachedHiddenItemsPair,
                                                      &fieldValue,
                                                      ZR_TRUE)) {
            return ZR_NULL;
        }
    }

    return array;
}

static ZR_FORCE_INLINE SZrObject *zr_container_ensure_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

static ZR_FORCE_INLINE SZrObject *zr_container_ensure_items_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_items_field_string_fast(state));
}

static SZrObject *zr_container_make_pair(SZrState *state, const SZrTypeValue *first, const SZrTypeValue *second) {
    SZrObject *pair = ZrLib_Type_NewInstance(state, "Pair");
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairFirstField, first)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairFirstField)) {
            return ZR_NULL;
        }
    }
    if (second != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, second)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairSecondField)) {
            return ZR_NULL;
        }
    }
    return pair;
}

static SZrObject *zr_container_make_linked_node(SZrState *state, const SZrTypeValue *value) {
    SZrObject *node = ZrLib_Type_NewInstance(state, "LinkedNode");
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (value != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, node, kContainerValueField, value)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, node, kContainerValueField)) {
            return ZR_NULL;
        }
    }
    if (!zr_container_set_null_field_fast(state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(state, node, kContainerPreviousField)) {
        return ZR_NULL;
    }
    return node;
}

static TZrBool zr_container_map_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *key,
                                           TZrSize *outIndex,
                                           SZrObject **outEntryObject) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = ZR_NULL;
    }
    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_FALSE;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;

        wantedHash = wantedRawObject->hash;
        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            if (zr_container_try_hot_map_lookup_cache(state,
                                                      cache,
                                                      entries,
                                                      wantedRawObject,
                                                      outIndex,
                                                      outEntryObject)) {
                return ZR_TRUE;
            }
        }

        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_FALSE;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;
            SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                if (outEntryObject != ZR_NULL) {
                    *outEntryObject = entryObject;
                }
                zr_container_update_hot_map_lookup_cache(state,
                                                         entries,
                                                         wantedRawObject,
                                                         index,
                                                         entryObject);
                return ZR_TRUE;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    if (outIndex != ZR_NULL) {
                        *outIndex = index;
                    }
                    if (outEntryObject != ZR_NULL) {
                        *outEntryObject = entryObject;
                    }
                    zr_container_update_hot_map_lookup_cache(state,
                                                             entries,
                                                             wantedRawObject,
                                                             index,
                                                             entryObject);
                    return ZR_TRUE;
                }
            }
        }

        zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, 0u, ZR_NULL);
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            if (outEntryObject != ZR_NULL) {
                *outEntryObject = entryObject;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE SZrObject *zr_container_map_find_entry_object_fast(SZrState *state,
                                                                          SZrObject *entries,
                                                                          const SZrTypeValue *key) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;
        SZrObject *entryObject = ZR_NULL;

        wantedHash = wantedRawObject->hash;
        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            if (zr_container_try_hot_map_lookup_cache(
                        state, cache, entries, wantedRawObject, ZR_NULL, &entryObject)) {
                return entryObject;
            }
        }

        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_NULL;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;

            entryObject = zr_container_array_get_object_fast(state, entries, index);
            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                return entryObject;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                    return entryObject;
                }
            }
        }

        return ZR_NULL;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

static TZrBool zr_container_set_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *value,
                                           TZrSize *outIndex) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || entries == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(entries);
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        if (zr_container_array_raw_int_active(entries)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(value, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (entries->superArrayRawIntData[index] == signedNeedle) {
                        if (outIndex != ZR_NULL) {
                            *outIndex = index;
                        }
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);

            if (zr_container_int_values_equal_fast(entryValue, value)) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, value);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);
        if (entryValue != ZR_NULL && zr_container_value_hash(state, entryValue) == wantedHash &&
            zr_container_values_equal(state, entryValue, value)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_container_array_ensure_capacity(SZrState *state, SZrObject *arrayObject, TZrSize requiredLength) {
    TZrInt64 capacity;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    capacity = zr_container_get_int_field(state, arrayObject, kContainerCapacityField, 0);
    if ((TZrSize)capacity >= requiredLength) {
        return ZR_TRUE;
    }

    if (capacity <= 0) {
        capacity = ZR_CONTAINER_SEQUENCE_INITIAL_CAPACITY;
    }
    while ((TZrSize)capacity < requiredLength) {
        capacity *= ZR_CONTAINER_SEQUENCE_GROWTH_FACTOR;
    }
    return zr_container_set_int_field_fast(state, arrayObject, kContainerCapacityField, capacity);
}

static TZrBool zr_container_array_prepare_backing_storage(SZrState *state,
                                                          SZrObject *itemsObject,
                                                          TZrSize requiredCapacity) {
    if (state == ZR_NULL || itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }
    if (requiredCapacity == 0) {
        return ZR_TRUE;
    }

    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, &itemsObject->nodeMap, requiredCapacity)) {
        return ZR_FALSE;
    }

    return ZrCore_HashSet_EnsurePairPoolForElementCount(state, &itemsObject->nodeMap, requiredCapacity);
}

static SZrObject *zr_container_iterator_make(SZrState *state,
                                             SZrObject *source,
                                             EZrValueType sourceType,
                                             TZrInt64 indexValue,
                                             SZrObject *nextNode,
                                             FZrNativeFunction moveNextFunction) {
    SZrObject *iterator;
    SZrObjectPrototype *iteratorPrototype;

    if (state == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    iteratorPrototype = zr_container_iterator_runtime_prototype(state, moveNextFunction);
    if (iteratorPrototype == ZR_NULL) {
        return ZR_NULL;
    }

    iterator = ZrCore_Object_New(state, iteratorPrototype);
    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, iterator);

    if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
        return ZR_NULL;
    }
    if (!zr_container_set_iterator_source_object_fast(state, iterator, source, sourceType) ||
        !zr_container_set_iterator_index_fast(state, iterator, indexValue) ||
        !zr_container_set_iterator_next_node_fast(state, iterator, nextNode)) {
        return ZR_NULL;
    }
    return iterator;
}

static SZrObject *zr_container_iterator_self(SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *selfValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = state->callInfoList;
    selfValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, selfValue->value.object);
}

static TZrInt64 zr_container_iterator_finish_move_next(SZrState *state, TZrBool ok) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ok, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static ZR_FORCE_INLINE SZrObject *zr_container_iterator_source_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorSourcePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerSourceField);
}

static ZR_FORCE_INLINE SZrObject *zr_container_iterator_next_node_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorNextNodePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerNextNodeField);
}

static ZR_FORCE_INLINE TZrInt64 zr_container_iterator_index_fast(SZrState *state,
                                                                 SZrObject *iterator,
                                                                 TZrInt64 defaultValue) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return defaultValue;
    }

    pair = iterator->cachedIteratorIndexPair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
            return value->value.nativeObject.nativeInt64;
        }
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
            return (TZrInt64)value->value.nativeObject.nativeUInt64;
        }
    }

    return zr_container_get_int_field(state, iterator, kContainerIndexField, defaultValue);
}

static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *source;
    TZrInt64 index;
    const SZrTypeValue *current;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    source = zr_container_iterator_source_object_fast(state, iterator);
    index = zr_container_iterator_index_fast(state, iterator, 0);
    if (source != ZR_NULL &&
        source->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        source->superArrayRawIntData != ZR_NULL &&
        source->superArrayRawIntLength <= source->superArrayRawIntCapacity) {
        if (index < 0 || (TZrUInt64)index >= (TZrUInt64)source->superArrayRawIntLength) {
            if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
                return zr_container_iterator_finish_move_next(state, ZR_FALSE);
            }
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        if (!zr_container_set_iterator_current_int_fast(state, iterator, source->superArrayRawIntData[(TZrSize)index]) ||
            !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_TRUE);
    }

    current = (source != ZR_NULL && index >= 0) ? ZrLib_Array_Get(state, source, (TZrSize)index) : ZR_NULL;
    if (current == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    if (!zr_container_set_iterator_current_value_fast(state, iterator, current) ||
        !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *node;
    const SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    node = zr_container_iterator_next_node_object_fast(state, iterator);
    if (node == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    value = zr_container_get_own_field_value_fast(state, node, kContainerValueField);
    if (value != ZR_NULL) {
        if (!zr_container_set_iterator_current_value_fast(state, iterator, value)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    } else {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    }
    if (!zr_container_set_iterator_next_node_fast(state,
                                                  iterator,
                                                  zr_container_get_object_field_fast(state, node, kContainerNextField))) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

static TZrBool zr_container_pair_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *pair;
    TZrSize argc;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    argc = ZrLib_CallContext_ArgumentCount(context);
    if (argc != 0 && argc != 2) {
        ZrLib_CallContext_RaiseArityError(context, 0, 2);
    }

    pair = zr_container_resolve_construct_target(context);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argc == 2) {
        if (!zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairFirstField,
                                               ZrLib_CallContext_Argument(context, 0)) ||
            !zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairSecondField,
                                               ZrLib_CallContext_Argument(context, 1))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, pair, kContainerPairFirstField) ||
            !zr_container_set_null_field_fast(context->state, pair, kContainerPairSecondField)) {
            return ZR_FALSE;
        }
    }

    return zr_container_finish_object(context, result, pair);
}

static TZrBool zr_container_pair_equals(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    const SZrTypeValue *selfFirst;
    const SZrTypeValue *selfSecond;
    const SZrTypeValue *otherFirst;
    const SZrTypeValue *otherSecond;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    if (!zr_container_object_is_owner_instance(context, other)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    selfFirst = zr_container_pair_get_first_fast(context->state, self);
    selfSecond = zr_container_pair_get_second_fast(context->state, self);
    otherFirst = zr_container_pair_get_first_fast(context->state, other);
    otherSecond = zr_container_pair_get_second_fast(context->state, other);
    zr_container_result_set_bool_fast(result,
                                      zr_container_values_equal(context->state, selfFirst, otherFirst) &&
                                              zr_container_values_equal(context->state, selfSecond, otherSecond));
    return ZR_TRUE;
}

static TZrBool zr_container_pair_compare(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    TZrInt64 compare;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_int_fast(result, 1);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    compare = zr_container_values_compare(context->state,
                                          zr_container_pair_get_first_fast(context->state, self),
                                          zr_container_pair_get_first_fast(context->state, other));
    if (compare == 0) {
        compare = zr_container_values_compare(context->state,
                                              zr_container_pair_get_second_fast(context->state, self),
                                              zr_container_pair_get_second_fast(context->state, other));
    }
    zr_container_result_set_int_fast(result, compare);
    return ZR_TRUE;
}

static TZrBool zr_container_pair_hash_code(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    TZrUInt64 firstHash;
    TZrUInt64 secondHash;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL) {
        return ZR_FALSE;
    }

    firstHash = zr_container_value_hash(context->state, zr_container_pair_get_first_fast(context->state, self));
    secondHash = zr_container_value_hash(context->state, zr_container_pair_get_second_fast(context->state, self));
    zr_container_result_set_int_fast(result,
                                     (TZrInt64)((firstHash * ZR_CONTAINER_HASH_MIX_PRIME) ^
                                                (secondHash + ZR_CONTAINER_HASH_MIX_OFFSET)));
    return ZR_TRUE;
}

static TZrBool zr_container_array_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *arrayObject = zr_container_resolve_construct_target(context);
    TZrInt64 capacity = 0;
    SZrObject *items;

    if (arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLib_CallContext_ArgumentCount(context) == 1 && !ZrLib_CallContext_ReadInt(context, 0, &capacity)) {
        return ZR_FALSE;
    }
    if (capacity < 0) {
        ZrCore_Debug_RunError(context->state, "Array capacity must be non-negative");
    }

    items = ZrLib_Array_New(context->state);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_array_prepare_backing_storage(context->state, items, capacity > 0 ? (TZrSize)capacity : 0)) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, arrayObject, kContainerItemsField, items) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerLengthField, 0) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerCapacityField, capacity)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, arrayObject);
}

static TZrBool zr_container_array_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(items);
    if (value == ZR_NULL || !zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !zr_container_storage_push(context->state, items, value) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_insert(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    length = zr_container_array_length_fast(items);
    if (indexValue < 0 || (TZrSize)indexValue > length) {
        ZrCore_Debug_RunError(context->state, "Array.insert index out of range");
    }
    if (!zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !zr_container_storage_insert(context->state, items, (TZrSize)indexValue, ZrLib_CallContext_Argument(context, 1)) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_remove_at(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (indexValue < 0 || !zr_container_storage_remove_at(context->state, items, (TZrSize)indexValue)) {
        ZrCore_Debug_RunError(context->state, "Array.removeAt index out of range");
    }

    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)zr_container_array_length_fast(items))) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(items);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_array_index_of(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        if (zr_container_array_raw_int_active(items)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (items->superArrayRawIntData[index] == signedNeedle) {
                        zr_container_result_set_int_fast(result, (TZrInt64)index);
                        return ZR_TRUE;
                    }
                }
            }

            zr_container_result_set_int_fast(result, -1);
            return ZR_TRUE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_int_fast(result, -1);
    return ZR_TRUE;
}

static TZrBool zr_container_array_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_container_self_object(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (zr_container_array_raw_int_active(items)) {
        TZrInt64 signedNeedle;

        if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
            for (TZrSize index = 0; index < length; index++) {
                if (items->superArrayRawIntData[index] == signedNeedle) {
                    zr_container_result_set_bool_fast(result, ZR_TRUE);
                    return ZR_TRUE;
                }
            }
        }
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

static TZrBool zr_container_array_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          items,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static TZrInt64 zr_container_native_array_get_iterator_native(SZrState *state) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer base;
    SZrTypeValue *resultValue;
    SZrTypeValue *selfValue;
    SZrObject *self;
    SZrObject *iterator;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    base = callInfo->functionBase.valuePointer;
    resultValue = ZrCore_Stack_GetValue(base);
    selfValue = ZrCore_Stack_GetValue(base + 1);
    if (resultValue == ZR_NULL || selfValue == ZR_NULL ||
        selfValue->type != ZR_VALUE_TYPE_ARRAY || selfValue->value.object == ZR_NULL) {
        if (resultValue != ZR_NULL) {
            zr_container_result_set_null_fast(resultValue);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    iterator = zr_container_iterator_make(state,
                                          self,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    if (iterator == ZR_NULL) {
        zr_container_result_set_null_fast(resultValue);
    } else {
        zr_container_value_set_object_fast(state, resultValue, iterator, ZR_VALUE_TYPE_OBJECT);
    }

    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrBool zr_container_array_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }
    if (zr_container_array_raw_int_active(items)) {
        if ((TZrUInt64)indexValue >= (TZrUInt64)items->superArrayRawIntLength) {
            zr_container_result_set_null_fast(result);
            return ZR_TRUE;
        }
        zr_container_result_set_int_fast(result, items->superArrayRawIntData[(TZrSize)indexValue]);
        return ZR_TRUE;
    }

    value = zr_container_array_get_value_fast(context->state, items, (TZrSize)indexValue);
    return zr_container_result_copy_no_profile(context->state, result, value);
}

static TZrBool zr_container_array_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    const SZrTypeValue *value;
    SZrTypeValue receiverValue;
    SZrTypeValue key;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0 || (TZrSize)indexValue >= zr_container_array_length_fast(items)) {
        ZrCore_Debug_RunError(context->state, "Array index out of range");
    }

    value = ZrLib_CallContext_Argument(context, 1);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(items) && ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        items->superArrayRawIntData[(TZrSize)indexValue] = value->value.nativeObject.nativeInt64;
        items->superArrayRawIntDirty = ZR_TRUE;
        zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    zr_container_array_make_receiver_value(context->state, self, &receiverValue);
    ZrCore_Value_InitAsInt(context->state, &key, indexValue);
    if (!ZrCore_Object_SuperArraySetInt(context->state, &receiverValue, &key, value)) {
        return ZR_FALSE;
    }
    return zr_container_result_copy_no_profile(context->state, result, value);
}

static TZrBool zr_container_map_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_map_contains_key(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrBool found;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    found = zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL, ZR_NULL);
    zr_container_result_set_bool_fast(result, found);
    return ZR_TRUE;
}

static TZrBool zr_container_map_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index, ZR_NULL) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_map_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_map_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entryObject;
    SZrObject *entries;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

static TZrBool zr_container_map_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_get_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          result);
}

static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_readonly_inline_fast(SZrState *state,
                                                                              const SZrTypeValue *selfValue,
                                                                              const SZrTypeValue *keyValue,
                                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              const SZrTypeValue *mappedValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    TZrBool insertedNewEntry = ZR_FALSE;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject != ZR_NULL) {
        if (!zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue)) {
            return ZR_FALSE;
        }
    } else {
        SZrObject *pair = zr_container_make_pair(state, keyValue, mappedValue);
        SZrTypeValue pairValue;
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }

        self = ZR_CAST_OBJECT(state, selfValue->value.object);
        if (self == ZR_NULL) {
            return ZR_FALSE;
        }
        entries = zr_container_get_entries_array_cached_fast(state, self);
        if (entries == ZR_NULL) {
            entries = zr_container_ensure_entries_array_fast(state, self);
            if (entries == ZR_NULL) {
                return ZR_FALSE;
            }
        }

        zr_container_value_set_object_fast(state, &pairValue, pair, ZR_VALUE_TYPE_OBJECT);
        if (!zr_container_storage_push(state, entries, &pairValue)) {
            return ZR_FALSE;
        }
        insertedNewEntry = ZR_TRUE;
    }

    if (insertedNewEntry &&
        !zr_container_set_int_field_fast(state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    return result == ZR_NULL ? ZR_TRUE : zr_container_result_copy_no_profile(state, result, mappedValue);
}

static TZrBool zr_container_map_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_set_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          ZrLib_CallContext_Argument(context, 1),
                                          result);
}

static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_readonly_inline_no_result_fast(
        SZrState *state,
        const SZrTypeValue *selfValue,
        const SZrTypeValue *keyValue,
        const SZrTypeValue *mappedValue) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject = ZR_NULL;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries != ZR_NULL) {
        entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    }
    if (entryObject != ZR_NULL) {
        return zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue);
    }

    return zr_container_map_set_item_core(state, selfValue, keyValue, mappedValue, ZR_NULL);
}

static TZrBool zr_container_set_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_set_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (zr_container_set_find_index(context->state, entries, value, ZR_NULL)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_storage_push(context->state, entries, value) ||
        !zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_set_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    zr_container_result_set_bool_fast(
            result,
            zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL));
    return ZR_TRUE;
}

static TZrBool zr_container_set_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_container_set_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_set_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static TZrBool zr_container_linked_node_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *node = zr_container_resolve_construct_target(context);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 0) {
        if (!zr_container_set_value_field_fast(context->state,
                                               node,
                                               kContainerValueField,
                                               ZrLib_CallContext_Argument(context, 0))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, node, kContainerValueField)) {
            return ZR_FALSE;
        }
    }
    if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

static void zr_container_linked_list_unlink_node(SZrState *state, SZrObject *list, SZrObject *node) {
    SZrObject *previous;
    SZrObject *next;
    TZrInt64 count;

    if (state == ZR_NULL || list == ZR_NULL || node == ZR_NULL) {
        return;
    }

    previous = zr_container_get_object_field_fast(state, node, kContainerPreviousField);
    next = zr_container_get_object_field_fast(state, node, kContainerNextField);
    if (previous != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, previous, kContainerNextField, next);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerFirstField, next);
    }
    if (next != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, next, kContainerPreviousField, previous);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerLastField, previous);
    }

    (void)zr_container_set_null_field_fast(state, node, kContainerNextField);
    (void)zr_container_set_null_field_fast(state, node, kContainerPreviousField);
    count = zr_container_get_int_field(state, list, kContainerCountField, 0);
    (void)zr_container_set_int_field_fast(state, list, kContainerCountField, count > 0 ? count - 1 : 0);
}

static TZrBool zr_container_linked_list_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

static TZrBool zr_container_linked_list_add_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *first;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerNextField, first)) {
        return ZR_FALSE;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, first, kContainerPreviousField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

static TZrBool zr_container_linked_list_add_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *last;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerPreviousField, last)) {
        return ZR_FALSE;
    }
    if (last != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, last, kContainerNextField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

static TZrBool zr_container_linked_list_remove_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *first;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (first == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, first, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, first);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_remove_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *last;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (last == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, last, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, last);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    const SZrTypeValue *needle;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    needle = ZrLib_CallContext_Argument(context, 0);
    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        const SZrTypeValue *value = zr_container_get_own_field_value_fast(context->state, node, kContainerValueField);
        if (value != ZR_NULL && zr_container_values_equal(context->state, value, needle)) {
            zr_container_linked_list_unlink_node(context->state, self, node);
            zr_container_result_set_bool_fast(result, ZR_TRUE);
            return ZR_TRUE;
        }
        node = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        SZrObject *next = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
        if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
            !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
            return ZR_FALSE;
        }
        node = next;
    }

    if (!zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

static TZrBool zr_container_linked_list_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    iterator = zr_container_iterator_make(context->state,
                                          self,
                                          ZR_VALUE_TYPE_OBJECT,
                                          0,
                                          zr_container_get_object_field_fast(context->state, self, kContainerFirstField),
                                          zr_container_linked_list_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

static const ZrLibParameterDescriptor kArrayValueParameter[] = {{"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kArrayInsertParameters[] = {{"index", "int", ZR_NULL}, {"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kArrayIndexParameter[] = {{"index", "int", ZR_NULL}};
static const ZrLibParameterDescriptor kMapKeyParameter[] = {{"key", "K", ZR_NULL}};
static const ZrLibParameterDescriptor kMapSetItemParameters[] = {{"key", "K", ZR_NULL}, {"value", "V", ZR_NULL}};
static const ZrLibParameterDescriptor kSetValueParameter[] = {{"value", "T", ZR_NULL}};
static const ZrLibParameterDescriptor kPairParameters[] = {{"first", "K", ZR_NULL}, {"second", "V", ZR_NULL}};
static const ZrLibParameterDescriptor kPairOtherParameter[] = {{"other", "Pair<K,V>", ZR_NULL}};
static const ZrLibParameterDescriptor kLinkedNodeValueParameter[] = {{"value", "T", ZR_NULL}};

static const TZrChar *kMapKeyConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<K>"};
static const TZrChar *kSetValueConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<T>"};
static const TZrChar *kArrayImplements[] = {"zr.builtin.IArrayLike<T>", "zr.builtin.IEnumerable<T>"};
static const TZrChar *kMapImplements[] = {"zr.builtin.IEnumerable<Pair<K,V>>"};
static const TZrChar *kSetImplements[] = {"zr.builtin.IEnumerable<T>"};
static const TZrChar *kPairImplements[] = {
        "zr.builtin.IEquatable<Pair<K,V>>",
        "zr.builtin.IComparable<Pair<K,V>>",
        "zr.builtin.IHashable"
};
static const TZrChar *kLinkedListImplements[] = {"zr.builtin.IEnumerable<T>"};

static const ZrLibGenericParameterDescriptor kSingleGenericT[] = {{"T", ZR_NULL, ZR_NULL, 0}};
static const ZrLibGenericParameterDescriptor kGenericKV[] = {{"K", ZR_NULL, ZR_NULL, 0}, {"V", ZR_NULL, ZR_NULL, 0}};
static const ZrLibGenericParameterDescriptor kMapGenericParameters[] = {
        {"K", ZR_NULL, kMapKeyConstraints, ZR_ARRAY_COUNT(kMapKeyConstraints)},
        {"V", ZR_NULL, ZR_NULL, 0},
};
static const ZrLibGenericParameterDescriptor kSetGenericParameters[] = {
        {"T", ZR_NULL, kSetValueConstraints, ZR_ARRAY_COUNT(kSetValueConstraints)},
};

static const ZrLibFieldDescriptor kIteratorFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", ZR_NULL, ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD),
};
static const ZrLibMethodDescriptor kIteratorMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "bool", ZR_NULL, ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT),
};
static const ZrLibMethodDescriptor kIterableMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, ZR_NULL, "zr.builtin.IEnumerator<T>", ZR_NULL, ZR_FALSE, ZR_NULL,
                                           0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
static const ZrLibFieldDescriptor kArrayLikeFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", ZR_NULL),
};
static const ZrLibMetaMethodDescriptor kArrayLikeMetaMethods[] = {
        {ZR_META_GET_ITEM, 1, 1, ZR_NULL, "T", ZR_NULL, kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)},
        {ZR_META_SET_ITEM, 2, 2, ZR_NULL, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};
static const ZrLibMethodDescriptor kEquatableMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, ZR_NULL, "bool", ZR_NULL, ZR_FALSE, kSetValueParameter,
                                      ZR_ARRAY_COUNT(kSetValueParameter)),
};
static const ZrLibMethodDescriptor kComparableMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, ZR_NULL, "int", ZR_NULL, ZR_FALSE, kSetValueParameter,
                                      ZR_ARRAY_COUNT(kSetValueParameter)),
};
static const ZrLibMethodDescriptor kHashableMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, ZR_NULL, "int", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibFieldDescriptor kArrayFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("capacity", "int", ZR_NULL),
};
static const ZrLibMethodDescriptor kArrayMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_array_add, "null", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("insert", 2, 2, zr_container_array_insert, "null", ZR_NULL, ZR_FALSE,
                                      kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeAt", 1, 1, zr_container_array_remove_at, "null", ZR_NULL, ZR_FALSE,
                                      kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_array_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_array_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("indexOf", 1, 1, zr_container_array_index_of, "int", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_array_get_iterator, "zr.builtin.IEnumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
static const ZrLibMetaMethodDescriptor kArrayMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_array_constructor, "Array<T>", ZR_NULL, kArrayIndexParameter, 1},
        {ZR_META_GET_ITEM, 1, 1, zr_container_array_get_item, "T", ZR_NULL, kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)},
        {ZR_META_SET_ITEM, 2, 2, zr_container_array_set_item, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

static const ZrLibFieldDescriptor kMapFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
static const ZrLibMethodDescriptor kMapMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("containsKey", 1, 1, zr_container_map_contains_key, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_map_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_map_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_map_get_iterator, "zr.builtin.IEnumerator<Pair<K,V>>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
static const ZrLibMetaMethodDescriptor kMapMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_map_constructor, "Map<K,V>", ZR_NULL, ZR_NULL, 0},
        {.metaType = ZR_META_GET_ITEM,
         .minArgumentCount = 1,
         .maxArgumentCount = 1,
         .callback = zr_container_map_get_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapKeyParameter,
         .parameterCount = ZR_ARRAY_COUNT(kMapKeyParameter),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
         .readonlyInlineGetFastCallback = zr_container_map_get_item_readonly_inline_fast},
        {.metaType = ZR_META_SET_ITEM,
         .minArgumentCount = 2,
         .maxArgumentCount = 2,
         .callback = zr_container_map_set_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapSetItemParameters,
         .parameterCount = ZR_ARRAY_COUNT(kMapSetItemParameters),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL,
         .readonlyInlineSetNoResultFastCallback = zr_container_map_set_item_readonly_inline_no_result_fast},
};

static const ZrLibFieldDescriptor kSetFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
static const ZrLibMethodDescriptor kSetMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_set_add, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_set_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_set_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_set_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_set_get_iterator, "zr.builtin.IEnumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
static const ZrLibMetaMethodDescriptor kSetMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_set_constructor, "Set<T>", ZR_NULL, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor kPairFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "K", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("second", "V", ZR_NULL),
};
static const ZrLibMethodDescriptor kPairMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, zr_container_pair_equals, "bool", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, zr_container_pair_compare, "int", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, zr_container_pair_hash_code, "int", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
};
static const ZrLibMetaMethodDescriptor kPairMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 2, zr_container_pair_constructor, "Pair<K,V>", ZR_NULL, kPairParameters, ZR_ARRAY_COUNT(kPairParameters)},
        {ZR_META_COMPARE, 1, 1, zr_container_pair_compare, "int", ZR_NULL, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
};

static const ZrLibFieldDescriptor kLinkedListFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("last", "LinkedNode<T>", ZR_NULL),
};
static const ZrLibMethodDescriptor kLinkedListMethods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addFirst", 1, 1, zr_container_linked_list_add_first, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addLast", 1, 1, zr_container_linked_list_add_last, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeFirst", 0, 0, zr_container_linked_list_remove_first, "T", ZR_NULL,
                                      ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeLast", 0, 0, zr_container_linked_list_remove_last, "T", ZR_NULL, ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_linked_list_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_linked_list_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_linked_list_get_iterator, "zr.builtin.IEnumerator<T>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
static const ZrLibMetaMethodDescriptor kLinkedListMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_linked_list_constructor, "LinkedList<T>", ZR_NULL, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor kLinkedNodeFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("value", "T", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("next", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("previous", "LinkedNode<T>", ZR_NULL),
};
static const ZrLibMetaMethodDescriptor kLinkedNodeMetaMethods[] = {
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_linked_node_constructor, "LinkedNode<T>", ZR_NULL, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
};

static const ZrLibTypeDescriptor g_container_types[] = {
        {"Array", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kArrayFields, ZR_ARRAY_COUNT(kArrayFields), kArrayMethods, ZR_ARRAY_COUNT(kArrayMethods), kArrayMetaMethods, ZR_ARRAY_COUNT(kArrayMetaMethods), ZR_NULL, ZR_NULL, kArrayImplements, ZR_ARRAY_COUNT(kArrayImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Array<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        {"Map", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kMapFields, ZR_ARRAY_COUNT(kMapFields), kMapMethods, ZR_ARRAY_COUNT(kMapMethods), kMapMetaMethods, ZR_ARRAY_COUNT(kMapMetaMethods), ZR_NULL, ZR_NULL, kMapImplements, ZR_ARRAY_COUNT(kMapImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Map<K,V>()", kMapGenericParameters, ZR_ARRAY_COUNT(kMapGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        {"Set", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kSetFields, ZR_ARRAY_COUNT(kSetFields), kSetMethods, ZR_ARRAY_COUNT(kSetMethods), kSetMetaMethods, ZR_ARRAY_COUNT(kSetMetaMethods), ZR_NULL, ZR_NULL, kSetImplements, ZR_ARRAY_COUNT(kSetImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Set<T>()", kSetGenericParameters, ZR_ARRAY_COUNT(kSetGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        {"Pair", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kPairFields, ZR_ARRAY_COUNT(kPairFields), kPairMethods, ZR_ARRAY_COUNT(kPairMethods), kPairMetaMethods, ZR_ARRAY_COUNT(kPairMetaMethods), ZR_NULL, ZR_NULL, kPairImplements, ZR_ARRAY_COUNT(kPairImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Pair<K,V>(first: K, second: V)", kGenericKV, ZR_ARRAY_COUNT(kGenericKV), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)},
        {"LinkedList", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedListFields, ZR_ARRAY_COUNT(kLinkedListFields), kLinkedListMethods, ZR_ARRAY_COUNT(kLinkedListMethods), kLinkedListMetaMethods, ZR_ARRAY_COUNT(kLinkedListMetaMethods), ZR_NULL, ZR_NULL, kLinkedListImplements, ZR_ARRAY_COUNT(kLinkedListImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedList<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        {"LinkedNode", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedNodeFields, ZR_ARRAY_COUNT(kLinkedNodeFields), ZR_NULL, 0, kLinkedNodeMetaMethods, ZR_ARRAY_COUNT(kLinkedNodeMetaMethods), ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedNode<T>(value: T)", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), 0},
};

static const ZrLibModuleDescriptor g_container_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.container",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_container_types,
        .typeCount = ZR_ARRAY_COUNT(g_container_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Built-in generic container and interface module.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = ZR_NULL,
        .minRuntimeAbi = 0,
        .requiredCapabilities = 0,
};

const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void) {
    return &g_container_module_descriptor;
}

static TZrBool zr_container_install_basic_array_method(SZrState *state,
                                                       const TZrChar *methodName,
                                                       FZrNativeFunction nativeFunction) {
    SZrObjectPrototype *prototype;
    SZrObject *prototypeObject;
    SZrClosureNative *closure;
    SZrTypeValue closureValue;

    if (state == ZR_NULL || state->global == ZR_NULL || methodName == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    prototypeObject = &prototype->super;
    if (ZrLib_Object_GetFieldCString(state, prototypeObject, methodName) != ZR_NULL) {
        return ZR_TRUE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = nativeFunction;
    ZrCore_Value_InitAsRawObject(state, &closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue.isNative = ZR_TRUE;
    ZrLib_Object_SetFieldCString(state, prototypeObject, methodName, &closureValue);
    return ZR_TRUE;
}

static TZrBool zr_container_install_basic_array_adapter(SZrGlobalState *global) {
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_install_basic_array_method(global->mainThreadState,
                                                   "getIterator",
                                                   zr_container_native_array_get_iterator_native);
}

TZrBool ZrVmLibContainer_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_container_module_descriptor) &&
           zr_container_install_basic_array_adapter(global);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibContainer_GetModuleDescriptor();
}
#endif
