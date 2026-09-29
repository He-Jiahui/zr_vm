#include "zr_vm_core/task_frame_runtime.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"

/** @brief 私有的池化 continuation 存储，其 layout 借用自所属 task。 */
typedef struct SZrCoreTaskFrame {
    struct SZrCoreTaskFrame *next;
    TZrUInt32 slotCapacity;
    TZrUInt32 stateId;
    const SZrCoreTaskFrameLayout *layout;
    SZrTypeValue *slots;
    TZrBool *initialized;
    SZrGcRootHandle *roots;
} SZrCoreTaskFrame;

/** @brief 识别清理时需要释放所有权包装的值。 */
static TZrBool task_frame_has_owned_value(const SZrTypeValue *value) {
    return value != ZR_NULL && value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE;
}

/** @brief 释放一次拥有所有权的值，再将其 slot 置为空值以便复用。 */
static void task_frame_release_value(SZrState *state, SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }
    if (task_frame_has_owned_value(value)) {
        ZrCore_Ownership_ReleaseValue(state, value);
    }
    ZrCore_Value_ResetAsNull(value);
}

/** @brief 所属 task 或 slot 结束使用时，释放其 GC root handle。 */
static void task_frame_release_root(SZrState *state, SZrGcRootHandle *root) {
    if (root != ZR_NULL) {
        ZrCore_GcRootHandle_Release(state, root);
    }
}

/** @brief 为非空 GC 对象建立 root；标量和空值不需要 handle。 */
static TZrBool task_frame_root_value(SZrState *state,
                                     const SZrTypeValue *value,
                                     SZrGcRootHandle *root) {
    if (root == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL || !value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrCore_GcRootHandle_Create(state, value->value.object, root);
}

/**
 * @brief 运行已配置的 drop 回调，再释放一个已初始化 slot 的值及 root。
 * @note 用户 drop 回调运行时，slot 值仍然有效。
 */
static void task_frame_cleanup_slot(SZrState *state,
                                    SZrCoreTaskFrame *frame,
                                    TZrUInt32 slotIndex) {
    const SZrCoreTaskFrameSlotLayout *slotLayout;

    if (frame == ZR_NULL || frame->layout == ZR_NULL ||
        slotIndex >= frame->layout->slotCount || !frame->initialized[slotIndex]) {
        return;
    }
    slotLayout = &frame->layout->slotLayouts[slotIndex];
    if (slotLayout->requiresDrop && slotLayout->drop != ZR_NULL) {
        slotLayout->drop(state, &frame->slots[slotIndex], slotLayout->dropUserData);
    }
    task_frame_release_value(state, &frame->slots[slotIndex]);
    if (slotLayout->isGcRoot) {
        ZrCore_GcRootHandle_Release(state, &frame->roots[slotIndex]);
    }
    frame->initialized[slotIndex] = ZR_FALSE;
}

/** @brief 归还或销毁 frame 前，逐个清理 slot。 */
static void task_frame_cleanup_slots(SZrState *state, SZrCoreTaskFrame *frame) {
    TZrUInt32 index;

    if (frame == ZR_NULL || frame->layout == ZR_NULL) {
        return;
    }
    for (index = 0U; index < frame->layout->slotCount; index++) {
        task_frame_cleanup_slot(state, frame, index);
    }
}

/** @brief 清理剩余存活的 slot，再释放 frame 的所有分配。 */
static void task_frame_destroy(SZrState *state, SZrCoreTaskFrame *frame) {
    if (frame == ZR_NULL) {
        return;
    }
    task_frame_cleanup_slots(state, frame);
    free(frame->roots);
    free(frame->initialized);
    free(frame->slots);
    free(frame);
}

/**
 * @brief 复用 slot 容量匹配的 frame，或为指定 layout 分配新 frame。
 * @return layout 无效或分配失败时返回空值；成功时借出一个 frame。
 */
static SZrCoreTaskFrame *task_frame_pool_take(SZrState *state,
                                               SZrCoreTaskFramePool *pool,
                                               const SZrCoreTaskFrameLayout *layout) {
    SZrCoreTaskFrame **link;
    SZrCoreTaskFrame *frame;
    TZrUInt32 index;

    ZR_UNUSED_PARAMETER(state);
    if (pool == ZR_NULL || layout == ZR_NULL ||
        (layout->slotCount != 0U && layout->slotLayouts == ZR_NULL)) {
        return ZR_NULL;
    }

    link = &pool->freeFrames;
    while (*link != ZR_NULL && (*link)->slotCapacity != layout->slotCount) {
        link = &(*link)->next;
    }
    if (*link != ZR_NULL) {
        frame = *link;
        *link = frame->next;
        frame->next = ZR_NULL;
        pool->pooledFrameCount--;
    } else {
        frame = (SZrCoreTaskFrame *)calloc(1U, sizeof(*frame));
        if (frame == ZR_NULL) {
            return ZR_NULL;
        }
        frame->slotCapacity = layout->slotCount;
        if (layout->slotCount != 0U) {
            frame->slots = (SZrTypeValue *)calloc(layout->slotCount, sizeof(*frame->slots));
            frame->initialized = (TZrBool *)calloc(layout->slotCount, sizeof(*frame->initialized));
            frame->roots = (SZrGcRootHandle *)calloc(layout->slotCount, sizeof(*frame->roots));
            if (frame->slots == ZR_NULL || frame->initialized == ZR_NULL || frame->roots == ZR_NULL) {
                task_frame_destroy(state, frame);
                return ZR_NULL;
            }
            for (index = 0U; index < layout->slotCount; index++) {
                ZrCore_Value_ResetAsNull(&frame->slots[index]);
            }
        }
        pool->frameAllocationCount++;
    }
    frame->layout = layout;
    frame->stateId = 0U;
    pool->activeFrameCount++;
    return frame;
}

/** @brief 清理借出的 frame，并将其放回仍有效的 pool 空闲链表。 */
static void task_frame_pool_return(SZrState *state,
                                   SZrCoreTaskFramePool *pool,
                                   SZrCoreTaskFrame *frame) {
    if (pool == ZR_NULL || frame == ZR_NULL) {
        return;
    }
    task_frame_cleanup_slots(state, frame);
    frame->layout = ZR_NULL;
    frame->stateId = 0U;
    frame->next = pool->freeFrames;
    pool->freeFrames = frame;
    if (pool->activeFrameCount > 0U) {
        pool->activeFrameCount--;
    }
    pool->pooledFrameCount++;
}

/** @brief 归还 task 的 continuation frame，并清空 task 中借用的 frame 指针。 */
static void task_frame_task_release_frame(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->frame == ZR_NULL) {
        return;
    }
    task_frame_pool_return(state, task->pool, task->frame);
    task->frame = ZR_NULL;
}

/** @brief 释放存活 frame 的 slot 前，调用一次 layout 清理回调。 */
static void task_frame_task_run_finally(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->finallyRan || task->layout == ZR_NULL) {
        return;
    }
    task->finallyRan = ZR_TRUE;
    if (task->layout->finally != ZR_NULL) {
        task->layout->finally(state, task, task->layout->finallyUserData);
    }
}

/**
 * @brief 保留并 root 最终值，运行 finally，再将 frame 归还 pool。
 * @return 无法保留或建立 materialized result 的 root 时返回 false。
 */
static TZrBool task_frame_task_complete(SZrState *state,
                                        SZrCoreTaskFrameTask *task,
                                        SZrTypeValue *result) {
    if (task == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    task_frame_release_root(state, &task->resultRoot);
    ZrCore_Value_AssignMaterializedStackValue(state, &task->result, result);
    if (!task_frame_root_value(state, &task->result, &task->resultRoot)) {
        task_frame_release_value(state, &task->result);
        /* TODO: 请明确此处失败后是否应设置故障状态或释放资源；调用方仍需 Free 该 task。 */
        return ZR_FALSE;
    }
    task_frame_task_run_finally(state, task);
    task_frame_task_release_frame(state, task);
    task->status = ZR_CORE_TASK_FRAME_STATUS_COMPLETED;
    task->resultConsumed = ZR_FALSE;
    return ZR_TRUE;
}

/**
 * @brief 运行一次 poll，并将结果转换为挂起或终态。
 * @note 返回 SUSPEND 却未保留 frame 时，会将 task 置为故障。
 */
static TZrBool task_frame_task_run(SZrState *state, SZrCoreTaskFrameTask *task) {
    SZrTypeValue result;
    EZrCoreTaskFramePollOutcome outcome;

    if (state == ZR_NULL || task == ZR_NULL || task->poll == ZR_NULL ||
        task->layout == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(&result);
    task->status = ZR_CORE_TASK_FRAME_STATUS_RUNNING;
    outcome = task->poll(state, task, task->userData, &result);
    if (outcome == ZR_CORE_TASK_FRAME_POLL_COMPLETE) {
        return task_frame_task_complete(state, task, &result);
    }
    if (outcome == ZR_CORE_TASK_FRAME_POLL_SUSPEND && task->frame != ZR_NULL) {
        task->status = ZR_CORE_TASK_FRAME_STATUS_SUSPENDED;
        return ZR_TRUE;
    }
    if (task->status != ZR_CORE_TASK_FRAME_STATUS_FAULTED) {
        ZrCore_TaskFrameTask_Fault(state, task, ZR_NULL);
    }
    return task->status == ZR_CORE_TASK_FRAME_STATUS_FAULTED;
}

void ZrCore_TaskFramePool_Init(SZrCoreTaskFramePool *pool) {
    if (pool != ZR_NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

void ZrCore_TaskFramePool_Free(SZrState *state, SZrCoreTaskFramePool *pool) {
    SZrCoreTaskFrame *frame;

    if (pool == ZR_NULL) {
        return;
    }
    frame = pool->freeFrames;
    while (frame != ZR_NULL) {
        SZrCoreTaskFrame *next = frame->next;
        task_frame_destroy(state, frame);
        frame = next;
    }
    memset(pool, 0, sizeof(*pool));
}

void ZrCore_TaskFrameTask_Init(SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL) {
        return;
    }
    memset(task, 0, sizeof(*task));
    task->status = ZR_CORE_TASK_FRAME_STATUS_IDLE;
    ZrCore_Value_ResetAsNull(&task->result);
    ZrCore_Value_ResetAsNull(&task->error);
}

void ZrCore_TaskFrameTask_Free(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL) {
        return;
    }
    if (task->status != ZR_CORE_TASK_FRAME_STATUS_IDLE) {
        task_frame_task_run_finally(state, task);
    }
    task_frame_task_release_frame(state, task);
    task_frame_release_root(state, &task->resultRoot);
    task_frame_release_root(state, &task->errorRoot);
    task_frame_release_value(state, &task->result);
    task_frame_release_value(state, &task->error);
    task->status = ZR_CORE_TASK_FRAME_STATUS_IDLE;
    task->pool = ZR_NULL;
    task->layout = ZR_NULL;
    task->poll = ZR_NULL;
    task->userData = ZR_NULL;
    task->resultConsumed = ZR_FALSE;
    task->finallyRan = ZR_FALSE;
    task->debugAsyncFaultProvenance = ZR_DEBUG_ASYNC_FAULT_NONE;
}

TZrBool ZrCore_TaskFrameTask_Start(SZrState *state,
                                   SZrCoreTaskFrameTask *task,
                                   SZrCoreTaskFramePool *pool,
                                   const SZrCoreTaskFrameLayout *layout,
                                   FZrCoreTaskFramePoll poll,
                                   TZrPtr userData) {
    if (state == ZR_NULL || task == ZR_NULL || pool == ZR_NULL || layout == ZR_NULL || poll == ZR_NULL ||
        task->status != ZR_CORE_TASK_FRAME_STATUS_IDLE ||
        (layout->slotCount != 0U && layout->slotLayouts == ZR_NULL)) {
        return ZR_FALSE;
    }
    task->pool = pool;
    task->layout = layout;
    task->poll = poll;
    task->userData = userData;
    return task_frame_task_run(state, task);
}

TZrBool ZrCore_TaskFrameTask_Resume(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->status != ZR_CORE_TASK_FRAME_STATUS_SUSPENDED || task->frame == ZR_NULL) {
        return ZR_FALSE;
    }
    return task_frame_task_run(state, task);
}

EZrCoreTaskFrameStatus ZrCore_TaskFrameTask_Status(const SZrCoreTaskFrameTask *task) {
    return task != ZR_NULL ? task->status : ZR_CORE_TASK_FRAME_STATUS_IDLE;
}

TZrBool ZrCore_TaskFrameTask_ProjectDebugTerminal(
        const SZrCoreTaskFrameTask *task,
        TZrBool isolatedTransport,
        SZrDebugAsyncTerminalEvent *outEvent) {
    return ZrCore_Debug_ProjectTaskFrameTerminal(
            task != ZR_NULL ? (TZrUInt32)task->status : (TZrUInt32)ZR_CORE_TASK_FRAME_STATUS_IDLE,
            isolatedTransport,
            task != ZR_NULL ? (EZrDebugAsyncFaultProvenance)task->debugAsyncFaultProvenance
                            : ZR_DEBUG_ASYNC_FAULT_NONE,
            outEvent);
}

TZrUInt32 ZrCore_TaskFrameTask_State(const SZrCoreTaskFrameTask *task) {
    return task != ZR_NULL && task->frame != ZR_NULL ? task->frame->stateId : 0U;
}

TZrBool ZrCore_TaskFrameTask_Suspend(SZrState *state,
                                     SZrCoreTaskFrameTask *task,
                                     TZrUInt32 stateId) {
    if (state == ZR_NULL || task == ZR_NULL || task->pool == ZR_NULL || task->layout == ZR_NULL ||
        stateId >= task->layout->stateCount) {
        return ZR_FALSE;
    }
    if (task->frame == ZR_NULL) {
        task->frame = task_frame_pool_take(state, task->pool, task->layout);
        if (task->frame == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    task->frame->stateId = stateId;
    return ZR_TRUE;
}

TZrBool ZrCore_TaskFrameTask_StoreSlot(SZrState *state,
                                       SZrCoreTaskFrameTask *task,
                                       TZrUInt32 slotIndex,
                                       const SZrTypeValue *value) {
    SZrCoreTaskFrame *frame;
    const SZrCoreTaskFrameSlotLayout *slotLayout;

    if (state == ZR_NULL || task == ZR_NULL || value == ZR_NULL || task->frame == ZR_NULL ||
        task->layout == ZR_NULL || slotIndex >= task->layout->slotCount) {
        return ZR_FALSE;
    }
    frame = task->frame;
    slotLayout = &task->layout->slotLayouts[slotIndex];
    if (frame->initialized[slotIndex]) {
        task_frame_cleanup_slot(state, frame, slotIndex);
    }
    ZrCore_Value_Copy(state, &frame->slots[slotIndex], value);
    frame->initialized[slotIndex] = ZR_TRUE;
    if (slotLayout->isGcRoot && frame->slots[slotIndex].isGarbageCollectable &&
        frame->slots[slotIndex].value.object != ZR_NULL &&
        !ZrCore_GcRootHandle_Create(state,
                                    frame->slots[slotIndex].value.object,
                                    &frame->roots[slotIndex])) {
        /* Root registration failed after the copy; roll back through the regular drop path. */
        task_frame_cleanup_slot(state, frame, slotIndex);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_TaskFrameTask_LoadSlot(SZrState *state,
                                      SZrCoreTaskFrameTask *task,
                                      TZrUInt32 slotIndex,
                                      SZrTypeValue *outValue) {
    SZrCoreTaskFrame *frame;
    const SZrCoreTaskFrameSlotLayout *slotLayout;
    SZrRawObject *rooted;

    if (state == ZR_NULL || task == ZR_NULL || outValue == ZR_NULL || task->frame == ZR_NULL ||
        task->layout == ZR_NULL || slotIndex >= task->layout->slotCount) {
        return ZR_FALSE;
    }
    frame = task->frame;
    if (!frame->initialized[slotIndex]) {
        return ZR_FALSE;
    }
    slotLayout = &task->layout->slotLayouts[slotIndex];
    if (slotLayout->isGcRoot && frame->slots[slotIndex].isGarbageCollectable &&
        frame->slots[slotIndex].value.object != ZR_NULL) {
        rooted = ZR_NULL;
        if (!ZrCore_GcRootHandle_Resolve(state, &frame->roots[slotIndex], &rooted) || rooted == ZR_NULL) {
            return ZR_FALSE;
        }
        frame->slots[slotIndex].value.object = rooted;
    }
    ZrCore_Value_Copy(state, outValue, &frame->slots[slotIndex]);
    return ZR_TRUE;
}

TZrBool ZrCore_TaskFrameTask_FaultWithDebugProvenance(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        const SZrTypeValue *error,
        TZrUInt32 faultProvenance) {
    if (state == ZR_NULL || task == ZR_NULL) {
        return ZR_FALSE;
    }
    if (faultProvenance >= ZR_DEBUG_ASYNC_FAULT_MAX) {
        return ZR_FALSE;
    }
    task_frame_task_run_finally(state, task);
    task_frame_task_release_frame(state, task);
    task_frame_release_root(state, &task->resultRoot);
    task_frame_release_value(state, &task->result);
    task_frame_release_root(state, &task->errorRoot);
    if (error != ZR_NULL) {
        ZrCore_Value_Copy(state, &task->error, error);
        if (!task_frame_root_value(state, &task->error, &task->errorRoot)) {
            task_frame_release_value(state, &task->error);
            /* TODO: 请明确保留 fault error 失败时 task 状态的契约。 */
            return ZR_FALSE;
        }
    } else {
        ZrCore_Value_ResetAsNull(&task->error);
    }
    task->status = ZR_CORE_TASK_FRAME_STATUS_FAULTED;
    task->debugAsyncFaultProvenance = faultProvenance;
    task->resultConsumed = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrCore_TaskFrameTask_Fault(SZrState *state,
                                   SZrCoreTaskFrameTask *task,
                                   const SZrTypeValue *error) {
    return ZrCore_TaskFrameTask_FaultWithDebugProvenance(
            state, task, error, ZR_DEBUG_ASYNC_FAULT_NONE);
}

EZrCoreTaskFrameAwaitStatus ZrCore_TaskFrameTask_Await(SZrState *state,
                                                        SZrCoreTaskFrameTask *task,
                                                        SZrTypeValue *outResult,
                                                        SZrTypeValue *outError) {
    if (task == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_AWAIT_FAULTED;
    }
    if (task->status == ZR_CORE_TASK_FRAME_STATUS_SUSPENDED ||
        task->status == ZR_CORE_TASK_FRAME_STATUS_RUNNING) {
        return ZR_CORE_TASK_FRAME_AWAIT_PENDING;
    }
    if (task->status == ZR_CORE_TASK_FRAME_STATUS_FAULTED) {
        if (outError != ZR_NULL) {
            ZrCore_Value_Copy(state, outError, &task->error);
        }
        return ZR_CORE_TASK_FRAME_AWAIT_FAULTED;
    }
    if (task->status != ZR_CORE_TASK_FRAME_STATUS_COMPLETED || outResult == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_AWAIT_FAULTED;
    }
    if (task->resultConsumed) {
        return ZR_CORE_TASK_FRAME_AWAIT_RESULT_CONSUMED;
    }
    if (task_frame_has_owned_value(&task->result)) {
        ZrCore_Value_AssignMaterializedStackValue(state, outResult, &task->result);
        task_frame_release_root(state, &task->resultRoot);
        task->resultConsumed = ZR_TRUE;
    } else {
        ZrCore_Value_Copy(state, outResult, &task->result);
    }
    return ZR_CORE_TASK_FRAME_AWAIT_READY;
}
