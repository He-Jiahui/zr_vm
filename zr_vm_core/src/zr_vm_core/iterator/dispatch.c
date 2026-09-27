#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/state.h"

/* 外部推进入口只允许一次 producer 活动；producer 必须 Publish 或进入终态，
 * BUG: producer/析构若异常跳出，isMoving 不复位，后续推进永久被拒。 */
TZrBool ZrCore_IteratorFrame_MoveNext(SZrState *state, SZrIteratorFrame *frame) {
    if (state == ZR_NULL || frame == ZR_NULL || frame->isMoving ||
        frame->state == ZR_ITERATOR_FRAME_COMPLETED ||
        frame->state == ZR_ITERATOR_FRAME_FAULTED ||
        frame->state == ZR_ITERATOR_FRAME_CLOSED) {
        return ZR_FALSE;
    }
    if (frame->producer == ZR_NULL) {
        ZrCore_IteratorFrame_Fault(state, frame);
        return ZR_FALSE;
    }
    if (frame->state == ZR_ITERATOR_FRAME_YIELDED) {
        frame->state = ZR_ITERATOR_FRAME_READY;
    }

    frame->isMoving = ZR_TRUE;
    frame->producer(state, frame, frame->userData);
    frame->isMoving = ZR_FALSE;
    if (frame->state == ZR_ITERATOR_FRAME_READY) {
        ZrCore_IteratorFrame_Fault(state, frame);
    }
    return frame->state == ZR_ITERATOR_FRAME_YIELDED;
}
