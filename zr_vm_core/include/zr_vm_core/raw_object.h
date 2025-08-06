//
// Created by HeJiahui on 2025/8/7.
//

#ifndef ZR_VM_CORE_RAW_OBJECT_H
#define ZR_VM_CORE_RAW_OBJECT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrRawObject;

typedef void (*FRawObjectScanMarkGc)(struct SZrState *state, struct SZrRawObject *parentThis);

struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next;
    EZrRawObjectType type;
    TBool isNative;
    SZrGarbageCollectionObjectMark garbageCollectMark;
    struct SZrRawObject *gcList;
    FRawObjectScanMarkGc scanMarkGcFunction;
    // default hash value is the address of the object
    TUInt64 hash;
};

typedef struct SZrRawObject SZrRawObject;

ZR_FORCE_INLINE void ZrRawObjectConstruct(SZrRawObject *super, EZrRawObjectType type) {
    super->next = ZR_NULL;
    super->type = type;
    super->isNative = ZR_FALSE;
    super->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    super->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW;
    super->garbageCollectMark.generation = ZR_GARBAGE_COLLECT_GENERATION_INVALID;
    super->gcList = ZR_NULL;
    super->scanMarkGcFunction = ZR_NULL;
    // because of the alignment, the hash value should be address / ZR_ALIGN_SIZE
    super->hash = ((TUInt64) &super) / ZR_ALIGN_SIZE;
}

ZR_FORCE_INLINE void ZrRawObjectInitHash(SZrRawObject *super, TUInt64 hash) { super->hash = hash; }

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkInited(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkWaitToScan(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkReferenced(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}


#endif // ZR_VM_CORE_RAW_OBJECT_H
