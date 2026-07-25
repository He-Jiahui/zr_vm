//
// Narrow provider handoff for the canonical zr.task Job/Scheduler contract.
//

#ifndef ZR_VM_LIBRARY_TASK_RUNTIME_H
#define ZR_VM_LIBRARY_TASK_RUNTIME_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/gc_domain.h"

struct SZrObject;
struct SZrState;
struct SZrTypeValue;

typedef struct ZrLibraryTaskRuntimeWorkItem {
    SZrGcRootHandle taskRoot;
} ZrLibraryTaskRuntimeWorkItem;

typedef TZrBool (*FZrLibraryTaskRuntimeAwaitHook)(
        struct SZrState *state,
        struct SZrObject *task,
        TZrPtr context);

typedef struct ZrLibraryTaskRuntimeAwaitRegistration {
    FZrLibraryTaskRuntimeAwaitHook awaitHook;
    TZrPtr context;
} ZrLibraryTaskRuntimeAwaitRegistration;

/*
 * Consume a canonical Job exactly once and create its caller-domain Task
 * through the existing Scheduler ABI. Providers use this instead of reading
 * Job implementation fields.
 */
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_ScheduleJob(
        struct SZrState *state,
        struct SZrObject *scheduler,
        struct SZrObject *job,
        struct SZrTypeValue *result);

/* Prepare a provider-owned work item without exposing Job storage. The returned
 * root keeps the caller-domain Task and its callable alive until the provider
 * executes, faults, or releases the item exactly once. */
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_PrepareJob(
        struct SZrState *state,
        struct SZrObject *scheduler,
        struct SZrObject *job,
        struct SZrTypeValue *result,
        ZrLibraryTaskRuntimeWorkItem *outItem);
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_ExecutePreparedJob(
        struct SZrState *state,
        ZrLibraryTaskRuntimeWorkItem *item);
ZR_LIBRARY_API void ZrLibrary_TaskRuntime_FaultPreparedJob(
        struct SZrState *state,
        ZrLibraryTaskRuntimeWorkItem *item,
        const TZrChar *message);
ZR_LIBRARY_API void ZrLibrary_TaskRuntime_ReleasePreparedJob(
        struct SZrState *state,
        ZrLibraryTaskRuntimeWorkItem *item);

/* Isolated providers may inspect the callable retained by a prepared caller
 * Task and settle that Task from a caller-domain completion queue. Neither API
 * exposes the Job object or permits a worker-domain task root. */
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_CopyPreparedCallable(
        struct SZrState *state,
        const ZrLibraryTaskRuntimeWorkItem *item,
        struct SZrTypeValue *outCallable);
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_CompletePreparedJob(
        struct SZrState *state,
        ZrLibraryTaskRuntimeWorkItem *item,
        const struct SZrTypeValue *result);

/* Provider schedulers register a native wait hook so Task.result() can await
 * completion without exposing a source-level pump/step member. */
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_RegisterAwaitHook(
        struct SZrState *state,
        struct SZrObject *scheduler,
        const ZrLibraryTaskRuntimeAwaitRegistration *registration);
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_AwaitProviderTask(
        struct SZrState *state,
        struct SZrObject *scheduler,
        struct SZrObject *task,
        TZrBool *outHandled);
ZR_LIBRARY_API TZrBool ZrLibrary_TaskRuntime_IsTaskComplete(
        struct SZrState *state,
        struct SZrObject *task);

#endif // ZR_VM_LIBRARY_TASK_RUNTIME_H
