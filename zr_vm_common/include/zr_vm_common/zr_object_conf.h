//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_OBJECT_CONF_H
#define ZR_OBJECT_CONF_H

#include "zr_vm_common/zr_type_conf.h"

#define ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER 20
#define ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER 100
#define ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT 100
// how much to allocate before next GC step (log2)
#define ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE 17 /* 128KB */
// wait memory to 200% before starting new gc cycle
#define ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT 200

enum EZrGarbageCollectMode {
    ZR_GARBAGE_COLLECT_MODE_GENERATIONAL,
    ZR_GARBAGE_COLLECT_MODE_INCREMENTAL,
    ZR_GARBAGE_COLLECT_MODE_MAX
};

typedef enum EZrGarbageCollectMode EZrGarbageCollectMode;


enum EZrGarbageCollectIncrementalObjectStatus {
    // gc ignore
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT,
    // (white) still not scanned on this turn
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED,
    // (gray) object is referenced but its children remain unscanned
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN,
    // (black) scanned and marked as referenced
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED,
    // (white) not referenced and wait to be released
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED,
    // (white) mark destructed object as released
    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED,

    ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_MAX
};

typedef enum EZrGarbageCollectIncrementalObjectStatus EZrGarbageCollectIncrementalObjectStatus;

enum EZrGarbageCollectGenerationalObjectStatus {
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_ALIVE,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_LONG_ALIVE,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED,
    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SCANNED_PREVIOUS,

    ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_MAX
};

typedef enum EZrGarbageCollectGenerationalObjectStatus EZrGarbageCollectGenerationalObjectStatus;

enum EZrGarbageCollectStatus {
    ZR_GARBAGE_COLLECT_STATUS_RUNNING,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_USER,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF,
    ZR_GARBAGE_COLLECT_STATUS_STOP_BY_EXITING,
    ZR_GARBAGE_COLLECT_STATUS_MAX,
};

typedef enum EZrGarbageCollectStatus EZrGarbageCollectStatus;

enum EZrGarbageCollectRunningStatus {
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_END,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED,
    ZR_GARBAGE_COLLECT_RUNNING_STATUS_MAX
};

typedef enum EZrGarbageCollectRunningStatus EZrGarbageCollectRunningStatus;

enum EZrGarbageCollectGeneration {
    ZR_GARBAGE_COLLECT_GENERATION_INVALID,
    ZR_GARBAGE_COLLECT_GENERATION_A,
    ZR_GARBAGE_COLLECT_GENERATION_B,
};

typedef enum EZrGarbageCollectGeneration EZrGarbageCollectGeneration;

enum EZrObjectPrototypeType {
    ZR_OBJECT_PROTOTYPE_TYPE_INVALID,
    ZR_OBJECT_PROTOTYPE_TYPE_MODULE,
    ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
    ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
    ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
    ZR_OBJECT_PROTOTYPE_TYPE_ENUM,
    ZR_OBJECT_PROTOTYPE_TYPE_NATIVE,
    ZR_OBJECT_PROTOTYPE_TYPE_MAX
};

typedef enum EZrObjectPrototypeType EZrObjectPrototypeType;

struct SZrGarbageCollectionObjectMark {
    EZrGarbageCollectIncrementalObjectStatus status;
    EZrGarbageCollectGenerationalObjectStatus generationalStatus;
    EZrGarbageCollectGeneration generation;
};

typedef struct SZrGarbageCollectionObjectMark SZrGarbageCollectionObjectMark;

struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next;
    EZrValueType type;
    TBool isNative;
    SZrGarbageCollectionObjectMark garbageCollectMark;
    struct SZrRawObject *gcList;
};

typedef struct SZrRawObject SZrRawObject;

ZR_FORCE_INLINE void ZrRawObjectConstruct(SZrRawObject *super, EZrValueType type) {
    super->next = ZR_NULL;
    super->type = type;
    super->isNative = ZR_FALSE;
    super->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    super->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW;
    super->garbageCollectMark.generation = ZR_GARBAGE_COLLECT_GENERATION_INVALID;
    super->gcList = ZR_NULL;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkInited(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkWaitToScan(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsMarkReferenced(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

struct ZR_STRUCT_ALIGN SZrHashRawObject {
    SZrRawObject super;
    TUInt64 hash;
    struct SZrHashRawObject *next;
};

typedef struct SZrHashRawObject SZrHashRawObject;
ZR_FORCE_INLINE void ZrHashRawObjectInit(SZrHashRawObject *super, EZrValueType type, TUInt64 hash) {
    // we have already initialized super
    // ZrRawObjectConstruct(&super->super, type);
    super->hash = hash;
    super->next = ZR_NULL;
}

struct ZR_STRUCT_ALIGN SZrHashKeyValuePair {
    SZrHashRawObject super;
    SZrHashRawObject *key;
    SZrRawObject *value;
};

typedef struct SZrHashKeyValuePair SZrHashKeyValuePair;

ZR_FORCE_INLINE void ZrHashKeyValuePairInit(SZrHashKeyValuePair *pair, SZrHashRawObject *key, SZrRawObject *value) {
    ZrHashRawObjectInit(&pair->super, key->super.type, key->hash);
    pair->key = key;
    pair->value = value;
}
#endif // ZR_OBJECT_CONF_H
