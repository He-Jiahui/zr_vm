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
    object->cachedLengthPair = ZR_NULL;
    object->cachedCapacityPair = ZR_NULL;
}

#endif
