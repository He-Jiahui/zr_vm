#ifndef ZR_VM_THREAD_RUNTIME_INTERNAL_H
#define ZR_VM_THREAD_RUNTIME_INTERNAL_H

#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_lib_thread/runtime.h"

#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership_transfer.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/task_runtime.h"

typedef enum EZrVmTaskTransportKind {
    /* Channel/Shared/Mutex 当前使用的 native transport 值种类；旧 direct-worker 也沿用它，
     * 当前隔离调度器另用 core OwnershipTransfer。 */
    ZR_VM_TASK_TRANSPORT_KIND_NONE = 0,
    ZR_VM_TASK_TRANSPORT_KIND_NULL = 1,
    ZR_VM_TASK_TRANSPORT_KIND_BOOL = 2,
    ZR_VM_TASK_TRANSPORT_KIND_INT = 3,
    ZR_VM_TASK_TRANSPORT_KIND_UINT = 4,
    ZR_VM_TASK_TRANSPORT_KIND_FLOAT = 5,
    ZR_VM_TASK_TRANSPORT_KIND_STRING = 6,
    ZR_VM_TASK_TRANSPORT_KIND_CHANNEL = 7,
    ZR_VM_TASK_TRANSPORT_KIND_TRANSFER = 8,
    ZR_VM_TASK_TRANSPORT_KIND_SHARED = 9,
    ZR_VM_TASK_TRANSPORT_KIND_WEAK_SHARED = 10,
    ZR_VM_TASK_TRANSPORT_KIND_UNIQUE_MUTEX = 11,
    ZR_VM_TASK_TRANSPORT_KIND_SHARED_MUTEX = 12
} EZrVmTaskTransportKind;

#define ZR_VM_TASK_SHARED_NATIVE_CELL_FIELD "__zr_task_shared_native_cell"
#define ZR_VM_TASK_SHARED_IS_WEAK_FIELD "__zr_task_shared_is_weak"
#define ZR_VM_TASK_MUTEX_NATIVE_CELL_FIELD "__zr_task_mutex_native_cell"
#define ZR_VM_TASK_MUTEX_KIND_FIELD "__zr_task_mutex_kind"
#define ZR_VM_TASK_LOCK_MUTEX_CELL_FIELD "__zr_task_lock_mutex_cell"
#define ZR_VM_TASK_LOCK_KIND_FIELD "__zr_task_lock_kind"
#define ZR_VM_TASK_LOCK_ACTIVE_FIELD "__zr_task_lock_active"

typedef enum EZrVmTaskMutexKind {
    ZR_VM_TASK_MUTEX_KIND_UNIQUE = 1,
    ZR_VM_TASK_MUTEX_KIND_SHARED = 2
} EZrVmTaskMutexKind;

typedef enum EZrVmTaskLockKind {
    ZR_VM_TASK_LOCK_KIND_EXCLUSIVE = 1,
    ZR_VM_TASK_LOCK_KIND_SHARED = 2
} EZrVmTaskLockKind;

#if defined(ZR_PLATFORM_WIN)
typedef CRITICAL_SECTION ZrVmTaskMutex;
typedef CONDITION_VARIABLE ZrVmTaskCondition;
#else
typedef pthread_mutex_t ZrVmTaskMutex;
typedef pthread_cond_t ZrVmTaskCondition;
#endif

typedef struct ZrVmTaskTransportValue {
    /* 原生队列或 cell 持有的跨线程副本；STRING 和引用型 kind 须由 transport_clear 结清。 */
    TZrUInt32 kind;
    TZrUInt32 reserved;
    /* STRING owns exactly this many bytes plus an extra terminating NUL. */
    TZrSize stringByteLength;
    union {
        TZrBool boolValue;
        TZrInt64 intValue;
        TZrUInt64 uintValue;
        TZrFloat64 floatValue;
        TZrChar *stringValue;
        TZrPtr pointerValue;
    } as;
} ZrVmTaskTransportValue;

typedef enum EZrVmTaskSharedCellLifecycle {
    ZR_VM_TASK_SHARED_CELL_LIFECYCLE_ALIVE = 1,
    ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DESTROYING = 2,
    ZR_VM_TASK_SHARED_CELL_LIFECYCLE_DROPPED = 3
} EZrVmTaskSharedCellLifecycle;

typedef struct ZrVmTaskSharedCell {
    /* Shared/WeakShared façade 共用的原生引用状态；GC 不减计数，Shared 需显式 release，
     * WeakShared 当前没有对应释放入口。 */
    ZrVmTaskMutex mutex;
    TZrUInt64 strongCount;
    TZrUInt64 weakCount;
    TZrUInt32 lifecycleState;
    TZrUInt32 reserved;
    ZrVmTaskTransportValue value;
} ZrVmTaskSharedCell;

typedef struct ZrVmTaskMutexCell {
    /* guard 保留 cell 引用和读写许可；unlock 负责归还，丢弃活跃 guard 没有析构路径。 */
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    TZrUInt64 refCount;
    TZrUInt64 readerCount;
    TZrBool writerActive;
    TZrUInt8 kind;
    TZrUInt8 reserved[6];
    ZrVmTaskTransportValue value;
} ZrVmTaskMutexCell;

enum {
    ZR_VM_TASK_SCHEDULER_MESSAGE_COMPLETE = 1,
    ZR_VM_TASK_SCHEDULER_MESSAGE_FAULT = 2,
    ZR_VM_TASK_SCHEDULER_MESSAGE_ISOLATED_COMPLETE = 3,
    ZR_VM_TASK_SCHEDULER_MESSAGE_ISOLATED_FAULT = 4
};

typedef struct ZrVmTaskSchedulerMessage {
    /* 旧 worker 传 handle+payload；当前隔离 worker 传 workItem+envelope，caller 域消费后确认。 */
    TZrUInt32 kind;
    TZrUInt32 reserved;
    struct SZrObject *handle;
    ZrVmTaskTransportValue payload;
    ZrLibraryTaskRuntimeWorkItem isolatedWorkItem;
    SZrOwnershipTransferEnvelope *isolatedEnvelope;
    SZrGcDomainIdentity isolatedWorkerDomain;
    TZrPtr isolatedCompletionContext;
    struct ZrVmTaskSchedulerMessage *next;
} ZrVmTaskSchedulerMessage;

typedef struct ZrVmTaskSchedulerRuntime {
    /* 调度器对象仅保存此 native 指针；worker 入队，caller 域消费完成消息。 */
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmTaskSchedulerMessage *head;
    ZrVmTaskSchedulerMessage *tail;
    TZrUInt64 isolateId;
} ZrVmTaskSchedulerRuntime;

typedef struct ZrVmAttachedDomainRequest {
    ZrLibraryTaskRuntimeWorkItem workItem;
    struct ZrVmAttachedDomainRequest *next;
    struct ZrVmAttachedDomainRequest *pendingNext;
} ZrVmAttachedDomainRequest;

typedef struct ZrVmAttachedDomainRuntime {
    /* attached worker 借用 caller global，离开前宿主必须维持 global 与同步原语有效。 */
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmAttachedDomainRequest *head;
    ZrVmAttachedDomainRequest *tail;
    ZrVmAttachedDomainRequest *pending;
    struct SZrGlobalState *global;
    TZrUInt32 workerLimit;
    TZrUInt32 liveWorkerCount;
    TZrUInt32 activeWorkerCount;
    TZrUInt32 queuedRequestCount;
    TZrBool accepting;
    ZrLibraryTaskRuntimeAwaitRegistration awaitRegistration;
} ZrVmAttachedDomainRuntime;

typedef struct ZrVmTaskWorkerLaunch {
    /* 启动工件拥有项目路径/捕获的 native 副本；allocator 参数借自 caller global。 */
    TZrChar *binaryPath;
    TZrChar *projectFile;
    TZrChar *projectDirectory;
    TZrChar *projectSource;
    TZrChar *projectBinary;
    TZrChar *projectEntry;
    TZrUInt32 captureCount;
    ZrVmTaskTransportValue *captures;
    ZrVmTaskSchedulerRuntime *ownerRuntime;
    SZrObject *ownerHandle;
    FZrAllocator allocator;
    TZrPtr userAllocationArguments;
    TZrUInt64 workerIsolateId;
    TZrBool supportMultithread;
} ZrVmTaskWorkerLaunch;

typedef struct ZrVmTaskChannelMessage {
    ZrVmTaskTransportValue value;
    struct ZrVmTaskChannelMessage *next;
} ZrVmTaskChannelMessage;

typedef struct ZrVmTaskChannelTransport {
    /* FIFO 和未读消息由原生内存持有；VM 的 Channel façade 仅借用此指针。 */
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmTaskSchedulerRuntime *notifyRuntime;
    ZrVmTaskChannelMessage *head;
    ZrVmTaskChannelMessage *tail;
    TZrUInt64 length;
    TZrBool closed;
} ZrVmTaskChannelTransport;

/* 两平台均以 broadcast 唤醒所有 waiter；wait 需在持锁时调用并由调用方循环检查谓词。 */
#if defined(ZR_PLATFORM_WIN)
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_init(ZrVmTaskMutex *mutex) { InitializeCriticalSection(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_destroy(ZrVmTaskMutex *mutex) { DeleteCriticalSection(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_lock(ZrVmTaskMutex *mutex) { EnterCriticalSection(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_unlock(ZrVmTaskMutex *mutex) { LeaveCriticalSection(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_init(ZrVmTaskCondition *condition) { InitializeConditionVariable(condition); }
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_destroy(ZrVmTaskCondition *condition) {
    ZR_UNUSED_PARAMETER(condition);
}
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_signal(ZrVmTaskCondition *condition) {
    WakeAllConditionVariable(condition);
}
static ZR_FORCE_INLINE TZrBool zr_vm_task_sync_condition_wait(ZrVmTaskCondition *condition,
                                                              ZrVmTaskMutex *mutex,
                                                              TZrUInt32 timeoutMs) {
    return SleepConditionVariableCS(condition, mutex, timeoutMs) ? ZR_TRUE : ZR_FALSE;
}
static ZR_FORCE_INLINE void zr_vm_task_sync_sleep_ms(TZrUInt32 timeoutMs) { Sleep(timeoutMs); }
#else
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_init(ZrVmTaskMutex *mutex) { pthread_mutex_init(mutex, ZR_NULL); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_destroy(ZrVmTaskMutex *mutex) { pthread_mutex_destroy(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_lock(ZrVmTaskMutex *mutex) { pthread_mutex_lock(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_mutex_unlock(ZrVmTaskMutex *mutex) { pthread_mutex_unlock(mutex); }
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_init(ZrVmTaskCondition *condition) { pthread_cond_init(condition, ZR_NULL); }
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_destroy(ZrVmTaskCondition *condition) { pthread_cond_destroy(condition); }
static ZR_FORCE_INLINE void zr_vm_task_sync_condition_signal(ZrVmTaskCondition *condition) { pthread_cond_broadcast(condition); }
static ZR_FORCE_INLINE TZrBool zr_vm_task_sync_condition_wait(ZrVmTaskCondition *condition,
                                                              ZrVmTaskMutex *mutex,
                                                              TZrUInt32 timeoutMs) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000u;
    ts.tv_nsec += (long)(timeoutMs % 1000u) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(condition, mutex, &ts) == 0 ? ZR_TRUE : ZR_FALSE;
}
static ZR_FORCE_INLINE void zr_vm_task_sync_sleep_ms(TZrUInt32 timeoutMs) {
    struct timespec ts;

    ts.tv_sec = timeoutMs / 1000u;
    ts.tv_nsec = (long)(timeoutMs % 1000u) * 1000000L;
    nanosleep(&ts, ZR_NULL);
}
#endif

/**
 * ## Summary
 * 从 native callback 的 Self 值取得借用对象，供 ThreadScheduler、
 * Channel 及包装类型回调继续核对其业务字段。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的上下文，借用至该 callback 结束；允许为空。
 * 当 Self payload 非空时，上下文及其 state 必须有效，且对象属于适配的 VM 域。
 *
 * ## Returns
 * @return context/Self 为空、值标签不是 OBJECT/ARRAY 或 payload 为空时返回 ZR_NULL；
 * 否则返回 receiver 的借用 SZrObject 指针，不转移所有权、不建立独立根。
 *
 * ## Constraints and lifecycle
 * 值标签筛选不验证包装类型、owner prototype 或业务字段。
 * ZR_CAST_OBJECT 的 raw object 种类检查仅在调试构建断言；
 * 调用者仍须提供合法动态对象。保活与所属域访问责任留在调用链，
 * 跨回调或异步阶段使用须另建立相应根和访问约束，不保存 context。
 *
 * ## References
 * ZrLib_CallContext_Self 刷新 callback 的 Self 栈视图；
 * zr_vm_thread_scheduler_schedule、Thread transport 和 wrapper 回调消费该对象；
 * ZR_CAST_OBJECT 负责对象/数组共用布局的转换。
 */
SZrObject *zr_vm_task_self_object(const ZrLibCallContext *context);
SZrObject *zr_vm_task_root_object(SZrState *state);
SZrObject *zr_vm_task_main_scheduler(SZrState *state);
ZrVmTaskSchedulerRuntime *zr_vm_task_scheduler_get_runtime(SZrState *state, SZrObject *scheduler);
void zr_vm_task_scheduler_signal_runtime(ZrVmTaskSchedulerRuntime *runtime);
TZrBool zr_vm_task_scheduler_process_external(SZrState *state, SZrObject *scheduler);
TZrBool zr_vm_task_scheduler_wait_for_external(SZrState *state, SZrObject *scheduler, TZrUInt32 timeoutMs);
TZrBool zr_vm_task_scheduler_enqueue_isolated_completion(
        ZrVmTaskSchedulerRuntime *runtime,
        ZrLibraryTaskRuntimeWorkItem *workItem,
        SZrOwnershipTransferEnvelope *envelope,
        SZrGcDomainIdentity workerDomain,
        TZrPtr completionContext,
        ZrVmTaskSchedulerMessage *preallocatedMessage);
TZrBool zr_vm_task_scheduler_enqueue_isolated_fault(
        ZrVmTaskSchedulerRuntime *runtime,
        ZrLibraryTaskRuntimeWorkItem *workItem,
        TZrPtr completionContext,
        ZrVmTaskSchedulerMessage *preallocatedMessage);
void zr_vm_thread_isolated_completion_processed(TZrPtr completionContext,
                                                TZrBool completed,
                                                TZrBool workerMustDisposeEnvelope);
void zr_vm_thread_isolated_abort_pending_caller_transfers(SZrState *state,
                                                           TZrPtr completionContext);
void zr_vm_thread_isolated_scheduler_dispatch_pending(SZrState *state,
                                                       SZrObject *scheduler);
void zr_vm_thread_isolated_scheduler_shutdown_all(SZrState *state);
void zr_vm_task_record_last_worker_isolate(SZrState *state, TZrUInt64 isolateId);
void zr_vm_task_record_last_worker_domain(SZrState *state, SZrGcDomainIdentity domain);
TZrUInt64 zr_vm_task_next_worker_isolate_id(void);
/**
 * ## Summary
 * 把已创建对象交给 native callback 的结果槽，
 * 统一 Thread 构造和包装回调的对象/数组返回约定。
 *
 * ## Parameters
 * @param[in] state 对象所属 VM 的有效当前 state；空值会返回 ZR_FALSE。
 * @param[out] result callback 的可覆写结果槽；为空时不写入。
 * 调用前不得遗留尚未处理的旧 ownership 值，重用槽须先处理旧值。
 * @param[in] object 在适配 VM 域中仍存活的对象；为空时不写入，
 * 本 helper 不接管其所有权。
 *
 * ## Returns
 * @return 任一参数为空时返回 ZR_FALSE 且不写结果；有效非空输入写入
 * raw object 和标签后返回 ZR_TRUE。true 不代表业务字段已初始化成功。
 *
 * ## Constraints and lifecycle
 * ZrLib_Value_SetObject 直接初始化对象值，不 Copy、不 retain，
 * 也不登记独立 GC root；不验证 domain、原型或完整业务类型。
 * 调用链须保持对象存活至输出值槽按 native dispatcher 规则完成交接，
 * 不能从本 helper 的 true 推导保活或字段发布成功。
 *
 * ## References
 * zr_vm_task_value_type_for_object 选择标签，ZrLib_Value_SetObject 写入结果；
 * ThreadScheduler、Channel、Shared/Mutex/Guard/Transfer 构造回调使用本约定。
 * zr_vm_task_create_async_handle 是旧本地链的调用，不是 canonical Job 入口。
 */
TZrBool zr_vm_task_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object);
/**
 * ## Summary
 * 按 C 字段名为 Thread 的 VM 对象保存字段值，
 * 让 scheduler、transport 和包装类型共享 Native 字段 API 的写入约定。
 *
 * ## Parameters
 * @param[in] state 有效当前 VM state，须与 object 的域适配；为空时不操作。
 * @param[in] object 目标 VM 对象的借用指针；为空时不操作。
 * @param[in] fieldName 本次调用可读、以 NUL 结尾的 C 字符串；为空时不操作。
 * @param[in] value 本次调用借用的 VM 值槽；为空时不操作。
 * 成功写入按 VM value-copy 规则存入对象，不转移调用者槽的管理责任。
 *
 * ## Returns
 * 无返回值。空输入不写入；本 wrapper 不把底层失败转换成成功/失败状态。
 * 正常返回不能证明字段已保存，也不替调用者处理异常状态。
 *
 * ## Constraints and lifecycle
 * ZrLib_Object_SetFieldCString 临时固定对象、值和键并可能分配字符串键；
 * pin 或建键失败可能提前返回，Core setter 也没有可供 wrapper 转交的返回状态。
 * 多字段初始化不具有本 helper 提供的回滚；调用方须按其具体发布协议处理失败。
 * 字段名和值只借用到本次调用结束，成功存储的对象边由 Core 管理。
 *
 * ## References
 * ZrLib_Object_SetFieldCString 规定 void 写入接口，
 * ZrCore_Object_SetValue 在成功存储路径执行 value copy 和 GC 边更新；
 * Thread 的标量 setter、scheduler 状态、transport 与 wrapper 共同调用本 helper。
 */
void zr_vm_task_set_value_field(SZrState *state,
                                SZrObject *object,
                                const TZrChar *fieldName,
                                const SZrTypeValue *value);
void zr_vm_task_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName);
void zr_vm_task_set_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);
void zr_vm_task_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);
void zr_vm_task_set_uint_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrUInt64 value);
const SZrTypeValue *zr_vm_task_get_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName);
SZrObject *zr_vm_task_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName);
TZrBool zr_vm_task_get_bool_field(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  TZrBool defaultValue);
TZrInt64 zr_vm_task_get_int_field(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  TZrInt64 defaultValue);
TZrUInt64 zr_vm_task_get_uint_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *fieldName,
                                    TZrUInt64 defaultValue);
TZrBool zr_vm_task_copy_value_or_null(SZrState *state, const SZrTypeValue *value, SZrTypeValue *result);
TZrBool zr_vm_task_default_support_multithread(SZrState *state);
TZrBool zr_vm_task_raise_runtime_error(SZrState *state, const TZrChar *message);
TZrBool zr_vm_task_require_multithread(SZrState *state, const TZrChar *message);
SZrObject *zr_vm_task_new_typed_object(SZrState *state, const TZrChar *typeName);
SZrObject *zr_vm_task_resolve_construct_target(ZrLibCallContext *context);
TZrBool zr_vm_task_is_integer_value(const SZrTypeValue *value);
TZrBool zr_vm_task_read_strict_int(const ZrLibCallContext *context, TZrSize index, TZrInt64 *outValue);
TZrBool zr_vm_task_read_strict_uint(const ZrLibCallContext *context, TZrSize index, TZrUInt64 *outValue);
TZrBool zr_vm_task_value_equals(const SZrTypeValue *lhs, const SZrTypeValue *rhs);
TZrBool zr_vm_task_transport_encode_value(SZrState *state,
                                          const SZrTypeValue *value,
                                          ZrVmTaskTransportValue *outValue,
                                          const TZrChar *contextMessage);
TZrBool zr_vm_task_transport_clone_value(const ZrVmTaskTransportValue *value, ZrVmTaskTransportValue *outValue);
TZrBool zr_vm_task_transport_decode_value(SZrState *state,
                                          const ZrVmTaskTransportValue *value,
                                          SZrTypeValue *result);
void zr_vm_task_transport_clear(ZrVmTaskTransportValue *value);
TZrBool zr_vm_task_channel_try_get_transport(SZrState *state,
                                             const SZrTypeValue *value,
                                             ZrVmTaskChannelTransport **outTransport);
TZrBool zr_vm_task_channel_make_value(SZrState *state, ZrVmTaskChannelTransport *transport, SZrTypeValue *result);
TZrBool zr_vm_task_mutex_try_get_cell(SZrState *state,
                                      const SZrTypeValue *value,
                                      ZrVmTaskMutexCell **outCell,
                                      TZrUInt32 *outKind);
TZrBool zr_vm_task_mutex_make_value(SZrState *state,
                                    ZrVmTaskMutexCell *cell,
                                    TZrUInt32 kind,
                                    SZrTypeValue *result);
TZrBool zr_vm_task_mutex_cell_add_ref(ZrVmTaskMutexCell *cell);
void zr_vm_task_mutex_cell_release(ZrVmTaskMutexCell *cell);
TZrBool zr_vm_task_shared_try_get_cell(SZrState *state,
                                       const SZrTypeValue *value,
                                       ZrVmTaskSharedCell **outCell,
                                       TZrBool *outIsWeak);
TZrBool zr_vm_task_shared_make_value(SZrState *state,
                                     ZrVmTaskSharedCell *cell,
                                     TZrBool isWeak,
                                     SZrTypeValue *result);
TZrBool zr_vm_task_shared_cell_is_alive_native(ZrVmTaskSharedCell *cell);
TZrBool zr_vm_task_shared_cell_add_strong_ref(ZrVmTaskSharedCell *cell);
TZrBool zr_vm_task_shared_cell_add_weak_ref(ZrVmTaskSharedCell *cell);
TZrBool zr_vm_task_shared_cell_add_weak_ref_if_alive(ZrVmTaskSharedCell *cell);
void zr_vm_task_shared_cell_release_strong(ZrVmTaskSharedCell *cell);
void zr_vm_task_shared_cell_release_weak(ZrVmTaskSharedCell *cell);
TZrBool zr_vm_task_spawn_thread_worker(ZrLibCallContext *context,
                                        const SZrTypeValue *callable,
                                         SZrTypeValue *result,
                                         SZrObject *mainScheduler);
TZrChar *zr_vm_task_worker_strdup(const TZrChar *text);
void zr_vm_task_worker_launch_free(ZrVmTaskWorkerLaunch *launch);
struct SZrLibrary_Project *zr_vm_task_worker_clone_project(
        SZrState *state,
        const ZrVmTaskWorkerLaunch *launch);
TZrBool zr_vm_task_worker_make_temp_path(TZrChar *buffer, TZrSize bufferSize);
TZrBool zr_vm_task_worker_load_function(SZrState *state,
                                         const TZrChar *path,
                                         struct SZrFunction **outFunction);
TZrBool zr_vm_task_worker_execute_callable(SZrState *state,
                                            const SZrTypeValue *callableValue,
                                            SZrTypeValue *resultValue);
TZrBool zr_vm_thread_attached_scheduler_init(SZrState *state,
                                              SZrObject *scheduler,
                                              TZrUInt32 workerCount);
TZrBool zr_vm_thread_attached_scheduler_schedule(SZrState *state,
                                                  SZrObject *scheduler,
                                                  SZrObject *job,
                                                  SZrTypeValue *result);
TZrBool zr_vm_thread_isolated_scheduler_init(SZrState *state,
                                              SZrObject *scheduler,
                                              TZrUInt32 workerCount,
                                              const SZrDomainTransferQuota *transferQuota);
TZrBool zr_vm_thread_isolated_scheduler_schedule(SZrState *state,
                                                  SZrObject *scheduler,
                                                  SZrObject *job,
                                                  SZrTypeValue *result);

TZrBool zr_vm_task_channel_construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_channel_send(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_channel_recv(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_channel_close(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_channel_is_closed(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_channel_length(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_load(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_store(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_clone(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_downgrade(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_release(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_weak_shared_upgrade(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_weak_shared_is_alive(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_transfer_construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_transfer_take(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_transfer_is_taken(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_unique_mutex_construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_unique_mutex_lock(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_mutex_construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_mutex_read(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_mutex_write(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_lock_load(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_lock_store(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_lock_unlock(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_lock_load(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool zr_vm_task_shared_lock_unlock(ZrLibCallContext *context, SZrTypeValue *result);

#endif
