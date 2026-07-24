#ifndef ZR_VM_CORE_ITERATOR_RUNTIME_H
#define ZR_VM_CORE_ITERATOR_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrIteratorFrame;

typedef enum EZrIteratorFrameState {
    ZR_ITERATOR_FRAME_READY = 0,
    ZR_ITERATOR_FRAME_YIELDED,
    ZR_ITERATOR_FRAME_COMPLETED,
    ZR_ITERATOR_FRAME_FAULTED,
    ZR_ITERATOR_FRAME_CLOSED
} EZrIteratorFrameState;

typedef void (*TZrIteratorFrameProducer)(
        struct SZrState *state,
        struct SZrIteratorFrame *frame,
        TZrPtr userData);
typedef void (*TZrIteratorFrameCleanup)(
        struct SZrState *state,
        struct SZrIteratorFrame *frame,
        TZrPtr userData);

typedef struct SZrIteratorFrame {
    EZrIteratorFrameState state;
    SZrTypeValue currentValue;
    SZrGcRootHandle currentRoot;
    TZrIteratorFrameProducer producer;
    TZrIteratorFrameCleanup cleanup;
    TZrPtr userData;
    TZrBool hasCurrentRoot;
    TZrBool isMoving;
    TZrBool cleanupInvoked;
    struct SZrIteratorFrame *nextFree;
} SZrIteratorFrame;

typedef struct SZrIteratorFramePool {
    SZrIteratorFrame *freeList;
    TZrSize allocationCount;
    TZrSize reuseCount;
} SZrIteratorFramePool;

ZR_CORE_API void ZrCore_IteratorFrame_Init(
        struct SZrState *state,
        SZrIteratorFrame *frame,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup);
ZR_CORE_API TZrBool ZrCore_IteratorFrame_MoveNext(
        struct SZrState *state,
        SZrIteratorFrame *frame);
ZR_CORE_API TZrBool ZrCore_IteratorFrame_Current(
        struct SZrState *state,
        const SZrIteratorFrame *frame,
        SZrTypeValue *outValue);
ZR_CORE_API TZrBool ZrCore_IteratorFrame_Publish(
        struct SZrState *state,
        SZrIteratorFrame *frame,
        const SZrTypeValue *value);
ZR_CORE_API void ZrCore_IteratorFrame_Complete(
        struct SZrState *state,
        SZrIteratorFrame *frame);
ZR_CORE_API void ZrCore_IteratorFrame_Fault(
        struct SZrState *state,
        SZrIteratorFrame *frame);
ZR_CORE_API void ZrCore_IteratorFrame_Close(
        struct SZrState *state,
        SZrIteratorFrame *frame);
ZR_CORE_API void ZrCore_IteratorFramePool_Init(
        SZrIteratorFramePool *pool);
ZR_CORE_API SZrIteratorFrame *ZrCore_IteratorFramePool_Acquire(
        struct SZrState *state,
        SZrIteratorFramePool *pool,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup);
ZR_CORE_API TZrBool ZrCore_IteratorFramePool_Release(
        struct SZrState *state,
        SZrIteratorFramePool *pool,
        SZrIteratorFrame *frame);
ZR_CORE_API void ZrCore_IteratorFramePool_Free(
        struct SZrState *state,
        SZrIteratorFramePool *pool);

#endif
