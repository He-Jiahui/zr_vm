#ifndef ZR_VM_CORE_OBJECT_INTERNAL_H
#define ZR_VM_CORE_OBJECT_INTERNAL_H

#include "zr_vm_core/object.h"

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
}

TZrBool ZrCore_Object_GetMemberWithKeyUnchecked(SZrState *state,
                                                SZrTypeValue *receiver,
                                                struct SZrString *memberName,
                                                const SZrTypeValue *memberKey,
                                                SZrTypeValue *result);

TZrBool ZrCore_Object_TryGetMemberWithKeyFastUnchecked(SZrState *state,
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

TZrBool ZrCore_Object_TrySetMemberWithKeyFastUnchecked(SZrState *state,
                                                       SZrTypeValue *receiver,
                                                       struct SZrString *memberName,
                                                       const SZrTypeValue *memberKey,
                                                       const SZrTypeValue *value,
                                                       TZrBool *outHandled);

TZrBool ZrCore_Object_GetByIndexUnchecked(SZrState *state,
                                          SZrTypeValue *receiver,
                                          const SZrTypeValue *key,
                                          SZrTypeValue *result);

TZrBool ZrCore_Object_SetByIndexUnchecked(SZrState *state,
                                          SZrTypeValue *receiver,
                                          const SZrTypeValue *key,
                                          const SZrTypeValue *value);

#endif
