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
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"

typedef enum EZrVmTaskTransportKind {
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
    TZrUInt32 kind;
    TZrUInt32 reserved;
    union {
        TZrBool boolValue;
        TZrInt64 intValue;
        TZrUInt64 uintValue;
        TZrFloat64 floatValue;
        TZrChar *stringValue;
        TZrPtr pointerValue;
    } as;
} ZrVmTaskTransportValue;

typedef struct ZrVmTaskSharedCell {
    ZrVmTaskMutex mutex;
    TZrUInt64 strongCount;
    TZrUInt64 weakCount;
    TZrBool alive;
    TZrUInt8 reserved[7];
    ZrVmTaskTransportValue value;
} ZrVmTaskSharedCell;

typedef struct ZrVmTaskMutexCell {
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
    ZR_VM_TASK_SCHEDULER_MESSAGE_FAULT = 2
};

typedef struct ZrVmTaskSchedulerMessage {
    TZrUInt32 kind;
    TZrUInt32 reserved;
    struct SZrObject *handle;
    ZrVmTaskTransportValue payload;
    struct ZrVmTaskSchedulerMessage *next;
} ZrVmTaskSchedulerMessage;

typedef struct ZrVmTaskSchedulerRuntime {
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmTaskSchedulerMessage *head;
    ZrVmTaskSchedulerMessage *tail;
    TZrUInt64 isolateId;
} ZrVmTaskSchedulerRuntime;

typedef struct ZrVmTaskChannelMessage {
    ZrVmTaskTransportValue value;
    struct ZrVmTaskChannelMessage *next;
} ZrVmTaskChannelMessage;

typedef struct ZrVmTaskChannelTransport {
    ZrVmTaskMutex mutex;
    ZrVmTaskCondition condition;
    ZrVmTaskSchedulerRuntime *notifyRuntime;
    ZrVmTaskChannelMessage *head;
    ZrVmTaskChannelMessage *tail;
    TZrUInt64 length;
    TZrBool closed;
} ZrVmTaskChannelTransport;

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

SZrObject *zr_vm_task_self_object(const ZrLibCallContext *context);
SZrObject *zr_vm_task_root_object(SZrState *state);
SZrObject *zr_vm_task_main_scheduler(SZrState *state);
ZrVmTaskSchedulerRuntime *zr_vm_task_scheduler_get_runtime(SZrState *state, SZrObject *scheduler);
void zr_vm_task_scheduler_signal_runtime(ZrVmTaskSchedulerRuntime *runtime);
TZrBool zr_vm_task_scheduler_process_external(SZrState *state, SZrObject *scheduler);
TZrBool zr_vm_task_scheduler_wait_for_external(SZrState *state, SZrObject *scheduler, TZrUInt32 timeoutMs);
void zr_vm_task_record_last_worker_isolate(SZrState *state, TZrUInt64 isolateId);
TZrUInt64 zr_vm_task_next_worker_isolate_id(void);
TZrBool zr_vm_task_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object);
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
void zr_vm_task_shared_cell_release_strong(ZrVmTaskSharedCell *cell);
void zr_vm_task_shared_cell_release_weak(ZrVmTaskSharedCell *cell);
TZrBool zr_vm_task_spawn_thread_worker(ZrLibCallContext *context,
                                       const SZrTypeValue *callable,
                                       SZrTypeValue *result,
                                       SZrObject *mainScheduler);

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
