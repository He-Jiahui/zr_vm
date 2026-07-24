#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/state.h"

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
