#ifndef ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H
#define ZR_VM_CORE_OBJECT_SUPER_ARRAY_INTERNAL_H

#include "zr_vm_core/debug.h"
#include "object_internal.h"
#include "zr_vm_core/stack.h"

#define ZR_OBJECT_HIDDEN_ITEMS_FIELD "__zr_items"
#define ZR_OBJECT_ARRAY_LENGTH_FIELD "length"
#define ZR_OBJECT_ARRAY_CAPACITY_FIELD "capacity"
#define ZR_OBJECT_ARRAY_ADD_MEMBER "add"
#define ZR_OBJECT_LITERAL_CACHE_CAPACITY 8U
#define ZR_OBJECT_SUPER_ARRAY_INITIAL_CAPACITY 4U
#define ZR_OBJECT_SUPER_ARRAY_GROWTH_FACTOR 2U

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

static ZR_FORCE_INLINE void zr_super_array_store_plain_int_reuse(SZrTypeValue *destination, TZrInt64 value) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(destination));
    ZR_VALUE_FAST_SET(destination, nativeInt64, value, ZR_VALUE_TYPE_INT64);
}

static ZR_FORCE_INLINE void zr_super_array_store_plain_null_reuse(SZrTypeValue *destination) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(destination));
    ZR_VALUE_FAST_SET(destination, nativeUInt64, 0, ZR_VALUE_TYPE_NULL);
}

static ZR_FORCE_INLINE void zr_super_array_assign_int_or_copy(SZrState *state,
                                                              SZrTypeValue *destination,
                                                              TZrInt64 value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);

    if (ZR_LIKELY(destination->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE)) {
        ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);
        ZR_VALUE_FAST_SET(destination, nativeInt64, value, ZR_VALUE_TYPE_INT64);
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
        ZR_VALUE_FAST_SET(destination, nativeUInt64, 0, ZR_VALUE_TYPE_NULL);
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

static ZR_FORCE_INLINE TZrBool zr_super_array_resolve_items_cached_assume_fast(SZrState *state,
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
    if (ZR_LIKELY(itemsObject != ZR_NULL)) {
        if (outReceiverObject != ZR_NULL) {
            *outReceiverObject = receiverObject;
        }
        *outItemsObject = itemsObject;
        return ZR_TRUE;
    }

    return ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(state, receiver, outReceiverObject, outItemsObject);
}

static ZR_FORCE_INLINE SZrObject *zr_super_array_resolve_items_object_only_cached_assume_fast(SZrState *state,
                                                                                               SZrTypeValue *receiver) {
    SZrObject *receiverObject;
    SZrObject *itemsObject;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY);

    receiverObject = ZR_CAST_OBJECT(state, receiver->value.object);
    ZR_ASSERT(receiverObject != ZR_NULL);

    itemsObject = zr_super_array_cached_items_object_assume_fast(receiverObject);
    if (ZR_LIKELY(itemsObject != ZR_NULL)) {
        return itemsObject;
    }

    return ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(state, receiver, ZR_NULL, &itemsObject) ? itemsObject
                                                                                                         : ZR_NULL;
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

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(receiver != ZR_NULL);
    ZR_ASSERT(result != ZR_NULL);

    itemsObject = zr_super_array_resolve_items_object_only_cached_assume_fast(state, receiver);
    if (itemsObject == ZR_NULL) {
        return ZR_FALSE;
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

    nodeMap = &itemsObject->nodeMap;
    if (ZR_UNLIKELY((TZrUInt64)indexValue >= (TZrUInt64)nodeMap->elementCount)) {
        ZrCore_Debug_RunError(state, "Array index out of range");
    }

    pair = nodeMap->buckets[(TZrSize)indexValue];
    ZR_ASSERT(pair != ZR_NULL);
    ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(pair->value.type));
    ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(&pair->value));
    zr_super_array_store_plain_int_reuse(&pair->value, value);
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

TZrBool ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(SZrState *state,
                                                       TZrStackValuePointer receiverBase,
                                                       TZrInt64 value);

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(SZrState *state,
                                                                    TZrStackValuePointer receiverBase,
                                                                    TZrInt64 repeatCount,
                                                                    TZrInt64 value);

#endif
