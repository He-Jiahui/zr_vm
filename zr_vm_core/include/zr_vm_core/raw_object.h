//
// Created by HeJiahui on 2025/8/7.
//

#ifndef ZR_VM_CORE_RAW_OBJECT_H
#define ZR_VM_CORE_RAW_OBJECT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrRawObject;
struct SZrOwnershipControl;

typedef void (*FRawObjectScanMarkGc)(struct SZrState *state, struct SZrRawObject *parentThis);

struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next;
    EZrRawObjectType type;
    TZrBool isNative;
    SZrGarbageCollectionObjectMark garbageCollectMark;
    struct SZrRawObject *gcList;
    FRawObjectScanMarkGc scanMarkGcFunction;
    struct SZrOwnershipControl *ownershipControl;
    // default hash value is the address of the object
    TZrUInt64 hash;
};

typedef struct SZrRawObject SZrRawObject;

ZR_FORCE_INLINE void ZrCore_RawObject_Construct(SZrRawObject *super, EZrRawObjectType type) {
    super->next = ZR_NULL;
    super->type = type;
    super->isNative = ZR_FALSE;
    super->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    super->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW;
    super->garbageCollectMark.generation = ZR_GARBAGE_COLLECT_GENERATION_INVALID;
    super->garbageCollectMark.heapGenerationKind = ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG;
    super->garbageCollectMark.regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_EDEN;
    super->garbageCollectMark.storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
    super->garbageCollectMark.regionId = 0;
    super->garbageCollectMark.survivalAge = 0;
    super->garbageCollectMark.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    super->garbageCollectMark.anchorScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
    super->garbageCollectMark.pinFlags = ZR_GARBAGE_COLLECT_PIN_KIND_NONE;
    super->garbageCollectMark.promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
    super->garbageCollectMark.forwardingAddress = ZR_NULL;
    super->garbageCollectMark.forwardingRefLocation = ZR_NULL;
    super->gcList = ZR_NULL;
    super->scanMarkGcFunction = ZR_NULL;
    super->ownershipControl = ZR_NULL;
    // because of the alignment, the hash value should be address / ZR_ALIGN_SIZE
    super->hash = ((TZrUInt64) &super) / ZR_ALIGN_SIZE;
}

ZR_FORCE_INLINE void ZrCore_RawObject_InitHash(SZrRawObject *super, TZrUInt64 hash) { super->hash = hash; }

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkInited(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkWaitToScan(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkReferenced(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}


#endif // ZR_VM_CORE_RAW_OBJECT_H
