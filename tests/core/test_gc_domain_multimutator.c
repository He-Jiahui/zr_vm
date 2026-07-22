#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"

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

typedef struct ZrMutatorWorkerContext {
    SZrState state;
    ZrTestAtomic ready;
    ZrTestAtomic failed;
    ZrTestAtomic stop;
    ZrTestAtomic progress;
    EZrGcNativeSafepointMode nativeMode;
    TZrBool nestedEnterOnPause;
    TZrUInt64 mutatorId;
} ZrMutatorWorkerContext;

static SZrState *g_state;

typedef struct ZrMutatorUnwindContext {
    TZrBool enteredExecution;
    TZrBool enteredNative;
} ZrMutatorUnwindContext;

static void mutator_unwind_try_body(SZrState *state, TZrPtr arguments) {
    ZrMutatorUnwindContext *context = (ZrMutatorUnwindContext *)arguments;

    context->enteredExecution = ZrCore_GcDomain_MutatorEnter(state);
    if (!context->enteredExecution) {
        return;
    }
    context->enteredNative = ZrCore_GcDomain_NativeEnter(
            state, ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL);
    if (!context->enteredNative) {
        return;
    }
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
}

static int test_atomic_load(ZrTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void test_atomic_store(ZrTestAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void test_atomic_increment(ZrTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedIncrement(value);
#else
    atomic_fetch_add_explicit(value, 1, memory_order_relaxed);
#endif
}

static void test_sleep_ms(TZrUInt32 milliseconds) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(milliseconds);
#else
    struct timespec duration;
    duration.tv_sec = (time_t)(milliseconds / 1000u);
    duration.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    nanosleep(&duration, ZR_NULL);
#endif
}

static void test_wait_ready(ZrMutatorWorkerContext *context) {
    TZrUInt32 attempts = 0u;
    while (!test_atomic_load(&context->ready) && attempts < 2000u) {
        test_sleep_ms(1u);
        attempts++;
    }
    TEST_ASSERT_TRUE(test_atomic_load(&context->ready));
}

static void mutator_worker_run(ZrMutatorWorkerContext *context) {
    if (!ZrCore_GcDomain_MutatorEnter(&context->state)) {
        test_atomic_store(&context->failed, 1);
        test_atomic_store(&context->ready, 1);
        return;
    }
    context->mutatorId = ZrCore_GcDomain_GetMutatorId(&context->state);
    if (context->nativeMode != ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
        if (!ZrCore_GcDomain_NativeEnter(&context->state, context->nativeMode)) {
            test_atomic_store(&context->failed, 1);
            ZrCore_GcDomain_MutatorLeave(&context->state);
            test_atomic_store(&context->ready, 1);
            return;
        }
    }
    test_atomic_store(&context->ready, 1);
    while (!test_atomic_load(&context->stop)) {
        if (context->nestedEnterOnPause) {
            SZrGcDomainMutatorSnapshot snapshot;
            ZrCore_GcDomain_GetMutatorSnapshot(&context->state, &snapshot);
            if (snapshot.pauseRequested) {
                if (!ZrCore_GcDomain_MutatorEnter(&context->state)) {
                    test_atomic_store(&context->failed, 1);
                    break;
                }
                ZrCore_GcDomain_MutatorLeave(&context->state);
                test_atomic_increment(&context->progress);
            }
        } else {
            test_atomic_increment(&context->progress);
            ZrCore_GcDomain_MutatorPoll(&context->state);
        }
    }
    if (context->nativeMode != ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) {
        ZrCore_GcDomain_NativeLeave(&context->state);
    }
    ZrCore_GcDomain_MutatorLeave(&context->state);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI mutator_worker_entry(LPVOID argument) {
    mutator_worker_run((ZrMutatorWorkerContext *)argument);
    return 0;
}

static ZrTestThread test_thread_start(ZrMutatorWorkerContext *context) {
    return CreateThread(ZR_NULL, 0u, mutator_worker_entry, context, 0u, ZR_NULL);
}

static void test_thread_join(ZrTestThread thread) {
    TEST_ASSERT_NOT_NULL(thread);
    TEST_ASSERT_EQUAL_UINT32(WAIT_OBJECT_0, WaitForSingleObject(thread, 5000u));
    CloseHandle(thread);
}
#else
static void *mutator_worker_entry(void *argument) {
    mutator_worker_run((ZrMutatorWorkerContext *)argument);
    return ZR_NULL;
}

static ZrTestThread test_thread_start(ZrMutatorWorkerContext *context) {
    ZrTestThread thread;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, ZR_NULL, mutator_worker_entry, context));
    return thread;
}

static void test_thread_join(ZrTestThread thread) {
    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, ZR_NULL));
}
#endif

static void worker_context_init(
        ZrMutatorWorkerContext *context,
        SZrState *ownerState,
        EZrGcNativeSafepointMode nativeMode) {
    memset(context, 0, sizeof(*context));
    ZrCore_RawObject_Construct(
            &context->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    context->state.global = ownerState->global;
    context->nativeMode = nativeMode;
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorAttach(
            ownerState, &context->state));
}

static void worker_context_stop(
        ZrMutatorWorkerContext *context,
        ZrTestThread thread) {
    test_atomic_store(&context->stop, 1);
    ZrCore_GcDomain_WakeMutators(&context->state);
    test_thread_join(thread);
    TEST_ASSERT_FALSE(test_atomic_load(&context->failed));
    ZrCore_GcDomain_MutatorDetach(&context->state);
}

static TZrBool collector_contains_object(
        const SZrGarbageCollector *collector,
        const SZrRawObject *expected) {
    const SZrRawObject *object = collector != ZR_NULL
                                        ? collector->gcObjectList
                                        : ZR_NULL;
    while (object != ZR_NULL) {
        if (object == expected) {
            return ZR_TRUE;
        }
        object = object->next;
    }
    return ZR_FALSE;
}

static SZrObject *create_plain_object(void) {
    SZrObject *object = ZrCore_Object_New(g_state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    return object;
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_domain_local_pause_parks_only_current_domain_mutators(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrMutatorWorkerContext localWorker;
    ZrMutatorWorkerContext otherWorker;
    ZrTestThread localThread;
    ZrTestThread otherThread;
    SZrGcDomainPauseDiagnostic diagnostic;
    SZrGcDomainMutatorSnapshot snapshot;
    int localProgress;
    int otherProgress;

    TEST_ASSERT_NOT_NULL(otherState);
    worker_context_init(&localWorker, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    worker_context_init(&otherWorker, otherState, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    localThread = test_thread_start(&localWorker);
    otherThread = test_thread_start(&otherWorker);
    test_wait_ready(&localWorker);
    test_wait_ready(&otherWorker);

    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_StopTheWorldBegin(g_state, 1000u, &diagnostic));
    TEST_ASSERT_FALSE(diagnostic.timedOut);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &snapshot);
    TEST_ASSERT_TRUE(snapshot.pauseRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.parkedMutatorCount);
    localProgress = test_atomic_load(&localWorker.progress);
    otherProgress = test_atomic_load(&otherWorker.progress);
    test_sleep_ms(20u);
    TEST_ASSERT_EQUAL_INT(localProgress, test_atomic_load(&localWorker.progress));
    TEST_ASSERT_TRUE(test_atomic_load(&otherWorker.progress) > otherProgress);
    ZrCore_GcDomain_StopTheWorldEnd(g_state);
    test_sleep_ms(10u);
    TEST_ASSERT_TRUE(test_atomic_load(&localWorker.progress) > localProgress);

    worker_context_stop(&localWorker, localThread);
    worker_context_stop(&otherWorker, otherThread);
    ZrTests_Runtime_State_Destroy(otherState);
}

static void test_no_safepoint_native_reports_exact_blocking_mutator(void) {
    ZrMutatorWorkerContext worker;
    ZrTestThread thread;
    SZrGcDomainPauseDiagnostic diagnostic;

    worker_context_init(
            &worker, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL);
    thread = test_thread_start(&worker);
    test_wait_ready(&worker);

    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_StopTheWorldBegin(g_state, 20u, &diagnostic));
    TEST_ASSERT_TRUE(diagnostic.timedOut);
    TEST_ASSERT_EQUAL_UINT64(worker.mutatorId, diagnostic.blockingMutatorId);
    TEST_ASSERT_EQUAL_INT(
            ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL,
            diagnostic.blockingNativeMode);

    worker_context_stop(&worker, thread);
}

static void test_blocking_detached_native_does_not_block_domain_pause(void) {
    ZrMutatorWorkerContext worker;
    ZrTestThread thread;
    SZrGcDomainPauseDiagnostic diagnostic;
    SZrGcDomainMutatorSnapshot snapshot;

    worker_context_init(
            &worker, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED);
    thread = test_thread_start(&worker);
    test_wait_ready(&worker);

    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_StopTheWorldBegin(g_state, 100u, &diagnostic));
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.blockingDetachedMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.parkedMutatorCount);
    ZrCore_GcDomain_StopTheWorldEnd(g_state);

    worker_context_stop(&worker, thread);
}

static void test_mutator_registry_tracks_attach_detach_and_pause_epoch(void) {
    ZrMutatorWorkerContext worker;
    ZrTestThread thread;
    SZrGcDomainMutatorSnapshot before;
    SZrGcDomainMutatorSnapshot paused;
    SZrGcDomainMutatorSnapshot after;

    worker_context_init(&worker, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &before);
    TEST_ASSERT_EQUAL_UINT32(2u, before.registeredMutatorCount);
    thread = test_thread_start(&worker);
    test_wait_ready(&worker);

    TEST_ASSERT_TRUE(ZrCore_GcDomain_StopTheWorldBegin(g_state, 1000u, ZR_NULL));
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &paused);
    TEST_ASSERT_TRUE(paused.safepointEpoch > before.safepointEpoch);
    TEST_ASSERT_EQUAL_UINT32(1u, paused.parkedMutatorCount);
    ZrCore_GcDomain_StopTheWorldEnd(g_state);

    worker_context_stop(&worker, thread);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &after);
    TEST_ASSERT_EQUAL_UINT32(1u, after.registeredMutatorCount);
    TEST_ASSERT_FALSE(after.pauseRequested);
}

static void test_registered_mutator_publishes_vm_and_aot_roots(void) {
    ZrMutatorWorkerContext mutator;
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    SZrObject *vmObject = create_plain_object();
    SZrObject *aotObject = create_plain_object();
    SZrRawObject *oldVmObject = ZR_CAST_RAW_OBJECT_AS_SUPER(vmObject);
    SZrRawObject *oldAotObject = ZR_CAST_RAW_OBJECT_AS_SUPER(aotObject);
    SZrTypeValueOnStack aotSlot;
    SZrAotGcRootSlot rootSlot;
    SZrAotGcRootMap rootMap;
    SZrAotGcRootFrame rootFrame;

    worker_context_init(
            &mutator, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    ZrCore_Value_InitAsRawObject(
            g_state, &mutator.state.currentException, oldVmObject);
    mutator.state.hasCurrentException = ZR_TRUE;
    ZrCore_Value_InitAsRawObject(g_state, &aotSlot.value, oldAotObject);
    aotSlot.toBeClosedValueOffset = 0u;
    mutator.state.stackBase.valuePointer = &aotSlot;
    mutator.state.stackTop.valuePointer = &aotSlot;
    mutator.state.stackTail.valuePointer = &aotSlot + 1;
    memset(&rootSlot, 0, sizeof(rootSlot));
    rootSlot.locationKind =
            (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET;
    rootMap.rootCount = 1u;
    rootMap.roots = &rootSlot;
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(
            &mutator.state, &rootFrame, &aotSlot, &rootMap));

    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    ZrCore_GarbageCollector_GcStep(g_state);

    TEST_ASSERT_TRUE(mutator.state.currentException.isGarbageCollectable);
    TEST_ASSERT_TRUE(aotSlot.value.isGarbageCollectable);
    TEST_ASSERT_TRUE(collector_contains_object(
            collector, mutator.state.currentException.value.object));
    TEST_ASSERT_TRUE(collector_contains_object(
            collector, aotSlot.value.value.object));
    TEST_ASSERT_TRUE(
            mutator.state.currentException.value.object == oldVmObject ||
            oldVmObject->garbageCollectMark.forwardingAddress ==
                    mutator.state.currentException.value.object);
    TEST_ASSERT_TRUE(
            aotSlot.value.value.object == oldAotObject ||
            oldAotObject->garbageCollectMark.forwardingAddress ==
                    aotSlot.value.value.object);

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(
            &mutator.state, &rootFrame));
    ZrCore_Value_ResetAsNull(&mutator.state.currentException);
    mutator.state.hasCurrentException = ZR_FALSE;
    mutator.state.stackBase.valuePointer = ZR_NULL;
    mutator.state.stackTop.valuePointer = ZR_NULL;
    mutator.state.stackTail.valuePointer = ZR_NULL;
    ZrCore_GcDomain_MutatorDetach(&mutator.state);
}

static void test_same_domain_mutator_uses_shared_write_barrier_domain(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrMutatorWorkerContext mutator;
    SZrObject *owner = create_plain_object();
    SZrObject *localTarget = create_plain_object();
    SZrObject *foreignTarget;
    SZrTypeValue localValue;
    SZrTypeValue foreignValue;

    TEST_ASSERT_NOT_NULL(otherState);
    foreignTarget = ZrCore_Object_New(otherState, ZR_NULL);
    TEST_ASSERT_NOT_NULL(foreignTarget);
    ZrCore_Object_Init(otherState, foreignTarget);
    worker_context_init(
            &mutator, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);

    ZR_GC_SET_REFERENCED(ZR_CAST_RAW_OBJECT_AS_SUPER(owner));
    ZrCore_Value_InitAsRawObject(
            g_state, &localValue, ZR_CAST_RAW_OBJECT_AS_SUPER(localTarget));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ValidateValueWrite(
            &mutator.state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(owner),
            &localValue));
    ZrCore_Value_Barrier(
            &mutator.state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner), &localValue);
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkWaitToScan(
            ZR_CAST_RAW_OBJECT_AS_SUPER(localTarget)));

    ZrCore_Value_InitAsRawObject(
            otherState,
            &foreignValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(foreignTarget));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_ValidateValueWrite(
            &mutator.state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(owner),
            &foreignValue));
    ZrCore_Value_Barrier(
            &mutator.state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner), &foreignValue);
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkInited(
            ZR_CAST_RAW_OBJECT_AS_SUPER(foreignTarget)));

    ZrCore_GcDomain_MutatorDetach(&mutator.state);
    ZrTests_Runtime_State_Destroy(otherState);
}

static void test_native_scope_temporarily_enters_inactive_main_mutator(void) {
    SZrGcDomainMutatorSnapshot before;
    SZrGcDomainMutatorSnapshot entered;
    SZrGcDomainMutatorSnapshot after;

    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &before);
    TEST_ASSERT_EQUAL_UINT32(0u, before.runningMutatorCount);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(
            g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE));
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &entered);
    TEST_ASSERT_EQUAL_UINT32(1u, entered.runningMutatorCount);
    ZrCore_GcDomain_NativeLeave(g_state);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &after);
    TEST_ASSERT_EQUAL_UINT32(0u, after.runningMutatorCount);
}

static void test_nested_vm_mutator_scope_leaves_only_at_outer_boundary(void) {
    SZrGcDomainMutatorSnapshot nested;
    SZrGcDomainMutatorSnapshot innerLeft;
    SZrGcDomainMutatorSnapshot outerLeft;

    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorEnter(g_state));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorEnter(g_state));
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &nested);
    TEST_ASSERT_EQUAL_UINT32(1u, nested.runningMutatorCount);
    ZrCore_GcDomain_MutatorLeave(g_state);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &innerLeft);
    TEST_ASSERT_EQUAL_UINT32(1u, innerLeft.runningMutatorCount);
    ZrCore_GcDomain_MutatorLeave(g_state);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &outerLeft);
    TEST_ASSERT_EQUAL_UINT32(0u, outerLeft.runningMutatorCount);
}

static void test_nested_vm_enter_parks_at_active_pause_boundary(void) {
    ZrMutatorWorkerContext worker;
    ZrTestThread thread;
    SZrGcDomainMutatorSnapshot paused;
    TZrBool pauseSucceeded;

    worker_context_init(
            &worker, g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    worker.nestedEnterOnPause = ZR_TRUE;
    memset(&paused, 0, sizeof(paused));
    thread = test_thread_start(&worker);
    test_wait_ready(&worker);

    pauseSucceeded = ZrCore_GcDomain_StopTheWorldBegin(g_state, 200u, ZR_NULL);
    if (pauseSucceeded) {
        ZrCore_GcDomain_GetMutatorSnapshot(g_state, &paused);
        ZrCore_GcDomain_StopTheWorldEnd(g_state);
    }
    while (test_atomic_load(&worker.progress) == 0) {
        test_sleep_ms(1u);
    }

    worker_context_stop(&worker, thread);
    TEST_ASSERT_TRUE(pauseSucceeded);
    TEST_ASSERT_EQUAL_UINT32(1u, paused.parkedMutatorCount);
}

static void test_native_critical_scope_cannot_collect_its_own_domain(void) {
    SZrGcDomainPauseDiagnostic diagnostic;

    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(
            g_state, ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL));
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_FALSE(ZrCore_GcDomain_StopTheWorldBegin(
            g_state, 20u, &diagnostic));
    TEST_ASSERT_TRUE(diagnostic.timedOut);
    TEST_ASSERT_EQUAL_UINT64(
            ZrCore_GcDomain_GetMutatorId(g_state), diagnostic.blockingMutatorId);
    TEST_ASSERT_EQUAL_INT(
            ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL,
            diagnostic.blockingNativeMode);
    ZrCore_GcDomain_NativeLeave(g_state);
}

static void test_native_unwind_resets_abandoned_vm_and_native_scopes(void) {
    ZrMutatorUnwindContext context;
    SZrGcDomainMutatorSnapshot afterUnwind;
    EZrThreadStatus status;

    memset(&context, 0, sizeof(context));
    status = ZrCore_Exception_TryRun(
            g_state, mutator_unwind_try_body, &context);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &afterUnwind);

    TEST_ASSERT_TRUE(context.enteredExecution);
    TEST_ASSERT_TRUE(context.enteredNative);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_EQUAL_UINT32(0u, afterUnwind.runningMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, afterUnwind.noSafepointCriticalMutatorCount);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorEnter(g_state));
    ZrCore_GcDomain_MutatorLeave(g_state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_domain_local_pause_parks_only_current_domain_mutators);
    RUN_TEST(test_no_safepoint_native_reports_exact_blocking_mutator);
    RUN_TEST(test_blocking_detached_native_does_not_block_domain_pause);
    RUN_TEST(test_mutator_registry_tracks_attach_detach_and_pause_epoch);
    RUN_TEST(test_registered_mutator_publishes_vm_and_aot_roots);
    RUN_TEST(test_same_domain_mutator_uses_shared_write_barrier_domain);
    RUN_TEST(test_native_scope_temporarily_enters_inactive_main_mutator);
    RUN_TEST(test_nested_vm_mutator_scope_leaves_only_at_outer_boundary);
    RUN_TEST(test_nested_vm_enter_parks_at_active_pause_boundary);
    RUN_TEST(test_native_critical_scope_cannot_collect_its_own_domain);
    RUN_TEST(test_native_unwind_resets_abandoned_vm_and_native_scopes);
    return UNITY_END();
}
