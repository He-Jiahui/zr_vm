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

/** @brief provider 自有的等待回调登记；scheduler 字段仅保存此结构的裸指针。
 *  @pre 登记对象及其 context 必须活到 scheduler 停止使用等待钩子之后。
 */
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

/** @brief 让 provider 消费 Job 并获得 caller-domain Task 的 GC 根句柄。
 *  @pre 同一 Job 只能消费一次；provider 必须在同一 GC 域完成或置错后调用 ReleasePreparedJob。
 *  @note isolated worker 只复制 callable 到自己的域；Task 的完成写回在 caller 域进行。
 */
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

/** @brief provider 注册等待钩子，使 Task.result 经 scheduler 等待任务完成。
 *  @pre registration 保持有效直到不再可能调用 Task.result；该 API 不复制结构体。
 */
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
