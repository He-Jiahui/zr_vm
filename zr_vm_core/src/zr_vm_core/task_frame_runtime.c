#include "zr_vm_core/task_frame_runtime.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"

typedef struct SZrCoreTaskFrame {
    struct SZrCoreTaskFrame *next;
    TZrUInt32 slotCapacity;
    TZrUInt32 stateId;
    const SZrCoreTaskFrameLayout *layout;
    SZrTypeValue *slots;
    TZrBool *initialized;
    SZrGcRootHandle *roots;
} SZrCoreTaskFrame;

static TZrBool task_frame_has_owned_value(const SZrTypeValue *value) {
    return value != ZR_NULL && value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE;
}

static void task_frame_release_value(SZrState *state, SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }
    if (task_frame_has_owned_value(value)) {
        ZrCore_Ownership_ReleaseValue(state, value);
    }
    ZrCore_Value_ResetAsNull(value);
}

static void task_frame_release_root(SZrState *state, SZrGcRootHandle *root) {
    if (root != ZR_NULL) {
        ZrCore_GcRootHandle_Release(state, root);
    }
}

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

static void task_frame_cleanup_slots(SZrState *state, SZrCoreTaskFrame *frame) {
    TZrUInt32 index;

    if (frame == ZR_NULL || frame->layout == ZR_NULL) {
        return;
    }
    for (index = 0U; index < frame->layout->slotCount; index++) {
        task_frame_cleanup_slot(state, frame, index);
    }
}

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

static void task_frame_task_release_frame(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->frame == ZR_NULL) {
        return;
    }
    task_frame_pool_return(state, task->pool, task->frame);
    task->frame = ZR_NULL;
}

static void task_frame_task_run_finally(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->finallyRan || task->layout == ZR_NULL) {
        return;
    }
    task->finallyRan = ZR_TRUE;
    if (task->layout->finally != ZR_NULL) {
        task->layout->finally(state, task, task->layout->finallyUserData);
    }
}

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
        return ZR_FALSE;
    }
    task_frame_task_run_finally(state, task);
    task_frame_task_release_frame(state, task);
    task->status = ZR_CORE_TASK_FRAME_STATUS_COMPLETED;
    task->resultConsumed = ZR_FALSE;
    return ZR_TRUE;
}

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
        task_frame_release_value(state, &frame->slots[slotIndex]);
        frame->initialized[slotIndex] = ZR_FALSE;
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

TZrBool ZrCore_TaskFrameTask_Fault(SZrState *state,
                                   SZrCoreTaskFrameTask *task,
                                   const SZrTypeValue *error) {
    if (state == ZR_NULL || task == ZR_NULL) {
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
            return ZR_FALSE;
        }
    } else {
        ZrCore_Value_ResetAsNull(&task->error);
    }
    task->status = ZR_CORE_TASK_FRAME_STATUS_FAULTED;
    task->resultConsumed = ZR_FALSE;
    return ZR_TRUE;
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
