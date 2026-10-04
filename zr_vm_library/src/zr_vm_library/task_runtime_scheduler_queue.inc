#include "zr_vm_core/gc.h"

/* Existing private fields are looked up without allocating keys or moving GC
 * objects. Their pair addresses are borrowed only until the next allocation. */
/**
 * @brief 在已有 scheduler 哈希字段中按字符串字节查找 pair，避免为私有队列状态创建新 key。
 * @pre name 为非空 C 字符串；结果是借用地址，分配后必须重新查找，不能作为长期字段句柄。
 */
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
/**
 * @brief 经核心 bool member setter 更新已有 queue/head 字段，再读回确认值及 threadStatus。
 * @note 仅接受无 ownership 的 normalized null/array/int64；false 不保证字段从未写入，
 * replacement attach 的失败清理由调用方处理。这里不创建字段，也不绕过版本、缓存或 barrier。
 */
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

/**
 * @brief 只在字段仍指向给定耗尽数组时将 queue 置 null；无论成功与否都保留旧 head。
 * @note 新数组建立路径先确认 head 重置，防止旧数组仍挂载时从零重放已完成 Task。
 */
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

/** @brief TryRun 栈上建队请求；scheduler 在外层被 pin，queue/attached 由同步回调回传。 */
typedef struct SZrTaskSchedulerQueueCreate {
    SZrObject *scheduler;
    SZrObject *queue;
    TZrBool attached;
} SZrTaskSchedulerQueueCreate;

/**
 * @brief 在外层 pin 与 TryRun 保护期间分配 replacement queue，分配后重新取得 queue/head pair。
 * @note 仅在旧 queue 为 null 且 head 为零时尝试挂载；attached 是更新检查的结果，不是分配成功标志。
 */
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

/**
 * @brief 返回已有合法 queue，或在 queue=null 时先确认 head 归零再受保护地创建 replacement。
 * @note 成功 pin 后由 TryRun 捕获创建错误，清理本次部分挂载的 replacement 并统一 unpin。
 * 返回 NULL 包含字段损坏、reset/pin/attach 拒绝和捕获错误；不承诺恢复此前已耗尽的队列。
 */
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
