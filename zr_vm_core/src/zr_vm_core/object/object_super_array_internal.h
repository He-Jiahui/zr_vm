#ifndef ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H
#define ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H

#include "zr_vm_core/debug.h"
#include "zr_vm_core/memory.h"
#include "object/object_internal.h"
#include "zr_vm_core/stack.h"

#define ZR_OBJECT_HIDDEN_ITEMS_FIELD "__zr_items"
#define ZR_OBJECT_ARRAY_LENGTH_FIELD "length"
#define ZR_OBJECT_ARRAY_CAPACITY_FIELD "capacity"
#define ZR_OBJECT_ARRAY_ADD_MEMBER "add"
#define ZR_OBJECT_LITERAL_CACHE_CAPACITY 8U
#define ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY 4U
#define ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR 2U
#define ZR_OBJECT_SUPER_ARRAY_EXACT_PAIR_SPAN_MIN_COUNT 8U

SZrString *ZrCore_Object_CachedKnownFieldString(SZrState *state, const TZrChar *literal);

TZrBool ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(SZrState *state,
                                                           SZrTypeValue *receiver,
                                                           SZrObject **outReceiverObject,
                                                           SZrObject **outItemsObject);

static ZR_FORCE_INLINE TZrBool zr_super_array_value_can_overwrite_without_release(const SZrTypeValue *value) {
    ZR_ASSERT(value != ZR_NULL);
    return ZrCore_Value_HasNormalizedNoOwnership(value);
}

static ZR_FORCE_INLINE SZrObject *zr_super_array_cached_items_object_assume_fast(SZrObject *receiverObject) {
    SZrObject *itemsObject;

    ZR_ASSERT(receiverObject != ZR_NULL);
    itemsObject = receiverObject->cachedHiddenItemsObject;
    if (ZR_LIKELY(itemsObject != ZR_NULL)) {
        ZR_ASSERT(itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    }
    return itemsObject;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_try_resolve_items_cached_only_assume_fast(SZrState *state,
                                                                                         SZrTypeValue *receiver,
                                                                                         SZrObject **outReceiverObject,
                                                                                         SZrObject **outItemsObject) {
    SZrObject *receiverObject;
    SZrObject *itemsObject;

    if (outReceiverObject != ZR_NULL) {
        *outReceiverObject = ZR_NULL;
    }
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(outItemsObject != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    receiverObject = ZR_CAST_OBJECT(state, receiver->value.object);
    ZR_ASSERT(receiverObject != ZR_NULL);

    itemsObject = zr_super_array_cached_items_object_assume_fast(receiverObject);
    if (ZR_UNLIKELY(itemsObject == ZR_NULL)) {
        return ZR_FALSE;
    }

    if (outReceiverObject != ZR_NULL) {
        *outReceiverObject = receiverObject;
    }
    *outItemsObject = itemsObject;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_items_object_from_value_assume_fast(SZrState *state,
                                                                                  const SZrTypeValue *itemsValue,
                                                                                  SZrObject **outItemsObject) {
    SZrObject *itemsObject;

    if (outItemsObject != ZR_NULL) {
        *outItemsObject = ZR_NULL;
    }
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(itemsValue != ZR_NULL);
    ZR_ASSERT(outItemsObject != ZR_NULL);

    if (ZR_UNLIKELY(itemsValue->type != ZR_VALUE_TYPE_OBJECT && itemsValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    itemsObject = ZR_CAST_OBJECT(state, itemsValue->value.object);
    if (ZR_UNLIKELY(itemsObject == ZR_NULL || itemsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    *outItemsObject = itemsObject;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE SZrObject *zr_super_array_bound_items_object_from_value_assume_fast(
        const SZrTypeValue *itemsValue) {
    SZrObject *itemsObject;

    ZR_ASSERT(itemsValue != ZR_NULL);
    ZR_ASSERT(itemsValue->type == ZR_VALUE_TYPE_OBJECT || itemsValue->type == ZR_VALUE_TYPE_ARRAY);
    ZR_ASSERT(itemsValue->value.object != ZR_NULL);

    itemsObject = ZR_CAST(SZrObject *, itemsValue->value.object);
    ZR_ASSERT(itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    return itemsObject;
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_int_reuse(SZrTypeValue *destination, TZrInt64 value) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(destination));
    destination->type = ZR_VALUE_TYPE_INT64;
    destination->value.nativeObject.nativeInt64 = value;
    destination->isGarbageCollectable = ZR_FALSE;
    destination->isNative = ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_value_is_normalized_plain(const SZrTypeValue *value) {
    ZR_ASSERT(value != ZR_NULL);
    return !value->isGarbageCollectable && value->isNative && zr_super_array_value_can_overwrite_without_release(value);
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_int_assume_normalized(SZrTypeValue *destination,
                                                                             TZrInt64 value) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(destination));
    destination->type = ZR_VALUE_TYPE_INT64;
    destination->value.nativeObject.nativeInt64 = value;
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_null_reuse(SZrTypeValue *destination) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(destination));
    destination->type = ZR_VALUE_TYPE_NULL;
    destination->value.nativeObject.nativeUInt64 = 0;
    destination->isGarbageCollectable = ZR_FALSE;
    destination->isNative = ZR_TRUE;
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_null_assume_normalized(SZrTypeValue *destination) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(destination));
    destination->type = ZR_VALUE_TYPE_NULL;
    destination->value.nativeObject.nativeUInt64 = 0;
}

static ZR_FORCE_INLINE void zr_super_array_assign_int_or_copy(SZrState *state,
                                                              SZrTypeValue *destination,
                                                              TZrInt64 value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);

    if (ZR_LIKELY(destination->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE)) {
        ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);
        zr_super_array_store_plain_int_reuse(destination, value);
        return;
    }

    {
        SZrTypeValue tempValue;
        ZR_VALUE_FAST_SET(&tempValue, nativeInt64, value, ZR_VALUE_TYPE_INT64);
        ZrCore_Value_CopyNoProfile(state, destination, &tempValue);
    }
}

static ZR_FORCE_INLINE void zr_super_array_assign_null_or_copy(SZrState *state, SZrTypeValue *destination) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);

    if (ZR_LIKELY(destination->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE)) {
        ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);
        zr_super_array_store_plain_null_reuse(destination);
        return;
    }

    {
        SZrTypeValue tempValue;
        ZR_VALUE_FAST_SET(&tempValue, nativeUInt64, 0, ZR_VALUE_TYPE_NULL);
        ZrCore_Value_CopyNoProfile(state, destination, &tempValue);
    }
}

static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_super_array_dense_pair_at_assume_fast(SZrObject *itemsObject,
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

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_is_active(const SZrObject *itemsObject) {
    return itemsObject != ZR_NULL &&
           itemsObject->superArrayRawIntData != ZR_NULL &&
           itemsObject->superArrayRawIntLength <= itemsObject->superArrayRawIntCapacity;
}

static ZR_FORCE_INLINE TZrSize zr_super_array_raw_int_length_or_node_count(const SZrObject *itemsObject) {
    ZR_ASSERT(itemsObject != ZR_NULL);
    return zr_super_array_raw_int_is_active(itemsObject)
                   ? itemsObject->superArrayRawIntLength
                   : itemsObject->nodeMap.elementCount;
}

static ZR_FORCE_INLINE void zr_super_array_raw_int_release(SZrGlobalState *global, SZrObject *itemsObject) {
    if (itemsObject == ZR_NULL) {
        return;
    }
    if (itemsObject->superArrayRawIntData != ZR_NULL &&
        global != ZR_NULL &&
        itemsObject->superArrayRawIntCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      itemsObject->superArrayRawIntData,
                                      itemsObject->superArrayRawIntCapacity * sizeof(TZrInt64),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    itemsObject->superArrayRawIntData = ZR_NULL;
    itemsObject->superArrayRawIntLength = 0;
    itemsObject->superArrayRawIntCapacity = 0;
    itemsObject->superArrayRawIntDirty = ZR_FALSE;
}

static ZR_FORCE_INLINE void zr_super_array_raw_int_disable(SZrState *state, SZrObject *itemsObject) {
    zr_super_array_raw_int_release(state != ZR_NULL ? state->global : ZR_NULL, itemsObject);
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_ensure_capacity(SZrState *state,
                                                                       SZrObject *itemsObject,
                                                                       TZrSize requiredCapacity) {
    TZrSize newCapacity;
    TZrSize oldBytes;
    TZrSize newBytes;
    TZrInt64 *newData;

    ZR_ASSERT(itemsObject != ZR_NULL);

    if (requiredCapacity == 0) {
        return ZR_TRUE;
    }
    if (itemsObject->superArrayRawIntData != ZR_NULL &&
        itemsObject->superArrayRawIntCapacity >= requiredCapacity) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL ||
        requiredCapacity > (ZR_MAX_SIZE / sizeof(TZrInt64))) {
        return ZR_FALSE;
    }

    newCapacity = itemsObject->superArrayRawIntCapacity != 0
            ? itemsObject->superArrayRawIntCapacity
            : ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY;
    while (newCapacity < requiredCapacity) {
        TZrSize grownCapacity;
        if (newCapacity > (ZR_MAX_SIZE / ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR)) {
            newCapacity = requiredCapacity;
            break;
        }
        grownCapacity = newCapacity * ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR;
        newCapacity = grownCapacity > newCapacity ? grownCapacity : requiredCapacity;
    }
    if (newCapacity > (ZR_MAX_SIZE / sizeof(TZrInt64))) {
        return ZR_FALSE;
    }

    newBytes = newCapacity * sizeof(TZrInt64);
    newData = (TZrInt64 *)ZrCore_Memory_RawMallocWithType(state->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newData == ZR_NULL) {
        return ZR_FALSE;
    }
    if (itemsObject->superArrayRawIntData != ZR_NULL && itemsObject->superArrayRawIntLength > 0) {
        ZrCore_Memory_RawCopy(newData,
                              itemsObject->superArrayRawIntData,
                              itemsObject->superArrayRawIntLength * sizeof(TZrInt64));
    }
    oldBytes = itemsObject->superArrayRawIntCapacity * sizeof(TZrInt64);
    if (itemsObject->superArrayRawIntData != ZR_NULL && oldBytes > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      itemsObject->superArrayRawIntData,
                                      oldBytes,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    itemsObject->superArrayRawIntData = newData;
    itemsObject->superArrayRawIntCapacity = newCapacity;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void zr_super_array_raw_int_append_range_optional(SZrState *state,
                                                                         SZrObject *itemsObject,
                                                                         TZrSize startIndex,
                                                                         TZrSize count,
                                                                         TZrInt64 value) {
    TZrSize index;
    TZrSize requiredLength;

    if (itemsObject == ZR_NULL || count == 0 ||
        startIndex > ZR_MAX_SIZE - count ||
        itemsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return;
    }

    requiredLength = startIndex + count;
    if (itemsObject->superArrayRawIntData != ZR_NULL &&
        startIndex != itemsObject->superArrayRawIntLength) {
        zr_super_array_raw_int_disable(state, itemsObject);
        return;
    }

    if (!zr_super_array_raw_int_ensure_capacity(state, itemsObject, requiredLength)) {
        zr_super_array_raw_int_disable(state, itemsObject);
        return;
    }

    for (index = startIndex; index < requiredLength; index++) {
        itemsObject->superArrayRawIntData[index] = value;
    }
    itemsObject->superArrayRawIntLength = requiredLength;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_try_load(const SZrObject *itemsObject,
                                                               TZrUInt64 unsignedIndex,
                                                               TZrInt64 *outValue) {
    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(outValue != ZR_NULL);

    if (!zr_super_array_raw_int_is_active(itemsObject) ||
        unsignedIndex >= (TZrUInt64)itemsObject->superArrayRawIntLength) {
        return ZR_FALSE;
    }

    *outValue = itemsObject->superArrayRawIntData[(TZrSize)unsignedIndex];
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_try_load_bound_items_assume_fast(
        const SZrObject *itemsObject,
        TZrUInt64 unsignedIndex,
        TZrInt64 *outValue) {
    const TZrInt64 *data;
    TZrSize length;

    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    ZR_ASSERT(outValue != ZR_NULL);

    data = itemsObject->superArrayRawIntData;
    length = itemsObject->superArrayRawIntLength;
    if (ZR_LIKELY(data != ZR_NULL && unsignedIndex < (TZrUInt64)length)) {
        ZR_ASSERT(length <= itemsObject->superArrayRawIntCapacity);
        *outValue = data[(TZrSize)unsignedIndex];
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE void zr_super_array_raw_int_store_existing_optional(SZrObject *itemsObject,
                                                                           TZrInt64 indexValue,
                                                                           TZrInt64 value) {
    if (!zr_super_array_raw_int_is_active(itemsObject) ||
        indexValue < 0 ||
        (TZrUInt64)indexValue >= (TZrUInt64)itemsObject->superArrayRawIntLength) {
        return;
    }

    itemsObject->superArrayRawIntData[(TZrSize)indexValue] = value;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_store_existing_dirty_optional(SZrObject *itemsObject,
                                                                                    TZrInt64 indexValue,
                                                                                    TZrInt64 value) {
    if (!zr_super_array_raw_int_is_active(itemsObject) ||
        indexValue < 0 ||
        (TZrUInt64)indexValue >= (TZrUInt64)itemsObject->superArrayRawIntLength) {
        return ZR_FALSE;
    }

    itemsObject->superArrayRawIntData[(TZrSize)indexValue] = value;
    itemsObject->superArrayRawIntDirty = ZR_TRUE;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_store_existing_dirty_bound_items_assume_fast(
        SZrObject *itemsObject,
        TZrInt64 indexValue,
        TZrInt64 value) {
    TZrInt64 *data;
    TZrSize length;

    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY);

    data = itemsObject->superArrayRawIntData;
    length = itemsObject->superArrayRawIntLength;
    if (ZR_LIKELY(data != ZR_NULL &&
                  indexValue >= 0 &&
                  (TZrUInt64)indexValue < (TZrUInt64)length)) {
        ZR_ASSERT(length <= itemsObject->superArrayRawIntCapacity);
        data[(TZrSize)indexValue] = value;
        itemsObject->superArrayRawIntDirty = ZR_TRUE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_raw_int_materialize_dirty(SZrState *state,
                                                                         SZrObject *itemsObject) {
    TZrSize index;

    if (!zr_super_array_raw_int_is_active(itemsObject) || !itemsObject->superArrayRawIntDirty) {
        return ZR_TRUE;
    }
    if (!object_node_map_is_ready(itemsObject) ||
        itemsObject->superArrayRawIntLength != itemsObject->nodeMap.elementCount) {
        ZrCore_Debug_RunError(state, "Array raw int storage out of sync");
        return ZR_FALSE;
    }

    for (index = 0; index < itemsObject->superArrayRawIntLength; index++) {
        SZrHashKeyValuePair *pair = zr_super_array_dense_pair_at_assume_fast(itemsObject, (TZrInt64)index);
        if (pair == ZR_NULL) {
            ZrCore_Debug_RunError(state, "Array raw int storage missing dense pair");
            return ZR_FALSE;
        }
        if (zr_super_array_value_is_normalized_plain(&pair->value)) {
            zr_super_array_store_plain_int_assume_normalized(&pair->value,
                                                             itemsObject->superArrayRawIntData[index]);
        } else {
            zr_super_array_store_plain_int_reuse(&pair->value, itemsObject->superArrayRawIntData[index]);
        }
    }
    itemsObject->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool zr_super_array_resolve_items_cached_assume_fast(SZrState *state,
                                                                                SZrTypeValue *receiver,
                                                                                SZrObject **outReceiverObject,
                                                                                SZrObject **outItemsObject) {
    if (ZR_LIKELY(zr_super_array_try_resolve_items_cached_only_assume_fast(
                state, receiver, outReceiverObject, outItemsObject))) {
        return ZR_TRUE;
    }

    return ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(state, receiver, outReceiverObject, outItemsObject);
}

static ZR_FORCE_INLINE SZrObject *zr_super_array_resolve_items_object_only_cached_assume_fast(SZrState *state,
                                                                                                SZrTypeValue *receiver) {
    SZrObject *itemsObject;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    if (ZR_LIKELY(zr_super_array_try_resolve_items_cached_only_assume_fast(state, receiver, ZR_NULL, &itemsObject))) {
        return itemsObject;
    }

    return ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(state, receiver, ZR_NULL, &itemsObject) ? itemsObject
                                                                                                         : ZR_NULL;
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_get_from_items_object_assume_fast(
        const SZrObject *itemsObject,
        TZrUInt64 unsignedIndex,
        SZrTypeValue *result) {
    const SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrInt64 rawValue;

    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(result));

    if (ZR_LIKELY(zr_super_array_raw_int_try_load_bound_items_assume_fast(itemsObject, unsignedIndex, &rawValue))) {
        zr_super_array_store_plain_int_assume_normalized(result, rawValue);
        return;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY(unsignedIndex >= (TZrUInt64)nodeMap->elementCount)) {
        zr_super_array_store_plain_null_assume_normalized(result);
        return;
    }

    pair = nodeMap->buckets[(TZrSize)unsignedIndex];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    zr_super_array_store_plain_int_assume_normalized(result, pair->value.value.nativeObject.nativeInt64);
}

static ZR_FORCE_INLINE void zr_super_array_get_from_items_object_assume_fast(SZrState *state,
                                                                             const SZrObject *itemsObject,
                                                                             TZrInt64 indexValue,
                                                                             SZrTypeValue *result) {
    const SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrInt64 rawValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    if (ZR_LIKELY(zr_super_array_raw_int_try_load_bound_items_assume_fast(
                itemsObject, (TZrUInt64)indexValue, &rawValue))) {
        zr_super_array_assign_int_or_copy(state, result, rawValue);
        return;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        zr_super_array_assign_null_or_copy(state, result);
        return;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    zr_super_array_assign_int_or_copy(state, result, pair->value.value.nativeObject.nativeInt64);
}

static ZR_FORCE_INLINE TZrBool zr_super_array_set_int_in_items_object_assume_fast(SZrState *state,
                                                                                  SZrObject *itemsObject,
                                                                                  TZrInt64 indexValue,
                                                                                  TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(itemsObject != ZR_NULL);

    if (ZR_UNLIKELY(itemsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    if (ZR_LIKELY(zr_super_array_raw_int_store_existing_dirty_bound_items_assume_fast(
                itemsObject, indexValue, value))) {
        return ZR_TRUE;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        ZrCore_Debug_RunError(state, "Array index out of range");
        return ZR_TRUE;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(&pair->value));
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(&pair->value));
    zr_super_array_store_plain_int_assume_normalized(&pair->value, value);
    zr_super_array_raw_int_store_existing_optional(itemsObject, indexValue, value);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void zr_super_array_set_int_in_bound_items_object_assume_fast(SZrState *state,
                                                                                    SZrObject *itemsObject,
                                                                                    TZrInt64 indexValue,
                                                                                    TZrInt64 value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(itemsObject != ZR_NULL);
    ZR_ASSERT(itemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY);

    if (ZR_LIKELY(zr_super_array_raw_int_store_existing_dirty_bound_items_assume_fast(
                itemsObject, indexValue, value))) {
        return;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        ZrCore_Debug_RunError(state, "Array index out of range");
        return;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(&pair->value));
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(&pair->value));
    zr_super_array_store_plain_int_assume_normalized(&pair->value, value);
    zr_super_array_raw_int_store_existing_optional(itemsObject, indexValue, value);
}

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArrayGetIntInlineAssumeFast(SZrState *state,
                                                                               SZrTypeValue *receiver,
                                                                               const SZrTypeValue *key,
                                                                               SZrTypeValue *result);

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFast(SZrState *state,
                                                                                      SZrTypeValue *receiver,
                                                                                      TZrInt64 indexValue,
                                                                                      SZrTypeValue *result) {
    SZrObject *itemsObject;
    const SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrBool plainDestination;
    TZrInt64 rawValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    itemsObject = zr_super_array_resolve_items_object_only_cached_assume_fast(state, receiver);
    if (itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }

    plainDestination = (TZrBool)(result->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE);
    if (ZR_LIKELY(plainDestination)) {
        ZR_ASSERT(result->ownershipControl == ZR_NULL && result->ownershipWeakRef == ZR_NULL);
        if (ZR_LIKELY(zr_super_array_raw_int_try_load_bound_items_assume_fast(
                    itemsObject, (TZrUInt64)indexValue, &rawValue))) {
            if (zr_super_array_value_is_normalized_plain(result)) {
                zr_super_array_store_plain_int_assume_normalized(result, rawValue);
            } else {
                zr_super_array_store_plain_int_reuse(result, rawValue);
            }
            return ZR_TRUE;
        }

        nodeMap = &itemsObject->nodeMap;
        if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
            if (zr_super_array_value_is_normalized_plain(result)) {
                zr_super_array_store_plain_null_assume_normalized(result);
            } else {
                zr_super_array_store_plain_null_reuse(result);
            }
            return ZR_TRUE;
        }

        pair = nodeMap->buckets[(TZrSize)indexValue];
        ZR_ASSERT(pair != ZR_NULL);
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
        if (zr_super_array_value_is_normalized_plain(result)) {
            zr_super_array_store_plain_int_assume_normalized(result, pair->value.value.nativeObject.nativeInt64);
        } else {
            zr_super_array_store_plain_int_reuse(result, pair->value.value.nativeObject.nativeInt64);
        }
        return ZR_TRUE;
    }

    if (ZR_LIKELY(zr_super_array_raw_int_try_load_bound_items_assume_fast(
                itemsObject, (TZrUInt64)indexValue, &rawValue))) {
        zr_super_array_assign_int_or_copy(state, result, rawValue);
        return ZR_TRUE;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        zr_super_array_assign_null_or_copy(state, result);
        return ZR_TRUE;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    zr_super_array_assign_int_or_copy(state, result, pair->value.value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFastPlainDestination(
        SZrState *state,
        SZrTypeValue *receiver,
        TZrInt64 indexValue,
        SZrTypeValue *result) {
    SZrObject *itemsObject;
    TZrUInt64 unsignedIndex;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(result));
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(result));

    itemsObject = zr_super_array_resolve_items_object_only_cached_assume_fast(state, receiver);
    if (itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }

    unsignedIndex = (TZrUInt64)indexValue;
    zr_super_array_store_plain_get_from_items_object_assume_fast(itemsObject, unsignedIndex, result);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArraySetIntInlineAssumeFast(SZrState *state,
                                                                               SZrTypeValue *receiver,
                                                                               const SZrTypeValue *key,
                                                                               const SZrTypeValue *value);

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArraySetIntByValueInlineAssumeFast(SZrState *state,
                                                                                      SZrTypeValue *receiver,
                                                                                      TZrInt64 indexValue,
                                                                                      TZrInt64 value) {
    SZrObject *itemsObject;
    const SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);

    itemsObject = zr_super_array_resolve_items_object_only_cached_assume_fast(state, receiver);
    if (itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_LIKELY(zr_super_array_raw_int_store_existing_dirty_bound_items_assume_fast(
                itemsObject, indexValue, value))) {
        return ZR_TRUE;
    }

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        ZrCore_Debug_RunError(state, "Array index out of range");
        return ZR_TRUE;
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(&pair->value));
    ZR_ASSERT(zr_super_array_value_is_normalized_plain(&pair->value));
    zr_super_array_store_plain_int_assume_normalized(&pair->value, value);
    zr_super_array_raw_int_store_existing_optional(itemsObject, indexValue, value);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArrayGetIntInlineAssumeFast(SZrState *state,
                                                                               SZrTypeValue *receiver,
                                                                               const SZrTypeValue *key,
                                                                               SZrTypeValue *result) {
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(key->type));
    return ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFast(state,
                                                                 receiver,
                                                                 key->value.nativeObject.nativeInt64,
                                                                 result);
}

static ZR_FORCE_INLINE TZrBool ZrCore_Object_SuperArraySetIntInlineAssumeFast(SZrState *state,
                                                                               SZrTypeValue *receiver,
                                                                               const SZrTypeValue *key,
                                                                               const SZrTypeValue *value) {
    ZR_ASSERT(key != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(key->type));
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(value->type));
    return ZrCore_Object_SuperArraySetIntByValueInlineAssumeFast(state,
                                                                 receiver,
                                                                 key->value.nativeObject.nativeInt64,
                                                                 value->value.nativeObject.nativeInt64);
}

TZrBool ZrCore_Object_SuperArrayTryGetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              SZrTypeValue *result,
                                              TZrBool *outApplicable);

TZrBool ZrCore_Object_SuperArrayTrySetIntFast(SZrState *state,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *key,
                                              const SZrTypeValue *value,
                                              TZrBool *outApplicable);

TZrBool ZrCore_Object_SuperArrayGetIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *key,
                                                 SZrTypeValue *result);

TZrBool ZrCore_Object_SuperArraySetIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *key,
                                                 const SZrTypeValue *value);

TZrBool ZrCore_Object_SuperArrayAddIntAssumeFast(SZrState *state,
                                                 SZrTypeValue *receiver,
                                                 const SZrTypeValue *value,
                                                 SZrTypeValue *result);

TZrBool ZrCore_Object_SuperArrayAddIntDiscardResultAssumeFast(SZrState *state,
                                                              SZrTypeValue *receiver,
                                                              const SZrTypeValue *value);

TZrBool ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(SZrState *state,
                                                       TZrStackValuePointer receiverBase,
                                                       TZrInt64 value);

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(SZrState *state,
                                                                    TZrStackValuePointer receiverBase,
                                                                    TZrInt64 repeatCount,
                                                                    TZrInt64 value);

#endif
