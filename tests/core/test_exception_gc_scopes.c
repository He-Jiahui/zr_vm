#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"
#include "gc/gc_domain_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef volatile LONG ZrScopeAtomic;
typedef HANDLE ZrScopeThread;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef _Atomic int ZrScopeAtomic;
typedef pthread_t ZrScopeThread;
#endif

#define ZR_SCOPE_WAIT_ATTEMPTS 2000u
#define ZR_SCOPE_LOCK_OBSERVATION_MS 100u
#define ZR_SCOPE_MAJOR_DRAIN_LIMIT 4096u

typedef struct ZrScopeCallback {
    EZrGcNativeSafepointMode mode;
    TZrBool enterExecution;
    TZrBool returnNormally;
    TZrBool executionEntered;
    TZrBool nativeEntered;
    TZrBool firstMutationLocked;
    TZrBool secondMutationLocked;
} ZrScopeCallback;

typedef struct ZrScopeWorker {
    SZrState state;
    ZrScopeAtomic started;
    ZrScopeAtomic acquired;
    ZrScopeAtomic exited;
} ZrScopeWorker;

static SZrState *g_state;

static void scope_fail_process(const char *reason) {
    (void)fprintf(stderr, "exception_gc_scopes: %s\n", reason);
    (void)fflush(stderr);
    _Exit(EXIT_FAILURE);
}

static int scope_atomic_load(ZrScopeAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void scope_atomic_store(ZrScopeAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void scope_sleep_ms(TZrUInt32 milliseconds) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(milliseconds);
#else
    struct timespec duration;
    duration.tv_sec = (time_t)(milliseconds / 1000u);
    duration.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static TZrBool scope_wait_flag(ZrScopeAtomic *flag) {
    for (TZrUInt32 attempt = 0u; attempt < ZR_SCOPE_WAIT_ATTEMPTS; ++attempt) {
        if (scope_atomic_load(flag)) return ZR_TRUE;
        scope_sleep_ms(1u);
    }
    return (TZrBool)scope_atomic_load(flag);
}

static SZrGcDomainMutatorRecord scope_read_record(SZrState *state) {
    SZrGcDomainMutatorRecord result = {0};
    SZrGcDomain *domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state) {
            result = domain->mutators[index];
            break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    return result;
}

static void scope_start_major(void) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    ZrCore_GarbageCollector_SetWorkerCount(g_state->global, 1u);
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    if (!collector->concurrentMajorActive) {
        scope_fail_process("real concurrent major did not start");
    }
}

/* Failure observations are taken before this owner-thread cleanup, allowing
 * depth regressions to report through Unity without destroying a locked VM. */
static void scope_cleanup_caller(void) {
    SZrGcDomainMutatorRecord record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.mutationDepth; depth > 0u; --depth)
        ZrCore_GcDomain_MutationEnd(g_state, ZR_TRUE);
    record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.nativeDepth; depth > 0u; --depth)
        ZrCore_GcDomain_NativeLeave(g_state);
    record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.executionDepth; depth > 0u; --depth)
        ZrCore_GcDomain_MutatorLeave(g_state);
}

static void scope_finish_major(void) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    for (TZrUInt32 step = 0u;
         collector->concurrentMajorActive && step < ZR_SCOPE_MAJOR_DRAIN_LIMIT;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }
    if (collector->concurrentMajorActive)
        scope_fail_process("major did not drain before state cleanup");
}

static void scope_callback(SZrState *state, TZrPtr arguments) {
    ZrScopeCallback *context = (ZrScopeCallback *)arguments;
    if (context->enterExecution)
        context->executionEntered = ZrCore_GcDomain_MutatorEnter(state);
    context->nativeEntered = ZrCore_GcDomain_NativeEnter(state, context->mode);
    context->firstMutationLocked = ZrCore_GcDomain_MutationBegin(state);
    context->secondMutationLocked = ZrCore_GcDomain_MutationBegin(state);
    if (!context->returnNormally) {
        /* INVALID avoids allocating an Error while testing the unwind itself. */
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_INVALID);
    }
    ZrCore_GcDomain_MutationEnd(state, context->secondMutationLocked);
    ZrCore_GcDomain_MutationEnd(state, context->firstMutationLocked);
    if (context->nativeEntered) ZrCore_GcDomain_NativeLeave(state);
    if (context->executionEntered) ZrCore_GcDomain_MutatorLeave(state);
}

static void scope_assert_record_equal(
        const SZrGcDomainMutatorRecord *expected,
        const SZrGcDomainMutatorRecord *actual) {
    TEST_ASSERT_EQUAL_UINT64(expected->mutatorId, actual->mutatorId);
    TEST_ASSERT_EQUAL_UINT32(expected->executionDepth, actual->executionDepth);
    TEST_ASSERT_EQUAL_UINT32(expected->nativeDepth, actual->nativeDepth);
    TEST_ASSERT_EQUAL_UINT32(expected->mutationDepth, actual->mutationDepth);
    TEST_ASSERT_EQUAL_INT(expected->nativeMode, actual->nativeMode);
    TEST_ASSERT_EQUAL_INT(expected->status, actual->status);
    TEST_ASSERT_EQUAL_INT(expected->nativeEnteredFromInactive,
                          actual->nativeEnteredFromInactive);
}

static void scope_run_case(TZrBool outerExecution, TZrBool outerNative,
                           TZrBool outerMutation, EZrGcNativeSafepointMode mode,
                           TZrBool returnNormally) {
    ZrScopeCallback context = {0};
    SZrGcDomainMutatorRecord before;
    SZrGcDomainMutatorRecord after;
    EZrThreadStatus status;
    scope_start_major();
    if (outerExecution && !ZrCore_GcDomain_MutatorEnter(g_state))
        scope_fail_process("outer execution entry failed");
    if (outerNative && !ZrCore_GcDomain_NativeEnter(g_state, mode))
        scope_fail_process("outer native entry failed");
    if (outerMutation && !ZrCore_GcDomain_MutationBegin(g_state))
        scope_fail_process("outer mutation entry failed");
    before = scope_read_record(g_state);
    context.mode = mode;
    context.enterExecution = (TZrBool)(mode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    context.returnNormally = returnNormally;
    status = ZrCore_Exception_TryRun(g_state, scope_callback, &context);
    after = scope_read_record(g_state);
    (void)fprintf(stderr,
                  "exception_gc_scopes: exec %u->%u native %u->%u mutation %u->%u mode %d->%d\n",
                  (unsigned)before.executionDepth, (unsigned)after.executionDepth,
                  (unsigned)before.nativeDepth, (unsigned)after.nativeDepth,
                  (unsigned)before.mutationDepth, (unsigned)after.mutationDepth,
                  (int)before.nativeMode, (int)after.nativeMode);
    scope_cleanup_caller();
    scope_finish_major();
    TEST_ASSERT_TRUE(context.nativeEntered);
    TEST_ASSERT_TRUE(context.firstMutationLocked);
    TEST_ASSERT_TRUE(context.secondMutationLocked);
    if (context.enterExecution) TEST_ASSERT_TRUE(context.executionEntered);
    TEST_ASSERT_EQUAL_INT(returnNormally ? ZR_THREAD_STATUS_FINE : ZR_THREAD_STATUS_INVALID,
                          status);
    scope_assert_record_equal(&before, &after);
}

static void test_throw_restores_empty_entry(void) {
    scope_run_case(ZR_FALSE, ZR_FALSE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_execution(void) {
    scope_run_case(ZR_TRUE, ZR_FALSE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_gc_aware_native(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_detached_native(void) {
    scope_run_case(ZR_FALSE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED, ZR_FALSE);
}
static void test_throw_preserves_outer_critical_native(void) {
    scope_run_case(ZR_FALSE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL, ZR_FALSE);
}
static void test_throw_preserves_outer_mutation(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_TRUE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_normal_return_preserves_scopes(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_TRUE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_TRUE);
}

typedef struct ZrNestedScopeCallback {
    ZrScopeCallback inner;
    SZrGcDomainMutatorRecord beforeInner;
    SZrGcDomainMutatorRecord afterInner;
    EZrThreadStatus innerStatus;
    TZrBool outerEntered;
} ZrNestedScopeCallback;

static void scope_nested_callback(SZrState *state, TZrPtr arguments) {
    ZrNestedScopeCallback *context = (ZrNestedScopeCallback *)arguments;
    context->outerEntered = (TZrBool)(
            ZrCore_GcDomain_MutatorEnter(state) &&
            ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) &&
            ZrCore_GcDomain_MutationBegin(state));
    context->beforeInner = scope_read_record(state);
    context->inner.mode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    context->inner.enterExecution = ZR_TRUE;
    context->innerStatus = ZrCore_Exception_TryRun(state, scope_callback, &context->inner);
    context->afterInner = scope_read_record(state);
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_INVALID);
}

static void test_nested_tryrun_restores_each_entry_scope(void) {
    ZrNestedScopeCallback context = {0};
    SZrGcDomainMutatorRecord before;
    SZrGcDomainMutatorRecord after;
    EZrThreadStatus status;
    scope_start_major();
    before = scope_read_record(g_state);
    status = ZrCore_Exception_TryRun(g_state, scope_nested_callback, &context);
    after = scope_read_record(g_state);
    scope_cleanup_caller();
    scope_finish_major();
    TEST_ASSERT_TRUE(context.outerEntered);
    TEST_ASSERT_TRUE(context.inner.firstMutationLocked);
    TEST_ASSERT_TRUE(context.inner.secondMutationLocked);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, context.innerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, status);
    scope_assert_record_equal(&context.beforeInner, &context.afterInner);
    scope_assert_record_equal(&before, &after);
}

static void scope_worker_run(ZrScopeWorker *worker) {
    TZrBool locked;
    scope_atomic_store(&worker->started, 1);
    locked = ZrCore_GcDomain_MutationBegin(&worker->state);
    scope_atomic_store(&worker->acquired, locked ? 1 : 0);
    ZrCore_GcDomain_MutationEnd(&worker->state, locked);
    scope_atomic_store(&worker->exited, 1);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI scope_worker_entry(LPVOID argument) {
    scope_worker_run((ZrScopeWorker *)argument);
    return 0u;
}
#else
static void *scope_worker_entry(void *argument) {
    scope_worker_run((ZrScopeWorker *)argument);
    return ZR_NULL;
}
#endif

static void scope_run_lock_case(TZrBool retainOuterMutation) {
    ZrScopeCallback callback = {0};
    ZrScopeWorker worker;
    ZrScopeThread thread;
    SZrGcDomainMutatorRecord after;
    TZrBool outerBlockedWorker = ZR_TRUE;
    EZrThreadStatus status;
    memset(&worker, 0, sizeof(worker));
#if !defined(ZR_PLATFORM_WIN)
    atomic_init(&worker.started, 0);
    atomic_init(&worker.acquired, 0);
    atomic_init(&worker.exited, 0);
#endif
    scope_start_major();
    if (retainOuterMutation) {
        if (!ZrCore_GcDomain_MutatorEnter(g_state) ||
            !ZrCore_GcDomain_MutationBegin(g_state))
            scope_fail_process("outer lock entry failed");
    }
    callback.mode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    callback.enterExecution = ZR_TRUE;
    status = ZrCore_Exception_TryRun(g_state, scope_callback, &callback);
    after = scope_read_record(g_state);
    ZrCore_RawObject_Construct(&worker.state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    worker.state.global = g_state->global;
    if (!ZrCore_GcDomain_MutatorAttach(g_state, &worker.state))
        scope_fail_process("worker attachment failed");
#if defined(ZR_PLATFORM_WIN)
    thread = CreateThread(ZR_NULL, 0u, scope_worker_entry, &worker, 0u, ZR_NULL);
    if (thread == ZR_NULL) scope_fail_process("worker creation failed");
#else
    if (pthread_create(&thread, ZR_NULL, scope_worker_entry, &worker) != 0)
        scope_fail_process("worker creation failed");
#endif
    if (!scope_wait_flag(&worker.started)) scope_fail_process("worker never started");
    if (retainOuterMutation) {
        scope_sleep_ms(ZR_SCOPE_LOCK_OBSERVATION_MS);
        outerBlockedWorker = (TZrBool)!scope_atomic_load(&worker.acquired);
        scope_cleanup_caller();
    }
    if (!scope_wait_flag(&worker.exited))
        scope_fail_process("caught Throw retained mutation lock; refusing unsafe cleanup");
#if defined(ZR_PLATFORM_WIN)
    if (WaitForSingleObject(thread, ZR_SCOPE_WAIT_ATTEMPTS) != WAIT_OBJECT_0)
        scope_fail_process("worker did not join");
    CloseHandle(thread);
#else
    if (pthread_join(thread, ZR_NULL) != 0) scope_fail_process("worker did not join");
#endif
    ZrCore_GcDomain_MutatorDetach(&worker.state);
    scope_finish_major();
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, status);
    TEST_ASSERT_TRUE(callback.firstMutationLocked);
    TEST_ASSERT_TRUE(callback.secondMutationLocked);
    TEST_ASSERT_TRUE(scope_atomic_load(&worker.acquired));
    TEST_ASSERT_TRUE(outerBlockedWorker);
    TEST_ASSERT_EQUAL_UINT32(retainOuterMutation ? 1u : 0u, after.mutationDepth);
    TEST_ASSERT_EQUAL_UINT32(retainOuterMutation ? 1u : 0u, after.executionDepth);
}

static void test_throw_keeps_outer_lock_until_caller_releases_it(void) {
    scope_run_lock_case(ZR_TRUE);
}
static void test_throw_releases_lock_for_another_mutator(void) {
    scope_run_lock_case(ZR_FALSE);
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        SZrGcDomainMutatorRecord record = scope_read_record(g_state);
        if (record.mutationDepth != 0u || record.executionDepth != 0u ||
            record.nativeDepth != 0u ||
            g_state->global->garbageCollector->concurrentMajorActive)
            scope_fail_process("scope or major remains active at teardown");
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

int main(int argc, char **argv) {
    TZrBool depthsOnly = (TZrBool)(argc == 2 && strcmp(argv[1], "--scope-depths-only") == 0);
    UNITY_BEGIN();
    RUN_TEST(test_throw_restores_empty_entry);
    RUN_TEST(test_throw_preserves_outer_execution);
    RUN_TEST(test_throw_preserves_outer_gc_aware_native);
    RUN_TEST(test_throw_preserves_outer_detached_native);
    RUN_TEST(test_throw_preserves_outer_critical_native);
    RUN_TEST(test_throw_preserves_outer_mutation);
    RUN_TEST(test_normal_return_preserves_scopes);
    RUN_TEST(test_nested_tryrun_restores_each_entry_scope);
    if (!depthsOnly) {
        RUN_TEST(test_throw_keeps_outer_lock_until_caller_releases_it);
        RUN_TEST(test_throw_releases_lock_for_another_mutator);
    }
    return UNITY_END();
}
