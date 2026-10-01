#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "gc/gc_domain_internal.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

#if !defined(ZR_PLATFORM_WIN)
#include <time.h>
#endif

#define ZR_GC_DOMAIN_MUTATOR_INITIAL_CAPACITY ((TZrSize)4u)
#define ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS ((TZrUInt32)10u)

/* 登记表只在协调锁内定位；返回的表项指针不得跨等待或扩容保存。 */
static SZrGcDomainMutatorRecord *gc_domain_find_mutator_locked(
        SZrGcDomain *domain,
        const SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        if (domain->mutators[index].state == state) {
            return &domain->mutators[index];
        }
    }
    return ZR_NULL;
}

/* 附着 state 时扩容原生登记表；先建新表，分配失败时保留旧表和既有登记。 */
static TZrBool gc_domain_grow_mutators_locked(SZrGcDomain *domain) {
    TZrSize newCapacity;
    TZrSize newBytes;
    SZrGcDomainMutatorRecord *newMutators;

    if (domain == ZR_NULL || domain->global == ZR_NULL) {
        return ZR_FALSE;
    }
    /* TODO: 容量翻倍及字节乘法未校验上界；核查可达到的 mutator 数和分配器的溢出约束。 */
    newCapacity = domain->mutatorCapacity == 0u
                          ? ZR_GC_DOMAIN_MUTATOR_INITIAL_CAPACITY
                          : domain->mutatorCapacity * 2u;
    newBytes = newCapacity * sizeof(SZrGcDomainMutatorRecord);
    newMutators = (SZrGcDomainMutatorRecord *)ZrCore_Memory_RawMallocWithType(
            domain->global, newBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (newMutators == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newMutators, 0, newBytes);
    if (domain->mutators != ZR_NULL && domain->mutatorLength > 0u) {
        ZrCore_Memory_RawCopy(
                newMutators,
                domain->mutators,
                domain->mutatorLength * sizeof(SZrGcDomainMutatorRecord));
    }
    if (domain->mutators != ZR_NULL && domain->mutatorCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(
                domain->global,
                domain->mutators,
                domain->mutatorCapacity * sizeof(SZrGcDomainMutatorRecord),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    domain->mutators = newMutators;
    domain->mutatorCapacity = newCapacity;
    return ZR_TRUE;
}

/* 暂停超时与等待遥测共用单调时钟，避免墙上时间跳变改变握手期限。 */
static TZrUInt64 gc_domain_now_milliseconds(void) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)GetTickCount64();
#else
    struct timespec current;
    if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) {
        return 0u;
    }
    return (TZrUInt64)current.tv_sec * 1000u +
           (TZrUInt64)current.tv_nsec / 1000000u;
#endif
}

/* 成功拿到暂停所有权后记录握手耗时；即使没有阻塞者也计为一次请求。 */
static void gc_domain_record_safepoint_wait(
        SZrGcDomain *domain,
        TZrUInt64 startedMilliseconds) {
    TZrUInt64 finishedMilliseconds;
    TZrUInt64 durationUs;

    if (domain == ZR_NULL) {
        return;
    }
    finishedMilliseconds = gc_domain_now_milliseconds();
    durationUs = finishedMilliseconds >= startedMilliseconds
                         ? (finishedMilliseconds - startedMilliseconds) * 1000u
                         : 0u;
    if (durationUs == 0u) {
        durationUs = 1u;
    }
    domain->safepointWaitCount++;
    domain->safepointWaitTotalUs += durationUs;
    if (domain->safepointWaitMaxUs < durationUs) {
        domain->safepointWaitMaxUs = durationUs;
    }
}

/* 持协调锁进入条件等待；短切片使广播丢失或超时竞争后仍能重查暂停状态。 */
static void gc_domain_wait_locked(
        SZrGcDomain *domain,
        TZrUInt32 milliseconds) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    (void)SleepConditionVariableCS(
            &domain->coordinationCondition,
            &domain->coordinationLock,
            milliseconds);
#else
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        return;
    }
    deadline.tv_sec += (time_t)(milliseconds / 1000u);
    deadline.tv_nsec += (long)(milliseconds % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    (void)pthread_cond_timedwait(
            &domain->coordinationCondition,
            &domain->coordinationLock,
            &deadline);
#endif
}

/* 新执行或原生入口必须在正在进行的暂停外等待；已运行者先公开本轮停靠代数。
 * 条件等待会放开协调锁，重新取得记录以容忍等待期间的登记表搬移。 */
static SZrGcDomainMutatorRecord *gc_domain_wait_for_entry_boundary_locked(
        SZrGcDomain *domain,
        SZrState *state,
        SZrGcDomainMutatorRecord *record) {
    if (record != ZR_NULL && record->mutationDepth > 0u) {
        return record;
    }
    /* BUG: 前次 PARKED 尚未恢复时新 collector 可推进 epoch；旧代数的记录继续等待，
     * first_blocker 会把它视为阻塞者，直到新暂停超时。 */
    while (record != ZR_NULL && domain->pauseRequested &&
           domain->collectorState != state) {
        if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING &&
            record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
            record->observedEpoch = domain->safepointEpoch;
            record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED;
            ZrCore_GcDomain_Broadcast(domain);
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
        record = gc_domain_find_mutator_locked(domain, state);
    }
    if (record != ZR_NULL &&
        record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        ZrCore_GcDomain_Broadcast(domain);
    }
    return record;
}

/* collector 只等待尚未承认当前 epoch 的运行者；detached native 必须遵守不持有可移动引用的契约。 */
static SZrGcDomainMutatorRecord *gc_domain_first_blocker_locked(
        SZrGcDomain *domain) {
    if (domain == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        SZrGcDomainMutatorRecord *record = &domain->mutators[index];

        if (record->state == domain->collectorState ||
            record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE ||
            record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED) {
            continue;
        }
        if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED &&
            record->observedEpoch == domain->safepointEpoch) {
            continue;
        }
        return record;
    }
    return ZR_NULL;
}

/* 域构造时建立暂停条件变量和独立的递归 mutation lock；失败不得发布半初始化域。 */
TZrBool ZrCore_GcDomain_CoordinationInit(SZrGcDomain *domain) {
    if (domain == ZR_NULL) {
        return ZR_FALSE;
    }
#if defined(ZR_PLATFORM_WIN)
    InitializeCriticalSection(&domain->coordinationLock);
    InitializeCriticalSection(&domain->mutationLock);
    InitializeConditionVariable(&domain->coordinationCondition);
    domain->mutationLockInitialized = ZR_TRUE;
#else
    pthread_mutexattr_t mutationAttributes;

    if (pthread_mutex_init(&domain->coordinationLock, ZR_NULL) != 0) {
        return ZR_FALSE;
    }
    if (pthread_cond_init(&domain->coordinationCondition, ZR_NULL) != 0) {
        (void)pthread_mutex_destroy(&domain->coordinationLock);
        return ZR_FALSE;
    }
    if (pthread_mutexattr_init(&mutationAttributes) != 0) {
        (void)pthread_cond_destroy(&domain->coordinationCondition);
        (void)pthread_mutex_destroy(&domain->coordinationLock);
        return ZR_FALSE;
    }
    if (pthread_mutexattr_settype(
                &mutationAttributes, PTHREAD_MUTEX_RECURSIVE) != 0 ||
        pthread_mutex_init(
                &domain->mutationLock, &mutationAttributes) != 0) {
        (void)pthread_mutexattr_destroy(&mutationAttributes);
        (void)pthread_cond_destroy(&domain->coordinationCondition);
        (void)pthread_mutex_destroy(&domain->coordinationLock);
        return ZR_FALSE;
    }
    (void)pthread_mutexattr_destroy(&mutationAttributes);
    domain->mutationLockInitialized = ZR_TRUE;
#endif
    domain->coordinationInitialized = ZR_TRUE;
    domain->nextMutatorId = 1u;
    return ZR_TRUE;
}

/* 域析构释放同步原语；调用者必须先使附着线程停止使用此域。 */
void ZrCore_GcDomain_CoordinationFree(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    if (domain->mutationLockInitialized) {
        DeleteCriticalSection(&domain->mutationLock);
    }
    DeleteCriticalSection(&domain->coordinationLock);
#else
    if (domain->mutationLockInitialized) {
        (void)pthread_mutex_destroy(&domain->mutationLock);
    }
    (void)pthread_cond_destroy(&domain->coordinationCondition);
    (void)pthread_mutex_destroy(&domain->coordinationLock);
#endif
    domain->mutationLockInitialized = ZR_FALSE;
    domain->coordinationInitialized = ZR_FALSE;
}

/* 根表、登记表及暂停状态的共同保护边界；不保护并发标记队列。 */
void ZrCore_GcDomain_Lock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    EnterCriticalSection(&domain->coordinationLock);
#else
    (void)pthread_mutex_lock(&domain->coordinationLock);
#endif
}

/* 与 Lock 配对；释放后不得继续使用先前取得的 mutator 表项指针。 */
void ZrCore_GcDomain_Unlock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    LeaveCriticalSection(&domain->coordinationLock);
#else
    (void)pthread_mutex_unlock(&domain->coordinationLock);
#endif
}

/* 状态转换后唤醒暂停请求者和停靠者；两类等待方醒来都须自行重查条件。 */
void ZrCore_GcDomain_Broadcast(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->coordinationInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    WakeAllConditionVariable(&domain->coordinationCondition);
#else
    (void)pthread_cond_broadcast(&domain->coordinationCondition);
#endif
}

/* 并发 major 的标记切片与写屏障用同一递归锁串行化共享标记状态。 */
void ZrCore_GcDomain_MutationLock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->mutationLockInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    EnterCriticalSection(&domain->mutationLock);
#else
    (void)pthread_mutex_lock(&domain->mutationLock);
#endif
}

/* 与 MutationLock 配对；不得在另一个线程或域上释放。 */
void ZrCore_GcDomain_MutationUnlock(SZrGcDomain *domain) {
    if (domain == ZR_NULL || !domain->mutationLockInitialized) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    LeaveCriticalSection(&domain->mutationLock);
#else
    (void)pthread_mutex_unlock(&domain->mutationLock);
#endif
}

/* 写屏障和物化路径先过暂停边界，并仅在并发 major 活跃时借用 mutation lock。
 * 返回值是是否持锁的令牌，调用方须原样交给 MutationEnd。 */
TZrBool ZrCore_GcDomain_MutationBegin(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrBool concurrentMajorActive;
    TZrBool beginSucceeded;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    record = gc_domain_wait_for_entry_boundary_locked(domain, state, record);
    /* TODO: false 同时表示无需锁和未登记；核查 Barrier/Object 写入调用方在登记失效时能否安全拒绝写入。 */
    if (record == ZR_NULL && domain->collectorState != state) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    concurrentMajorActive =
            domain->collector != ZR_NULL &&
            domain->collector->concurrentMajorActive;
    if (record != ZR_NULL &&
        record->mutationDepth == ~(TZrUInt32)0u) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    /* A nested begin already owns a recursive level even if a pause raced the
     * concurrent-major phase transition. */
    concurrentMajorActive =
            (TZrBool)(concurrentMajorActive ||
                      (record != ZR_NULL && record->mutationDepth > 0u));
    ZrCore_GcDomain_Unlock(domain);
    if (!concurrentMajorActive) {
        return ZR_FALSE;
    }

    /* Local Throw returns callback-owned levels to the TryRun entry snapshot. */
    ZrCore_GcDomain_MutationLock(domain);
    /* Registry storage can move or the state can be detached while the
     * coordination lock is released. Never retain the earlier record pointer. */
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    beginSucceeded = (TZrBool)(record != ZR_NULL ||
                               domain->collectorState == state);
    if (record != ZR_NULL) {
        if (record->mutationDepth == ~(TZrUInt32)0u) {
            beginSucceeded = ZR_FALSE;
        } else {
            record->mutationDepth++;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    if (!beginSucceeded) {
        ZrCore_GcDomain_MutationUnlock(domain);
    }
    return beginSucceeded;
}

/* 只释放本次 Begin 确实取得的锁，使未开启并发 major 的写入无需额外同步。 */
void ZrCore_GcDomain_MutationEnd(SZrState *state, TZrBool locked) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrBool pollDeferred = ZR_FALSE;

    if (!locked || state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL && record->mutationDepth > 0u) {
        record->mutationDepth--;
        if (record->mutationDepth == 0u) {
            pollDeferred = (TZrBool)(
                    (domain->pauseRequested && domain->collectorState != state));
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    ZrCore_GcDomain_MutationUnlock(domain);
    if (pollDeferred) {
        (void)ZrCore_GcDomain_MutatorPoll(state);
    }
}

/* 域先登记 inactive state，随后 AttachState 才发布 state->gcDomain 给 VM 和 GC 扫描。 */
TZrBool ZrCore_GcDomain_RegisterMutator(
        SZrGcDomain *domain,
        SZrState *state) {
    SZrGcDomainMutatorRecord *record;

    if (domain == ZR_NULL || state == ZR_NULL || !domain->active) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    if (domain->mutatorLength == domain->mutatorCapacity &&
        !gc_domain_grow_mutators_locked(domain)) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    record = &domain->mutators[domain->mutatorLength++];
    ZrCore_Memory_RawSet(record, 0, sizeof(*record));
    record->state = state;
    record->mutatorId = domain->nextMutatorId++;
    if (record->mutatorId == 0u) {
        record->mutatorId = domain->nextMutatorId++;
    }
    record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 从 GC 根扫描及暂停握手同时移除 state；调用者须已停止该 state 的所有执行。 */
void ZrCore_GcDomain_UnregisterMutator(
        SZrGcDomain *domain,
        SZrState *state) {
    if (domain == ZR_NULL || state == ZR_NULL) {
        return;
    }
    /* TODO: 此处未验证执行/原生深度；核查 State_Free 与 NativeCall_Detach 的并发退出路径。 */
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        if (domain->mutators[index].state == state) {
            TZrSize tailLength = domain->mutatorLength - index - 1u;
            for (TZrSize offset = 0u; offset < tailLength; offset++) {
                domain->mutators[index + offset] =
                        domain->mutators[index + offset + 1u];
            }
            domain->mutatorLength--;
            ZrCore_Memory_RawSet(
                    &domain->mutators[domain->mutatorLength],
                    0,
                    sizeof(SZrGcDomainMutatorRecord));
            break;
        }
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}

/* native/thread 入口只接受同一 global 的未附着 state，并复用 owner 的 GC 域。 */
TZrBool ZrCore_GcDomain_MutatorAttach(
        SZrState *ownerState,
        SZrState *mutatorState) {
    if (ownerState == ZR_NULL || ownerState->gcDomain == ZR_NULL ||
        mutatorState == ZR_NULL || mutatorState->gcDomain != ZR_NULL ||
        mutatorState->global != ownerState->global) {
        return ZR_FALSE;
    }
    ZrCore_GcDomain_AttachState(ownerState->gcDomain, mutatorState);
    return mutatorState->gcDomain == ownerState->gcDomain;
}

/* 解除 native/thread 临时附着；状态与执行栈的销毁仍由上层负责。 */
void ZrCore_GcDomain_MutatorDetach(SZrState *mutatorState) {
    SZrGcDomain *domain;

    if (mutatorState == ZR_NULL || mutatorState->gcDomain == ZR_NULL) {
        return;
    }
    domain = mutatorState->gcDomain;
    ZrCore_GcDomain_DetachState(domain, mutatorState);
}

/* VM 派发或次 state launch 进入可停靠执行作用域；嵌套进入只增加深度。 */
TZrBool ZrCore_GcDomain_MutatorEnter(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    record = gc_domain_wait_for_entry_boundary_locked(domain, state, record);
    if (record == ZR_NULL) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE) {
        record->executionDepth = 1u;
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
        record->observedEpoch = domain->safepointEpoch;
    } else if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING &&
               record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE &&
               record->executionDepth != ~(TZrUInt32)0u) {
        record->executionDepth++;
    } else {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 与每次成功 Enter 配对；最后一层执行及原生层都结束才可视为 inactive。 */
void ZrCore_GcDomain_MutatorLeave(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL && record->executionDepth > 0u) {
        record->executionDepth--;
        if (record->executionDepth == 0u && record->nativeDepth == 0u) {
            record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
            record->nativeEnteredFromInactive = ZR_FALSE;
            record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
            record->observedEpoch = domain->safepointEpoch;
            ZrCore_GcDomain_Broadcast(domain);
        }
    }
    ZrCore_GcDomain_Unlock(domain);
}

/* Exception_Throw 的非局部展开会越过普通 Leave；恢复登记状态以免 GC 永久等待旧作用域。 */
void ZrCore_GcDomain_MutatorUnwindScopes(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        /* BUG: attached-domain worker 只在循环前进入一次作用域；单个 Job 抛异常后
         * 此处清零外层深度，TryRun 返回后 worker 继续处理 Job 却被 GC 当作 inactive。 */
        record->executionDepth = 0u;
        record->nativeDepth = 0u;
        record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
        record->nativeEnteredFromInactive = ZR_FALSE;
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
        record->observedEpoch = domain->safepointEpoch;
        ZrCore_GcDomain_Broadcast(domain);
    }
    ZrCore_GcDomain_Unlock(domain);
}

/* VM/GC 安全点响应暂停请求，并在 collector 结束前保持当前 epoch 的停靠状态。 */
TZrBool ZrCore_GcDomain_MutatorPoll(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record == ZR_NULL || !domain->pauseRequested ||
        domain->collectorState == state ||
        record->status != ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING ||
        record->nativeMode != ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->mutationDepth > 0u) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    record->observedEpoch = domain->safepointEpoch;
    record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED;
    ZrCore_GcDomain_Broadcast(domain);
    /* BUG: 前一轮 End 与下一轮 Begin 可在本线程醒来前连续发生；旧 PARKED
     * 记录没有更新到新 epoch，collector 把它当阻塞者，直到新请求超时。 */
    while (domain->pauseRequested && domain->collectorState != state) {
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL &&
        record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
    }
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 诊断和线程测试读取域内登记编号；零表示没有可用登记。 */
TZrUInt64 ZrCore_GcDomain_GetMutatorId(const SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrUInt64 result = 0u;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return 0u;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL) {
        result = record->mutatorId;
    }
    ZrCore_GcDomain_Unlock(domain);
    return result;
}

/* 原生绑定进入前同时检查执行预算和暂停边界；嵌套原生层必须使用相同安全点模式。 */
TZrBool ZrCore_GcDomain_NativeEnter(
        SZrState *state,
        EZrGcNativeSafepointMode mode) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL ||
        mode < ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE ||
        mode > ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        return ZR_FALSE;
    }
    if (ZR_UNLIKELY(state->executionBudget != ZR_NULL) && !ZrCore_ExecutionBudget_Poll(state, ZR_FALSE)) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    /* BUG: critical 原生层内嵌套 NativeEnter 遇到 pauseRequested 时先在入口边界等待；
     * collector 同时把该 NO_SAFEPOINT_CRITICAL 记录当作阻塞者，只能等到暂停超时。 */
    record = gc_domain_wait_for_entry_boundary_locked(domain, state, record);
    if (record == ZR_NULL ||
        (ZR_UNLIKELY(state->executionBudget != ZR_NULL) && !ZrCore_ExecutionBudget_Poll(state, ZR_FALSE))) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->nativeDepth > 0u) {
        if (record->nativeMode != mode || record->nativeDepth == ~(TZrUInt32)0u) {
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        if (!ZrCore_ExecutionBudget_NativeEnter(state, ZR_TRUE)) {
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        record->nativeDepth++;
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    if (record->status != ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING &&
        record->status != ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecutionBudget_NativeEnter(state, ZR_TRUE)) {
        ZrCore_GcDomain_Unlock(domain);
        return ZR_FALSE;
    }
    if (record->status == ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
        record->nativeEnteredFromInactive = ZR_TRUE;
    } else {
        record->nativeEnteredFromInactive = ZR_FALSE;
    }
    record->nativeDepth = 1u;
    record->nativeMode = mode;
    if (mode == ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED;
    } else if (mode == ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL;
    }
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
    return ZR_TRUE;
}

/* 原生层退出后恢复 VM 可停靠状态；若暂停已请求，先 Poll 再继续执行 VM。 */
void ZrCore_GcDomain_NativeLeave(SZrState *state) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    TZrBool pollAfterLeave = ZR_FALSE;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_domain_find_mutator_locked(domain, state);
    if (record != ZR_NULL && record->nativeDepth > 0u) {
        record->nativeDepth--;
        if (record->nativeDepth == 0u) {
            record->nativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
            if (record->executionDepth == 0u) {
                record->status =
                        ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
                record->nativeEnteredFromInactive = ZR_FALSE;
            } else {
                record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING;
                pollAfterLeave = domain->pauseRequested &&
                                 domain->collectorState != state;
            }
            ZrCore_GcDomain_Broadcast(domain);
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    if (pollAfterLeave) {
        ZrCore_GcDomain_MutatorPoll(state);
    }
    if (ZR_UNLIKELY(state->executionBudget != ZR_NULL)) {
        (void)ZrCore_ExecutionBudget_Poll(state, ZR_FALSE);
    }
}

/* GC、checkpoint 和对象物化共用的域内暂停握手；成功者拥有一层须配对 End 的暂停。 */
TZrBool ZrCore_GcDomain_StopTheWorldBegin(
        SZrState *state,
        TZrUInt32 timeoutMilliseconds,
        SZrGcDomainPauseDiagnostic *outDiagnostic) {
    SZrGcDomain *domain;
    TZrUInt64 started;

    if (outDiagnostic != ZR_NULL) {
        ZrCore_Memory_RawSet(outDiagnostic, 0, sizeof(*outDiagnostic));
        outDiagnostic->blockingNativeMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    }
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return ZR_FALSE;
    }
    domain = state->gcDomain;
    started = gc_domain_now_milliseconds();
    ZrCore_GcDomain_Lock(domain);
    {
        SZrGcDomainMutatorRecord *caller =
                gc_domain_find_mutator_locked(domain, state);
        if (caller != ZR_NULL &&
            (caller->status ==
                     ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED ||
             caller->status ==
                     ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL)) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
                outDiagnostic->blockingMutatorId = caller->mutatorId;
                outDiagnostic->blockingNativeMode = caller->nativeMode;
                outDiagnostic->blockingState = caller->state;
                outDiagnostic->blockingNativeFrame =
                        caller->state != ZR_NULL
                                ? caller->state->callInfoList
                                : ZR_NULL;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
    }
    if (domain->pauseRequested && domain->collectorState == state) {
        domain->pauseDepth++;
        if (outDiagnostic != ZR_NULL) {
            outDiagnostic->safepointEpoch = domain->safepointEpoch;
        }
        ZrCore_GcDomain_Unlock(domain);
        return ZR_TRUE;
    }
    /* BUG: 第二个 RUNNING mutator 竞争暂停时直接在此等待，未承认首个请求的 epoch；
     * 首个 collector 把它当阻塞者，双方直到首个请求超时才可前进。 */
    /* TODO: now_milliseconds 失败返回 0，若已记录非零起点，此处无符号差值会提前超时；核查时钟故障策略。 */
    while (domain->pauseRequested) {
        TZrUInt64 elapsed = gc_domain_now_milliseconds() - started;
        if (elapsed >= timeoutMilliseconds) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
    domain->safepointEpoch++;
    if (domain->safepointEpoch == 0u) {
        domain->safepointEpoch = 1u;
    }
    domain->pauseRequested = ZR_TRUE;
    domain->collectorState = state;
    domain->pauseDepth = 1u;
    ZrCore_GcDomain_Broadcast(domain);

    for (;;) {
        SZrGcDomainMutatorRecord *blocker = gc_domain_first_blocker_locked(domain);
        TZrUInt64 elapsed;

        if (blocker == ZR_NULL) {
            gc_domain_record_safepoint_wait(domain, started);
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
            }
            ZrCore_GcDomain_Unlock(domain);
            return ZR_TRUE;
        }
        elapsed = gc_domain_now_milliseconds() - started;
        if (elapsed >= timeoutMilliseconds) {
            if (outDiagnostic != ZR_NULL) {
                outDiagnostic->timedOut = ZR_TRUE;
                outDiagnostic->safepointEpoch = domain->safepointEpoch;
                outDiagnostic->blockingMutatorId = blocker->mutatorId;
                outDiagnostic->blockingNativeMode = blocker->nativeMode;
                outDiagnostic->blockingState = blocker->state;
                /* BUG: blocker 仍运行时会无协调锁地改写 callInfoList；跨线程读取
                 * 形成数据竞争，且返回的帧指针可能在调用方使用前失效。 */
                outDiagnostic->blockingNativeFrame =
                        blocker->state != ZR_NULL
                                ? blocker->state->callInfoList
                                : ZR_NULL;
            }
            domain->pauseRequested = ZR_FALSE;
            domain->collectorState = ZR_NULL;
            domain->pauseDepth = 0u;
            ZrCore_GcDomain_Broadcast(domain);
            ZrCore_GcDomain_Unlock(domain);
            return ZR_FALSE;
        }
        gc_domain_wait_locked(domain, ZR_GC_DOMAIN_WAIT_SLICE_MILLISECONDS);
    }
}

/* 只有发起暂停的 state 能释放本层；嵌套请求到最外层才允许其他 mutator 继续。 */
void ZrCore_GcDomain_StopTheWorldEnd(SZrState *state) {
    SZrGcDomain *domain;

    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    if (!domain->pauseRequested || domain->collectorState != state) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    if (domain->pauseDepth > 1u) {
        domain->pauseDepth--;
        ZrCore_GcDomain_Unlock(domain);
        return;
    }
    domain->pauseRequested = ZR_FALSE;
    domain->collectorState = ZR_NULL;
    domain->pauseDepth = 0u;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}

/* 测试与遥测在协调锁内复制计数，避免向外泄露会随登记表扩容移动的记录。 */
void ZrCore_GcDomain_GetMutatorSnapshot(
        const SZrState *state,
        SZrGcDomainMutatorSnapshot *outSnapshot) {
    SZrGcDomain *domain;

    if (outSnapshot == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(outSnapshot, 0, sizeof(*outSnapshot));
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    outSnapshot->safepointEpoch = domain->safepointEpoch;
    outSnapshot->registeredMutatorCount = (TZrUInt32)domain->mutatorLength;
    outSnapshot->pauseRequested = domain->pauseRequested;
    for (TZrSize index = 0u; index < domain->mutatorLength; index++) {
        switch (domain->mutators[index].status) {
            case ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING:
                outSnapshot->runningMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED:
                outSnapshot->parkedMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED:
                outSnapshot->blockingDetachedMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL:
                outSnapshot->noSafepointCriticalMutatorCount++;
                break;
            case ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE:
            default:
                break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
}

/* 外部线程改变等待条件后发信号；仅唤醒检查，不代替停靠或解除暂停。 */
void ZrCore_GcDomain_WakeMutators(SZrState *state) {
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) {
        return;
    }
    ZrCore_GcDomain_Lock(state->gcDomain);
    ZrCore_GcDomain_Broadcast(state->gcDomain);
    ZrCore_GcDomain_Unlock(state->gcDomain);
}
