#include "zr_vm_core/task_frame_runtime.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"

/** @brief 私有的池化 continuation 存储，其 layout 借用自所属 task。 */
typedef struct SZrCoreTaskFrame {
    /* 只用于 pool 空闲链；借出时摘链并置空，不表示活动 task 的链接。 */
    struct SZrCoreTaskFrame *next;
    /* 三个数组的固定容量，pool 仅按 slotCount 完全匹配复用。 */
    TZrUInt32 slotCapacity;
    /* 借出的 continuation 状态；归还时清零，零也可为合法状态。 */
    TZrUInt32 stateId;
    /* 借出期间的 layout；归还前仍需它决定每个 slot 的清理策略。 */
    const SZrCoreTaskFrameLayout *layout;
    /* 宿主 calloc 的值槽数组；初始化位为真才参与 drop/root 清理。 */
    SZrTypeValue *slots;
    /* 与 slots 同索引的发布位；复制后置位，所有回滚都经正常清理撤位。 */
    TZrBool *initialized;
    /* 与 slots 同索引的域句柄；移动后的域地址须经 Resolve 才写回值槽。 */
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
    /* 释放 ownership 后才统一清空值槽；state 必须属于该值的有效域，不能拿跨域 Release 的拒绝当作成功释放。 */
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
    /* drop 先看到复制值，随后才释放 owner/root 并撤初始化位；根注册失败复用同一顺序。
    * TODO: drop 若抛异常或重入同 slot，撤位尚未执行；现有计数回调不足以确认外部清理约束。 */
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

    /* 只按容量复用已经清理的空闲帧，新 layout 将重新决定根与 drop 策略。 */
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
        /* 宿主 calloc 与 VM 预算/故障注入分配器分离；新帧尚未发布 layout，部分分配失败只释放存储。 */
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
            /* 值槽先置为无 ownership 的空值，后续 Value_Copy 才能安全读取目标元数据。 */
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
    /* 先清 slot 再解除 layout 借用并入空闲链；归还后不会留下有效初始化位或 root。 */
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
    /* 派发前发布 finallyRan，阻止嵌套入口再次调用 finally；不等于所有 task 清理都可重入。 */
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
    /* COMPLETE 仅在 UNIQUE/LOANED/SHARED/WEAK 时物化并消费临时源；BORROWED 与普通值复制到 header，源保留。 */
    task_frame_release_root(state, &task->resultRoot);
    ZrCore_Value_AssignMaterializedStackValue(state, &task->result, result);
    if (!task_frame_root_value(state, &task->result, &task->resultRoot)) {
        task_frame_release_value(state, &task->result);
        /* TODO: 结果 root 失败已释放 header.result，但未运行 finally/归还 frame/发布 COMPLETED；
         * 核查接入方是否总在 Start/Resume 返回 false 后 Free，以及应否转入 FAULTED。 */
        return ZR_FALSE;
    }
    /* 只有结果保留/root 成功才运行 finally、归还 frame 并发布 COMPLETED。 */
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
    /* 每次 poll 输出从空值开始；该自动结果只在 COMPLETE 路径物化到 header。
    * TODO: 核查外部 poll 在 SUSPEND/FAULT 时是否保持 outResult 为空以及抛异常后的收尾。 */
    ZrCore_Value_ResetAsNull(&result);
    task->status = ZR_CORE_TASK_FRAME_STATUS_RUNNING;
    outcome = task->poll(state, task, task->userData, &result);
    if (outcome == ZR_CORE_TASK_FRAME_POLL_COMPLETE) {
        return task_frame_task_complete(state, task, &result);
    }
    /* SUSPEND 必须已保留有效 frame；否则统一走故障清理，不能只凭 outcome 伪造暂停。 */
    if (outcome == ZR_CORE_TASK_FRAME_POLL_SUSPEND && task->frame != ZR_NULL) {
        task->status = ZR_CORE_TASK_FRAME_STATUS_SUSPENDED;
        return ZR_TRUE;
    }
    if (task->status != ZR_CORE_TASK_FRAME_STATUS_FAULTED) {
        ZrCore_TaskFrameTask_Fault(state, task, ZR_NULL);
    }
    return task->status == ZR_CORE_TASK_FRAME_STATUS_FAULTED;
}

/* 初始化空池供宿主或测试借帧；重置活跃池会丢失责任，调用方须先 Free 所有 task。 */
void ZrCore_TaskFramePool_Init(SZrCoreTaskFramePool *pool) {
    if (pool != ZR_NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

/* 只销毁已经归还的空闲帧；活动帧不在这条链上，调用方必须先清 task。 */
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

/* 新存储或 Free 后的 header 才可整体清零；在活跃 header 上调用会丢弃根与所有权值。 */
void ZrCore_TaskFrameTask_Init(SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL) {
        return;
    }
    memset(task, 0, sizeof(*task));
    task->status = ZR_CORE_TASK_FRAME_STATUS_IDLE;
    ZrCore_Value_ResetAsNull(&task->result);
    ZrCore_Value_ResetAsNull(&task->error);
}

/* 统一取消宿主对 frame/header 的持有：VM、pool 和借用 layout/context 都须仍有效。 */
void ZrCore_TaskFrameTask_Free(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL) {
        return;
    }
    if (task->status != ZR_CORE_TASK_FRAME_STATUS_IDLE) {
        /* finally 要在 slot 还可观察时运行，随后归还 frame；保持结果根直到其值不再被持有。 */
    task_frame_task_run_finally(state, task);
    }
    /* 归还帧后再释放 header 根与值，最后撤销所有借用关系，允许再次 Start。 */
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

/* 只借用调用方布局和上下文，绑定后立即同步 poll；true 包括已经处理的 FAULT 结果。 */
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
    /* layout/pool/poll/userData 只借用到 Free，不从 guest 动态 task 对象推导 continuation 状态。 */
    task->layout = layout;
    task->poll = poll;
    task->userData = userData;
    return task_frame_task_run(state, task);
}

/* 宿主显式恢复保留有帧的暂停任务；同一 task/pool 的并发或回调重入由接入方约束。 */
TZrBool ZrCore_TaskFrameTask_Resume(SZrState *state, SZrCoreTaskFrameTask *task) {
    if (task == ZR_NULL || task->status != ZR_CORE_TASK_FRAME_STATUS_SUSPENDED || task->frame == ZR_NULL) {
        return ZR_FALSE;
    }
    return task_frame_task_run(state, task);
}

EZrCoreTaskFrameStatus ZrCore_TaskFrameTask_Status(const SZrCoreTaskFrameTask *task) {
    return task != ZR_NULL ? task->status : ZR_CORE_TASK_FRAME_STATUS_IDLE;
}

/* 仅把 status/provenance 送入 debug 投影器，不推进 poll、不消费结果或改变生命周期。 */
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

/* 首次有效暂停才取得宿主帧；后续仅更新同一帧的 stateId，生命周期由 task 管理。 */
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

/* 源值必须满足 Value_Copy 的复制契约；替换已初始化槽时先履行旧值的 drop/释放责任。 */
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
    /* 复制后立即发布 initialized，使后续根注册失败也可以走正常 drop 回滚。 */
    ZrCore_Value_Copy(state, &frame->slots[slotIndex], value);
    frame->initialized[slotIndex] = ZR_TRUE;
    if (slotLayout->isGcRoot && frame->slots[slotIndex].isGarbageCollectable &&
        frame->slots[slotIndex].value.object != ZR_NULL &&
        !ZrCore_GcRootHandle_Create(state,
                                    frame->slots[slotIndex].value.object,
                                    &frame->roots[slotIndex])) {
        /* 根注册在复制后失败，仍需让已登记的 drop 观察复制值，再统一释放和撤位。 */
        task_frame_cleanup_slot(state, frame, slotIndex);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 输出必须先初始化；根句柄负责找回移动后的对象地址，复制输出自身不新增独立根。 */
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
        /* 域根表被 GC 重写后，此处 Resolve 的当前地址才是输出来源；外部保存的原始指针不随之更新。 */
        frame->slots[slotIndex].value.object = rooted;
    }
    ZrCore_Value_Copy(state, outValue, &frame->slots[slotIndex]);
    return ZR_TRUE;
}

/* 故障先履行 finally/frame 收尾，再保存错误与来源；错误 root 失败不会自动发布故障终态。 */
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
            /* TODO: 错误 root 失败已清理 frame/finally/error，但 status/provenance 仍未更新；
             * 核查故障入口失败后的状态投影及调用方 Free 约定，不将 false 当作已完成故障。 */
            return ZR_FALSE;
        }
    } else {
        ZrCore_Value_ResetAsNull(&task->error);
    }
    /* 故障状态与来源仅在错误值保留完成后发布；root 失败的旧状态另按 TODO 核查。 */
    task->status = ZR_CORE_TASK_FRAME_STATUS_FAULTED;
    task->debugAsyncFaultProvenance = faultProvenance;
    task->resultConsumed = ZR_FALSE;
    return ZR_TRUE;
}

/* 普通故障入口选择无额外 provenance；错误仍由 task 持有，Await 只复制到已初始化输出。 */
TZrBool ZrCore_TaskFrameTask_Fault(SZrState *state,
                                   SZrCoreTaskFrameTask *task,
                                   const SZrTypeValue *error) {
    return ZrCore_TaskFrameTask_FaultWithDebugProvenance(
            state, task, error, ZR_DEBUG_ASYNC_FAULT_NONE);
}

/* Await 是查询/消费入口；普通结果可重复读，拥有所有权的结果移交后不可再次取走。 */
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
    /* 故障 Await 只复制错误，不消费 header error 或撤销 errorRoot。 */
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
    /* 非 NONE 结果中 UNIQUE/LOANED/SHARED/WEAK 转移并清源；BORROWED 复制且保留 header 值，但也撤根并置 consumed，禁止再次取值。 */
    if (task_frame_has_owned_value(&task->result)) {
        ZrCore_Value_AssignMaterializedStackValue(state, outResult, &task->result);
        task_frame_release_root(state, &task->resultRoot);
        task->resultConsumed = ZR_TRUE;
    } else {
        /* 普通结果仍留在 header，可重复复制；task 的根只保活对象，不为输出创建新根。
        * TODO: 此分支没有 Resolve resultRoot；与 fault 输出及 Free 一起核查实际移动后的裸地址。 */
        ZrCore_Value_Copy(state, outResult, &task->result);
    }
    return ZR_CORE_TASK_FRAME_AWAIT_READY;
}
