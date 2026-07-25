#ifndef ZR_VM_CORE_TASK_FRAME_RUNTIME_H
#define ZR_VM_CORE_TASK_FRAME_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrCoreTaskFrame;
struct SZrDebugAsyncTerminalEvent;

typedef enum EZrCoreTaskFrameStatus {
    ZR_CORE_TASK_FRAME_STATUS_IDLE = 0,
    ZR_CORE_TASK_FRAME_STATUS_RUNNING,
    ZR_CORE_TASK_FRAME_STATUS_SUSPENDED,
    ZR_CORE_TASK_FRAME_STATUS_COMPLETED,
    ZR_CORE_TASK_FRAME_STATUS_FAULTED
} EZrCoreTaskFrameStatus;

typedef enum EZrCoreTaskFramePollOutcome {
    ZR_CORE_TASK_FRAME_POLL_COMPLETE = 0,
    ZR_CORE_TASK_FRAME_POLL_SUSPEND,
    ZR_CORE_TASK_FRAME_POLL_FAULT
} EZrCoreTaskFramePollOutcome;

typedef enum EZrCoreTaskFrameAwaitStatus {
    ZR_CORE_TASK_FRAME_AWAIT_READY = 0,
    ZR_CORE_TASK_FRAME_AWAIT_PENDING,
    ZR_CORE_TASK_FRAME_AWAIT_FAULTED,
    ZR_CORE_TASK_FRAME_AWAIT_RESULT_CONSUMED
} EZrCoreTaskFrameAwaitStatus;

typedef void (*FZrCoreTaskFrameDrop)(struct SZrState *state,
                                     SZrTypeValue *value,
                                     TZrPtr userData);

struct SZrCoreTaskFrameTask;

typedef void (*FZrCoreTaskFrameFinally)(struct SZrState *state,
                                        struct SZrCoreTaskFrameTask *task,
                                        TZrPtr userData);

typedef struct SZrCoreTaskFrameSlotLayout {
    TZrBool isGcRoot;
    TZrBool requiresDrop;
    FZrCoreTaskFrameDrop drop;
    TZrPtr dropUserData;
} SZrCoreTaskFrameSlotLayout;

typedef struct SZrCoreTaskFrameLayout {
    TZrUInt32 stateCount;
    TZrUInt32 slotCount;
    const SZrCoreTaskFrameSlotLayout *slotLayouts;
    FZrCoreTaskFrameFinally finally;
    TZrPtr finallyUserData;
} SZrCoreTaskFrameLayout;

typedef struct SZrCoreTaskFramePool {
    struct SZrCoreTaskFrame *freeFrames;
    TZrUInt32 frameAllocationCount;
    TZrUInt32 activeFrameCount;
    TZrUInt32 pooledFrameCount;
} SZrCoreTaskFramePool;

typedef struct SZrCoreTaskFrameTask SZrCoreTaskFrameTask;

typedef EZrCoreTaskFramePollOutcome (*FZrCoreTaskFramePoll)(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult);

struct SZrCoreTaskFrameTask {
    EZrCoreTaskFrameStatus status;
    struct SZrCoreTaskFrame *frame;
    SZrCoreTaskFramePool *pool;
    const SZrCoreTaskFrameLayout *layout;
    FZrCoreTaskFramePoll poll;
    TZrPtr userData;
    SZrTypeValue result;
    SZrTypeValue error;
    SZrGcRootHandle resultRoot;
    SZrGcRootHandle errorRoot;
    TZrUInt32 debugAsyncFaultProvenance;
    TZrBool resultConsumed;
    TZrBool finallyRan;
};

ZR_CORE_API void ZrCore_TaskFramePool_Init(SZrCoreTaskFramePool *pool);
ZR_CORE_API void ZrCore_TaskFramePool_Free(struct SZrState *state,
                                            SZrCoreTaskFramePool *pool);

ZR_CORE_API void ZrCore_TaskFrameTask_Init(SZrCoreTaskFrameTask *task);
ZR_CORE_API void ZrCore_TaskFrameTask_Free(struct SZrState *state,
                                            SZrCoreTaskFrameTask *task);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Start(struct SZrState *state,
                                                SZrCoreTaskFrameTask *task,
                                                SZrCoreTaskFramePool *pool,
                                                const SZrCoreTaskFrameLayout *layout,
                                                FZrCoreTaskFramePoll poll,
                                                TZrPtr userData);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Resume(struct SZrState *state,
                                                 SZrCoreTaskFrameTask *task);
ZR_CORE_API EZrCoreTaskFrameStatus ZrCore_TaskFrameTask_Status(
        const SZrCoreTaskFrameTask *task);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_ProjectDebugTerminal(
        const SZrCoreTaskFrameTask *task,
        TZrBool isolatedTransport,
        struct SZrDebugAsyncTerminalEvent *outEvent);
ZR_CORE_API TZrUInt32 ZrCore_TaskFrameTask_State(
        const SZrCoreTaskFrameTask *task);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Suspend(struct SZrState *state,
                                                  SZrCoreTaskFrameTask *task,
                                                  TZrUInt32 stateId);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_StoreSlot(struct SZrState *state,
                                                    SZrCoreTaskFrameTask *task,
                                                    TZrUInt32 slotIndex,
                                                    const SZrTypeValue *value);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_LoadSlot(struct SZrState *state,
                                                   SZrCoreTaskFrameTask *task,
                                                   TZrUInt32 slotIndex,
                                                   SZrTypeValue *outValue);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Fault(struct SZrState *state,
                                                SZrCoreTaskFrameTask *task,
                                                const SZrTypeValue *error);
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_FaultWithDebugProvenance(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        const SZrTypeValue *error,
        TZrUInt32 faultProvenance);
ZR_CORE_API EZrCoreTaskFrameAwaitStatus ZrCore_TaskFrameTask_Await(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        SZrTypeValue *outResult,
        SZrTypeValue *outError);

#endif /* ZR_VM_CORE_TASK_FRAME_RUNTIME_H */
