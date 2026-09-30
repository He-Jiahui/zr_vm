#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include "gc/gc_domain_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrTestThread;
typedef volatile LONG ZrTestAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrTestThread;
typedef _Atomic int ZrTestAtomic;
#endif

#define ZR_GC_NESTED_MUTATION_PAUSE_TIMEOUT_MS ((TZrUInt32)1500u)
#define ZR_GC_NESTED_MUTATION_WAIT_ATTEMPTS ((TZrUInt32)4000u)
#define ZR_GC_NESTED_MUTATION_EXIT_WAIT_ATTEMPTS ((TZrUInt32)5000u)

typedef struct ZrNestedMutationContext {
    SZrState state;
    SZrObject *owner;
    SZrTypeValue key;
    SZrTypeValue value;
    ZrTestAtomic ready;
    ZrTestAtomic failed;
    ZrTestAtomic outerScopeLocked;
    ZrTestAtomic mutationCompleted;
    ZrTestAtomic continueWithoutPause;
    ZrTestAtomic exited;
} ZrNestedMutationContext;

static SZrState *g_state;

static int test_atomic_load(ZrTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void test_atomic_initialize(ZrTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, 0);
#else
    atomic_init(value, 0);
#endif
}

static void test_atomic_store(ZrTestAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void fail_test_process(const char *reason) {
    (void)fprintf(stderr, "gc_nested_mutation: %s\n", reason);
    (void)fflush(stderr);
    _Exit(EXIT_FAILURE);
}

static void trace_test_stage(const char *stage) {
    (void)fprintf(stderr, "gc_nested_mutation: %s\n", stage);
    (void)fflush(stderr);
}

static void test_sleep_ms(TZrUInt32 milliseconds) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(milliseconds);
#else
    struct timespec duration;
    duration.tv_sec = (time_t)(milliseconds / 1000u);
    duration.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static void nested_mutation_worker_run(ZrNestedMutationContext *context) {
    TZrBool outerLocked;
    TZrBool sawPause = ZR_FALSE;

    if (!ZrCore_GcDomain_MutatorEnter(&context->state)) {
        test_atomic_store(&context->failed, 1);
        test_atomic_store(&context->ready, 1);
        return;
    }

    trace_test_stage("worker before outer MutationBegin");
    outerLocked = ZrCore_GcDomain_MutationBegin(&context->state);
    trace_test_stage("worker after outer MutationBegin");
    test_atomic_store(&context->outerScopeLocked, outerLocked ? 1 : 0);
    test_atomic_store(&context->ready, 1);
    if (!outerLocked) {
        test_atomic_store(&context->failed, 1);
        ZrCore_GcDomain_MutatorLeave(&context->state);
        return;
    }

    for (TZrUInt32 attempt = 0u;
         attempt < ZR_GC_NESTED_MUTATION_WAIT_ATTEMPTS;
         ++attempt) {
        SZrGcDomainMutatorSnapshot snapshot;
        ZrCore_GcDomain_GetMutatorSnapshot(&context->state, &snapshot);
        if (snapshot.pauseRequested) {
            sawPause = ZR_TRUE;
            break;
        }
        if (test_atomic_load(&context->continueWithoutPause)) {
            break;
        }
        test_sleep_ms(1u);
    }

    if (sawPause || test_atomic_load(&context->continueWithoutPause)) {
        /* The existing integer-key write takes the production fast-miss path,
         * which opens nested MutationBegin scopes while this outer scope owns
         * the recursive mutation lock. */
        trace_test_stage("worker before Object_SetValue");
        ZrCore_Object_SetValue(
                &context->state, context->owner, &context->key, &context->value);
        trace_test_stage("worker after Object_SetValue");
        test_atomic_store(&context->mutationCompleted, 1);
    } else {
        test_atomic_store(&context->failed, 1);
    }

    trace_test_stage("worker before outer MutationEnd");
    ZrCore_GcDomain_MutationEnd(&context->state, outerLocked);
    trace_test_stage("worker after outer MutationEnd");
    ZrCore_GcDomain_MutatorLeave(&context->state);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI nested_mutation_worker_entry(LPVOID argument) {
    ZrNestedMutationContext *context = (ZrNestedMutationContext *)argument;
    nested_mutation_worker_run(context);
    test_atomic_store(&context->exited, 1);
    return 0u;
}

static TZrBool test_thread_start(
        ZrNestedMutationContext *context,
        ZrTestThread *outThread) {
    *outThread = CreateThread(
            ZR_NULL, 0u, nested_mutation_worker_entry, context, 0u, ZR_NULL);
    return (TZrBool)(*outThread != ZR_NULL);
}

static TZrBool test_thread_join(ZrTestThread thread) {
    DWORD waitResult = WaitForSingleObject(thread, 5000u);
    if (waitResult == WAIT_OBJECT_0) {
        CloseHandle(thread);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}
#else
static void *nested_mutation_worker_entry(void *argument) {
    ZrNestedMutationContext *context = (ZrNestedMutationContext *)argument;
    nested_mutation_worker_run(context);
    test_atomic_store(&context->exited, 1);
    return ZR_NULL;
}

static TZrBool test_thread_start(
        ZrNestedMutationContext *context,
        ZrTestThread *outThread) {
    return (TZrBool)(pthread_create(
                             outThread, ZR_NULL, nested_mutation_worker_entry, context) == 0);
}

static TZrBool test_thread_join(ZrTestThread thread) {
    return (TZrBool)(pthread_join(thread, ZR_NULL) == 0);
}
#endif

static TZrBool wait_for_worker_ready(ZrNestedMutationContext *context) {
    for (TZrUInt32 attempt = 0u; attempt < ZR_GC_NESTED_MUTATION_WAIT_ATTEMPTS; ++attempt) {
        if (test_atomic_load(&context->ready)) {
            return ZR_TRUE;
        }
        test_sleep_ms(1u);
    }
    return (TZrBool)test_atomic_load(&context->ready);
}

static TZrBool wait_for_worker_exit(ZrNestedMutationContext *context) {
    for (TZrUInt32 attempt = 0u;
         attempt < ZR_GC_NESTED_MUTATION_EXIT_WAIT_ATTEMPTS;
         ++attempt) {
        if (test_atomic_load(&context->exited)) {
            return ZR_TRUE;
        }
        test_sleep_ms(1u);
    }
    return (TZrBool)test_atomic_load(&context->exited);
}

static void finish_concurrent_major(TZrBool *outFinished) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;

    for (TZrUInt32 step = 0u;
         collector->concurrentMajorActive && step < 4096u;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }
    if (collector->concurrentMajorActive) {
        ZrCore_GarbageCollector_GcFull(g_state, ZR_FALSE);
    }
    *outFinished = (TZrBool)!collector->concurrentMajorActive;
}

void setUp(void) {
    trace_test_stage("setUp before State_Create");
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    trace_test_stage("setUp after State_Create");
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        trace_test_stage("tearDown before State_Destroy");
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
        trace_test_stage("tearDown after State_Destroy");
    }
}

static void test_nested_mutation_finishes_before_real_domain_pause(void) {
    ZrNestedMutationContext worker;
    ZrTestThread thread;
    SZrObject *owner = ZrCore_Object_New(g_state, ZR_NULL);
    SZrRawObject *ownerRaw = ZR_NULL;
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    SZrGcDomainPauseDiagnostic diagnostic;
    SZrGcDomainMutatorSnapshot snapshot;
    TZrBool ownerIgnored = ZR_FALSE;
    TZrBool majorStarted = ZR_FALSE;
    TZrBool workerAttached = ZR_FALSE;
    TZrBool workerStarted = ZR_FALSE;
    TZrBool workerReady = ZR_FALSE;
    TZrBool pauseAcquired = ZR_FALSE;
    TZrBool pauseWasActive = ZR_FALSE;
    TZrBool workerParkedAtPause = ZR_FALSE;
    TZrBool mutationCompletedDuringPause = ZR_FALSE;
    TZrBool workerJoined = ZR_FALSE;
    TZrBool workerFailed = ZR_FALSE;
    TZrBool mutationStoredExpected = ZR_FALSE;
    TZrBool majorFinished = ZR_FALSE;

    memset(&worker, 0, sizeof(worker));
    memset(&thread, 0, sizeof(thread));
    test_atomic_initialize(&worker.ready);
    test_atomic_initialize(&worker.failed);
    test_atomic_initialize(&worker.outerScopeLocked);
    test_atomic_initialize(&worker.mutationCompleted);
    test_atomic_initialize(&worker.continueWithoutPause);
    test_atomic_initialize(&worker.exited);
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(&snapshot, 0, sizeof(snapshot));

    trace_test_stage("owner initialization before");
    if (owner != ZR_NULL) {
        ZrCore_Object_Init(g_state, owner);
        ownerRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(owner);
        ZrCore_Value_InitAsInt(g_state, &worker.key, 17);
        ZrCore_Value_InitAsInt(g_state, &worker.value, 7001);
        {
            SZrTypeValue initialValue;
            ZrCore_Value_InitAsInt(g_state, &initialValue, 7000);
            ZrCore_Object_SetValue(g_state, owner, &worker.key, &initialValue);
        }
        ownerIgnored = ZrCore_GarbageCollector_IgnoreObject(g_state, ownerRaw);
    }
    trace_test_stage("owner initialization after");

    ZrCore_GarbageCollector_SetWorkerCount(g_state->global, 1u);
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    trace_test_stage("first GcStep before");
    ZrCore_GarbageCollector_GcStep(g_state);
    trace_test_stage("first GcStep after");
    majorStarted =
            collector->concurrentMajorActive &&
            collector->collectionPhase ==
                    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;

    worker.owner = owner;
    if (majorStarted && ownerIgnored) {
        ZrCore_RawObject_Construct(
                &worker.state.super, ZR_RAW_OBJECT_TYPE_THREAD);
        worker.state.global = g_state->global;
        workerAttached = ZrCore_GcDomain_MutatorAttach(g_state, &worker.state);
    }
    if (workerAttached) {
        workerStarted = test_thread_start(&worker, &thread);
    }
    if (workerStarted) {
        workerReady = wait_for_worker_ready(&worker);
        (void)fprintf(stderr,
                      "gc_nested_mutation: worker ready=%d failed=%d\n",
                      workerReady ? 1 : 0,
                      test_atomic_load(&worker.failed));
        (void)fflush(stderr);
        if (workerReady && !test_atomic_load(&worker.failed)) {
            trace_test_stage("StopTheWorldBegin before");
            pauseAcquired = ZrCore_GcDomain_StopTheWorldBegin(
                    g_state,
                    ZR_GC_NESTED_MUTATION_PAUSE_TIMEOUT_MS,
                    &diagnostic);
            if (pauseAcquired) {
                ZrCore_GcDomain_GetMutatorSnapshot(g_state, &snapshot);
                pauseWasActive = snapshot.pauseRequested;
                workerParkedAtPause = snapshot.parkedMutatorCount > 0u;
                mutationCompletedDuringPause =
                        (TZrBool)test_atomic_load(&worker.mutationCompleted);
                ZrCore_GcDomain_StopTheWorldEnd(g_state);
            }
            (void)fprintf(stderr,
                          "gc_nested_mutation: StopTheWorldBegin after acquired=%d timedOut=%d pause=%d parked=%d mutationCompleted=%d\n",
                          pauseAcquired ? 1 : 0,
                          diagnostic.timedOut ? 1 : 0,
                          pauseWasActive ? 1 : 0,
                          workerParkedAtPause ? 1 : 0,
                          mutationCompletedDuringPause ? 1 : 0);
            (void)fflush(stderr);
        }
        test_atomic_store(&worker.continueWithoutPause, 1);
        ZrCore_GcDomain_WakeMutators(&worker.state);
        trace_test_stage("worker join before exit wait");
        if (!wait_for_worker_exit(&worker)) {
            fail_test_process(
                    "worker did not exit before the bounded cleanup deadline; refusing to free live state");
        }
        trace_test_stage("thread join before");
        workerJoined = test_thread_join(thread);
        (void)fprintf(stderr,
                      "gc_nested_mutation: worker join after joined=%d failed=%d\n",
                      workerJoined ? 1 : 0,
                      test_atomic_load(&worker.failed));
        (void)fflush(stderr);
        if (!workerJoined) {
            fail_test_process(
                    "thread join failed after worker exit; refusing to continue cleanup");
        }
        workerFailed = (TZrBool)test_atomic_load(&worker.failed);
        ZrCore_GcDomain_MutatorDetach(&worker.state);
    } else if (workerAttached) {
        ZrCore_GcDomain_MutatorDetach(&worker.state);
    }

    if (workerJoined && owner != ZR_NULL) {
        trace_test_stage("Object_GetValue before");
        const SZrTypeValue *storedValue =
                ZrCore_Object_GetValue(g_state, owner, &worker.key);
        mutationStoredExpected = (TZrBool)(
                storedValue != ZR_NULL &&
                storedValue->type == ZR_VALUE_TYPE_INT64 &&
                storedValue->value.nativeObject.nativeInt64 == 7001);
        (void)fprintf(stderr,
                      "gc_nested_mutation: Object_GetValue after expected=%d\n",
                      mutationStoredExpected ? 1 : 0);
        (void)fflush(stderr);
    }

    trace_test_stage("major drain before");
    if (collector->concurrentMajorActive) {
        finish_concurrent_major(&majorFinished);
    } else {
        majorFinished = ZR_TRUE;
    }
    (void)fprintf(stderr,
                  "gc_nested_mutation: major drain after finished=%d active=%d\n",
                  majorFinished ? 1 : 0,
                  collector->concurrentMajorActive ? 1 : 0);
    (void)fflush(stderr);
    if (ownerIgnored) {
        (void)ZrCore_GarbageCollector_UnignoreObject(g_state->global, ownerRaw);
    }

    TEST_ASSERT_NOT_NULL(owner);
    TEST_ASSERT_TRUE(ownerIgnored);
    TEST_ASSERT_TRUE(majorStarted);
    TEST_ASSERT_TRUE(workerAttached);
    TEST_ASSERT_TRUE(workerStarted);
    TEST_ASSERT_TRUE(workerReady);
    TEST_ASSERT_TRUE(test_atomic_load(&worker.outerScopeLocked));
    TEST_ASSERT_TRUE(pauseAcquired);
    TEST_ASSERT_FALSE(diagnostic.timedOut);
    TEST_ASSERT_TRUE(pauseWasActive);
    TEST_ASSERT_TRUE(workerParkedAtPause);
    TEST_ASSERT_TRUE(mutationCompletedDuringPause);
    TEST_ASSERT_TRUE(workerJoined);
    TEST_ASSERT_FALSE(workerFailed);
    TEST_ASSERT_TRUE(test_atomic_load(&worker.mutationCompleted));
    TEST_ASSERT_TRUE(mutationStoredExpected);
    TEST_ASSERT_TRUE(majorFinished);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nested_mutation_finishes_before_real_domain_pause);
    return UNITY_END();
}
