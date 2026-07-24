#include <string.h>

#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

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

static void iterator_frame_run_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame) {
    if (frame != ZR_NULL && !frame->cleanupInvoked && frame->cleanup != ZR_NULL) {
        frame->cleanupInvoked = ZR_TRUE;
        frame->cleanup(state, frame, frame->userData);
    }
}

static TZrBool iterator_frame_is_terminal(const SZrIteratorFrame *frame) {
    return frame != ZR_NULL &&
           (frame->state == ZR_ITERATOR_FRAME_COMPLETED ||
            frame->state == ZR_ITERATOR_FRAME_FAULTED ||
            frame->state == ZR_ITERATOR_FRAME_CLOSED);
}

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

void ZrCore_IteratorFrame_Complete(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_COMPLETED);
}

void ZrCore_IteratorFrame_Fault(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_FAULTED);
}

void ZrCore_IteratorFrame_Close(SZrState *state, SZrIteratorFrame *frame) {
    iterator_frame_finish(state, frame, ZR_ITERATOR_FRAME_CLOSED);
}

void ZrCore_IteratorFramePool_Init(SZrIteratorFramePool *pool) {
    if (pool != ZR_NULL) {
        memset(pool, 0, sizeof(*pool));
    }
}

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
