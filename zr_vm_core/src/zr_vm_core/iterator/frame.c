#include <string.h>

#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

/* 换项与终态共用释放路径，当前 GC root 与 owned value 不跨越下一次 yield。 */
static void iterator_frame_clear_current(
        SZrState *state,
        SZrIteratorFrame *frame) {
    if (frame == ZR_NULL) {
        return;
    }
    if (frame->hasCurrentRoot) {
        ZrCore_GcRootHandle_Release(state, &frame->currentRoot);
        frame->hasCurrentRoot = ZR_FALSE;
    }
    ZrCore_Ownership_ReleaseValue(state, &frame->currentValue);
    ZrCore_Value_ResetAsNull(&frame->currentValue);
}

/* cleanup 是一次性 userData 归还钩子；置位先于回调，避免其重入时重复执行。 */
static void iterator_frame_run_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame) {
    if (frame != ZR_NULL && !frame->cleanupInvoked && frame->cleanup != ZR_NULL) {
        frame->cleanupInvoked = ZR_TRUE;
        frame->cleanup(state, frame, frame->userData);
    }
}

/* pool 只接纳已完成清理的终态 frame，活动 frame 保留原持有者。 */
static TZrBool iterator_frame_is_terminal(const SZrIteratorFrame *frame) {
    return frame != ZR_NULL &&
           (frame->state == ZR_ITERATOR_FRAME_COMPLETED ||
            frame->state == ZR_ITERATOR_FRAME_FAULTED ||
            frame->state == ZR_ITERATOR_FRAME_CLOSED);
}

/* 先撤销当前值和 root，再发布终态并调用 cleanup；
 * BUG: 当前值析构若抛异常，会跳过终态及 cleanup；MoveNext 也无法复位 isMoving。 */
static void iterator_frame_finish(
        SZrState *state,
        SZrIteratorFrame *frame,
        EZrIteratorFrameState terminalState) {
    if (frame == ZR_NULL || iterator_frame_is_terminal(frame)) {
        return;
    }
    iterator_frame_clear_current(state, frame);
    frame->state = terminalState;
    iterator_frame_run_cleanup(state, frame);
}

/* 初始化全新或已归还的 frame；producer/userData/cleanup 借用期覆盖整个活动期，
 * 对仍持有当前值的活动 frame 重调 Init 会丢失其释放路径。 */
void ZrCore_IteratorFrame_Init(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup) {
    ZR_UNUSED_PARAMETER(state);
    if (frame == ZR_NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    ZrCore_Value_ResetAsNull(&frame->currentValue);
    frame->state = ZR_ITERATOR_FRAME_READY;
    frame->producer = producer;
    frame->userData = userData;
    frame->cleanup = cleanup;
}

/* 返回当前 yield 的借用快照；GC 对象经 root 解析搬迁后的地址。
 * BUG: 普通 struct 克隆后按输入 root 覆盖输出，返回原对象而非复制值。 */
TZrBool ZrCore_IteratorFrame_Current(
        SZrState *state,
        const SZrIteratorFrame *frame,
        SZrTypeValue *outValue) {
    SZrRawObject *currentObject;

    if (frame == ZR_NULL || outValue == ZR_NULL ||
        frame->state != ZR_ITERATOR_FRAME_YIELDED) {
        return ZR_FALSE;
    }
    *outValue = frame->currentValue;
    if (frame->hasCurrentRoot) {
        if (!ZrCore_GcRootHandle_Resolve(
                state, &frame->currentRoot, &currentObject)) {
            return ZR_FALSE;
        }
        outValue->value.object = currentObject;
    }
    return ZR_TRUE;
}

/* producer 只能在 MoveNext 的 READY 阶段发布；BUG: 普通 struct 复制时会克隆，
 * 但 root 仍绑定输入对象；复制失败也报告 YIELDED，掩盖失败。 */
TZrBool ZrCore_IteratorFrame_Publish(
        SZrState *state,
        SZrIteratorFrame *frame,
        const SZrTypeValue *value) {
    if (state == ZR_NULL || frame == ZR_NULL || value == ZR_NULL ||
        !frame->isMoving || frame->state != ZR_ITERATOR_FRAME_READY) {
        return ZR_FALSE;
    }
    iterator_frame_clear_current(state, frame);
    if (value->isGarbageCollectable && value->value.object != ZR_NULL) {
        if (!ZrCore_GcRootHandle_Create(
                state, value->value.object, &frame->currentRoot)) {
            return ZR_FALSE;
        }
        frame->hasCurrentRoot = ZR_TRUE;
    }
    ZrCore_Value_Copy(state, &frame->currentValue, value);
    frame->state = ZR_ITERATOR_FRAME_YIELDED;
    return ZR_TRUE;
}

/* 正常耗尽仍走统一终态释放，后续 MoveNext 不再调用 producer。 */
void ZrCore_IteratorFrame_Complete(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_COMPLETED);
}

/* 缺失 producer 或未按协议发布时，终止本 frame 并运行一次 cleanup。 */
void ZrCore_IteratorFrame_Fault(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_FAULTED);
}

/* 消费方提前停止时显式归还当前值和 producer 私有状态。 */
void ZrCore_IteratorFrame_Close(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_CLOSED);
}

/* 池的存储由调用方持有；只能在无未归还 frame 时重新初始化。 */
void ZrCore_IteratorFramePool_Init(SZrIteratorFramePool *pool) {
    if (pool != ZR_NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

/* 从空闲链复用或按所属 state 的 allocator 建立新 frame；
 * 返回后调用方独占活动租约，须终止并 Release 才能进 freeList。 */
SZrIteratorFrame *ZrCore_IteratorFramePool_Acquire(
        SZrState *state,
        SZrIteratorFramePool *pool,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup) {
    SZrIteratorFrame *frame;

    if (state == ZR_NULL || pool == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    frame = pool->freeList;
    if (frame != ZR_NULL) {
        pool->freeList = frame->nextFree;
        pool->reuseCount++;
    } else {
        frame = (SZrIteratorFrame *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(*frame),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (frame == ZR_NULL) {
            return ZR_NULL;
        }
        pool->allocationCount++;
    }
    ZrCore_IteratorFrame_Init(state, frame, producer, userData, cleanup);
    return frame;
}

/* 仅将本池发放的终态 frame 归还；pool 不记录跨池所有权，
 * 调用方必须保持同一 pool/state 并避免在 Free 后使用旧指针。 */
TZrBool ZrCore_IteratorFramePool_Release(
        SZrState *state,
        SZrIteratorFramePool *pool,
        SZrIteratorFrame *frame) {
    if (state == ZR_NULL || pool == ZR_NULL ||
        !iterator_frame_is_terminal(frame)) {
        return ZR_FALSE;
    }
    iterator_frame_clear_current(state, frame);
    memset(frame, 0, sizeof(*frame));
    frame->nextFree = pool->freeList;
    pool->freeList = frame;
    return ZR_TRUE;
}

/* 只释放已归还的 freeList；host 应先关闭并归还所有活动 frame，
 * 且使用原分配所属 global，避免将原生分配留到 state 销毁后。 */
void ZrCore_IteratorFramePool_Free(
        SZrState *state,
        SZrIteratorFramePool *pool) {
    SZrIteratorFrame *frame;

    if (state == ZR_NULL || state->global == ZR_NULL || pool == ZR_NULL) {
        return;
    }
    frame = pool->freeList;
    pool->freeList = ZR_NULL;
    while (frame != ZR_NULL) {
        SZrIteratorFrame *next = frame->nextFree;

        ZrCore_Memory_RawFreeWithType(
                state->global,
                frame,
                sizeof(*frame),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        frame = next;
    }
}
