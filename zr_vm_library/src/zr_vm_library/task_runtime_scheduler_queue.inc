#include "zr_vm_core/gc.h"

/* Existing private fields are looked up without allocating keys or moving GC
 * objects. Their pair addresses are borrowed only until the next allocation. */
static SZrHashKeyValuePair *task_runtime_scheduler_pair(SZrObject *scheduler,
                                                       const TZrChar *name) {
    TZrSize bucket;
    TZrSize length = strlen(name);

    if (scheduler == ZR_NULL || scheduler->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT ||
        !scheduler->nodeMap.isValid || scheduler->nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }
    for (bucket = 0; bucket < scheduler->nodeMap.capacity; ++bucket) {
        SZrHashKeyValuePair *pair;
        for (pair = scheduler->nodeMap.buckets[bucket]; pair != ZR_NULL; pair = pair->next) {
            const SZrString *key;
            if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
                continue;
            }
            key = (const SZrString *)pair->key.value.object;
            if (ZrCore_String_GetByteLength(key) == length &&
                memcmp(ZrCore_String_GetNativeString(key), name, length) == 0) {
                return pair;
            }
        }
    }
    return ZR_NULL;
}

/* The core member setter maintains memberVersion, caches and the GC barrier.
 * Read back this existing pair so a rejected write cannot look successful. */
static TZrBool task_runtime_scheduler_update(SZrState *state, SZrObject *scheduler,
                                             SZrHashKeyValuePair *pair,
                                             const SZrTypeValue *value) {
    SZrTypeValue receiver;
    if (state == ZR_NULL || scheduler == ZR_NULL || pair == ZR_NULL || value == ZR_NULL ||
        (pair->value.type != ZR_VALUE_TYPE_NULL && pair->value.type != ZR_VALUE_TYPE_ARRAY &&
         pair->value.type != ZR_VALUE_TYPE_INT64) ||
        (value->type != ZR_VALUE_TYPE_NULL && value->type != ZR_VALUE_TYPE_ARRAY &&
         value->type != ZR_VALUE_TYPE_INT64) ||
        !ZrCore_Value_HasNormalizedNoOwnership(&pair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(value)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(state, &receiver, scheduler, ZR_VALUE_TYPE_OBJECT);
    if (!ZrCore_Object_SetMember(state, &receiver, (SZrString *)pair->key.value.object, value) ||
        state->threadStatus != ZR_THREAD_STATUS_FINE || pair->value.type != value->type) {
        return ZR_FALSE;
    }
    if (value->type == ZR_VALUE_TYPE_NULL) return ZR_TRUE;
    if (value->type == ZR_VALUE_TYPE_INT64) {
        return pair->value.value.nativeObject.nativeInt64 == value->value.nativeObject.nativeInt64;
    }
    return value->type == ZR_VALUE_TYPE_ARRAY && pair->value.value.object == value->value.object;
}

static TZrBool task_runtime_scheduler_detach_queue(SZrState *state, SZrObject *scheduler,
                                                  SZrObject *queue) {
    SZrHashKeyValuePair *pair = task_runtime_scheduler_pair(scheduler, kTaskQueueField);
    SZrTypeValue empty;
    if (pair == ZR_NULL || pair->value.type != ZR_VALUE_TYPE_ARRAY ||
        pair->value.value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(queue)) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNull(&empty);
    /* Leave head unchanged even when detaching fails. The next empty-queue
     * creation must reset it before publishing the replacement queue. */
    return task_runtime_scheduler_update(state, scheduler, pair, &empty);
}

typedef struct SZrTaskSchedulerQueueCreate {
    SZrObject *scheduler;
    SZrObject *queue;
    TZrBool attached;
} SZrTaskSchedulerQueueCreate;

static void task_runtime_scheduler_create_queue(SZrState *state, TZrPtr argument) {
    SZrTaskSchedulerQueueCreate *request = (SZrTaskSchedulerQueueCreate *)argument;
    SZrHashKeyValuePair *queuePair;
    SZrHashKeyValuePair *headPair;
    SZrTypeValue value;
    request->queue = ZrLib_Array_New(state);
    if (request->queue == ZR_NULL) return;
    /* Reacquire pairs after allocation, while the scheduler remains pinned. */
    queuePair = task_runtime_scheduler_pair(request->scheduler, kTaskQueueField);
    headPair = task_runtime_scheduler_pair(request->scheduler, kTaskQueueHeadField);
    if (queuePair == ZR_NULL || queuePair->value.type != ZR_VALUE_TYPE_NULL ||
        headPair == ZR_NULL || headPair->value.type != ZR_VALUE_TYPE_INT64 ||
        headPair->value.value.nativeObject.nativeInt64 != 0) return;
    ZrLib_Value_SetObject(state, &value, request->queue, ZR_VALUE_TYPE_ARRAY);
    request->attached = task_runtime_scheduler_update(state, request->scheduler, queuePair, &value);
}

static SZrObject *task_runtime_scheduler_queue(SZrState *state, SZrObject *scheduler) {
    SZrHashKeyValuePair *queuePair = task_runtime_scheduler_pair(scheduler, kTaskQueueField);
    SZrHashKeyValuePair *headPair = task_runtime_scheduler_pair(scheduler, kTaskQueueHeadField);
    SZrObject *queue = ZR_NULL;
    SZrTypeValue value;
    SZrGcNativeCallPin pin;
    SZrTaskSchedulerQueueCreate request;
    EZrThreadStatus status;

    if (state == ZR_NULL || queuePair == ZR_NULL || headPair == ZR_NULL ||
        !ZrCore_Value_HasNormalizedNoOwnership(&queuePair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(&headPair->value) ||
        headPair->value.type != ZR_VALUE_TYPE_INT64) {
        return ZR_NULL;
    }
    if (queuePair->value.type == ZR_VALUE_TYPE_ARRAY && queuePair->value.value.object != ZR_NULL) {
        queue = ZR_CAST_OBJECT(state, queuePair->value.value.object);
        return queue->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? queue : ZR_NULL;
    }
    if (queuePair->value.type != ZR_VALUE_TYPE_NULL) return ZR_NULL;
    ZrLib_Value_SetInt(state, &value, 0);
    if (!task_runtime_scheduler_update(state, scheduler, headPair, &value) ||
        !ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(scheduler), &pin)) {
        return ZR_NULL;
    }
    memset(&request, 0, sizeof(request));
    request.scheduler = scheduler;
    status = ZrCore_Exception_TryRun(state, task_runtime_scheduler_create_queue, &request);
    if (status == ZR_THREAD_STATUS_FINE && request.attached) {
        queue = request.queue;
    } else {
        /* A barrier failure may happen after writing the value. Remove only
         * this unpublished replacement; scalar rollback cannot allocate. */
        queuePair = task_runtime_scheduler_pair(scheduler, kTaskQueueField);
        if (queuePair != ZR_NULL && queuePair->value.type == ZR_VALUE_TYPE_ARRAY &&
            queuePair->value.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(request.queue)) {
            ZrLib_Value_SetNull(&value);
            (void)task_runtime_scheduler_update(state, scheduler, queuePair, &value);
        }
    }
    ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    return queue;
}
