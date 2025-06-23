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


enum EZrGarbageCollectObjectStatus {
    ZR_GARBAGE_COLLECT_OBJECT_STATUS_PERMANENT,
    ZR_GARBAGE_COLLECT_OBJECT_STATUS_INITED,
    ZR_GARBAGE_COLLECT_OBJECT_STATUS_REFERENCED,
    ZR_GARBAGE_COLLECT_OBJECT_STATUS_UNREFERENCED,
    ZR_GARBAGE_COLLECT_OBJECT_STATUS_RELEASED,

    ZR_GARBAGE_COLLECT_OBJECT_STATUS_MAX
};

typedef enum EZrGarbageCollectObjectStatus EZrGarbageCollectObjectStatus;


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

struct SZrGarbageCollectionObjectMark {
    EZrGarbageCollectObjectStatus status;
    TUInt8 generations;
};

typedef struct SZrGarbageCollectionObjectMark SZrGarbageCollectionObjectMark;

struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next;
    EZrValueType type;
    SZrGarbageCollectionObjectMark garbageCollectMark;
};

typedef struct SZrRawObject SZrRawObject;

ZR_FORCE_INLINE void ZrRawObjectInit(SZrRawObject *super, EZrValueType type) {
    super->next = ZR_NULL;
    super->type = type;
    super->garbageCollectMark.status = ZR_GARBAGE_COLLECT_OBJECT_STATUS_INITED;
    super->garbageCollectMark.generations = 0;
}

struct ZR_STRUCT_ALIGN SZrHashRawObject {
    SZrRawObject super;
    TUInt64 hash;
    struct SZrHashRawObject *next;
};

typedef struct SZrHashRawObject SZrHashRawObject;
ZR_FORCE_INLINE void ZrHashRawObjectInit(SZrHashRawObject *super, EZrValueType type, TUInt64 hash) {
    ZrRawObjectInit(&super->super, type);
    super->hash = hash;
    super->next = ZR_NULL;
}

typedef struct SZrHashRawObject SZrHashRawObject;
#endif //ZR_OBJECT_CONF_H
