#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include "harness/runtime_support.h"
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

typedef struct ZrConcurrentMutatorContext {
    /* worker 的 state/对象输入由测试栈持有；主线程通过 release/acquire
     * 原子开关发布，停机后必须 join 再 detach。 */
    SZrState state;
    ZrTestAtomic ready;
    ZrTestAtomic stop;
    ZrTestAtomic failed;
    ZrTestAtomic sawPause;
    ZrTestAtomic progress;
    ZrTestAtomic mutate;
    ZrTestAtomic mutationCount;
    SZrObject *mutationOwner;
    SZrTypeValue mutationKey;
    SZrTypeValue mutationValues[2];
} ZrConcurrentMutatorContext;

static SZrState *g_state;

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

static void concurrent_mutator_run(ZrConcurrentMutatorContext *context) {
    if (!ZrCore_GcDomain_MutatorEnter(&context->state)) {
        test_atomic_store(&context->failed, 1);
        test_atomic_store(&context->ready, 1);
        return;
    }
    test_atomic_store(&context->ready, 1);
    while (!test_atomic_load(&context->stop)) {
        SZrGcDomainMutatorSnapshot snapshot;

        ZrCore_GcDomain_GetMutatorSnapshot(&context->state, &snapshot);
        if (snapshot.pauseRequested) {
            test_atomic_store(&context->sawPause, 1);
        }
        ZrCore_GcDomain_MutatorPoll(&context->state);
        if (test_atomic_load(&context->mutate) &&
            context->mutationOwner != ZR_NULL) {
            int mutationIndex = test_atomic_load(&context->mutationCount) & 1;
            ZrCore_Object_SetValue(
                    &context->state,
                    context->mutationOwner,
                    &context->mutationKey,
                    &context->mutationValues[mutationIndex]);
            test_atomic_increment(&context->mutationCount);
        }
        test_atomic_increment(&context->progress);
    }
    ZrCore_GcDomain_MutatorLeave(&context->state);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI concurrent_mutator_entry(LPVOID argument) {
    concurrent_mutator_run((ZrConcurrentMutatorContext *)argument);
    return 0u;
}

static ZrTestThread test_thread_start(ZrConcurrentMutatorContext *context) {
    ZrTestThread thread = CreateThread(
            ZR_NULL, 0u, concurrent_mutator_entry, context, 0u, ZR_NULL);
    TEST_ASSERT_NOT_NULL(thread);
    return thread;
}

static void test_thread_join(ZrTestThread thread) {
    TEST_ASSERT_EQUAL_UINT32(WAIT_OBJECT_0, WaitForSingleObject(thread, 5000u));
    CloseHandle(thread);
}
#else
static void *concurrent_mutator_entry(void *argument) {
    concurrent_mutator_run((ZrConcurrentMutatorContext *)argument);
    return ZR_NULL;
}

static ZrTestThread test_thread_start(ZrConcurrentMutatorContext *context) {
    ZrTestThread thread;
    TEST_ASSERT_EQUAL_INT(
            0, pthread_create(&thread, ZR_NULL, concurrent_mutator_entry, context));
    return thread;
}

static void test_thread_join(ZrTestThread thread) {
    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, ZR_NULL));
}
#endif

static void concurrent_mutator_start(
        SZrState *domainState,
        ZrConcurrentMutatorContext *context,
        ZrTestThread *outThread) {
    memset(context, 0, sizeof(*context));
    ZrCore_RawObject_Construct(
            &context->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    context->state.global = domainState->global;
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorAttach(
            domainState, &context->state));
    *outThread = test_thread_start(context);
    for (TZrUInt32 attempt = 0u;
         !test_atomic_load(&context->ready) && attempt < 2000u;
         ++attempt) {
        test_sleep_ms(1u);
    }
    /* BUG: ready/failed 断言在已启动线程的 join 之前，失败时 Unity
     * longjmp 会丢失 thread 及栈上 context，tearDown 仍销毁其 global。 */
    TEST_ASSERT_TRUE(test_atomic_load(&context->ready));
    TEST_ASSERT_FALSE(test_atomic_load(&context->failed));
}

static void concurrent_mutator_stop(
        ZrConcurrentMutatorContext *context,
        ZrTestThread thread) {
    test_atomic_store(&context->stop, 1);
    ZrCore_GcDomain_WakeMutators(&context->state);
    test_thread_join(thread);
    /* BUG: 这里的失败断言会跳过 MutatorDetach，domain 注册表继续
     * 保存测试栈上的 state 地址。 */
    TEST_ASSERT_FALSE(test_atomic_load(&context->failed));
    ZrCore_GcDomain_MutatorDetach(&context->state);
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

static void test_major_step_enters_concurrent_mark_without_finishing_cycle(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;

    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);

    ZrCore_GarbageCollector_GcStep(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &snapshot);

    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT,
            snapshot.collectionPhase);
    TEST_ASSERT_EQUAL_UINT64(0u, snapshot.majorCollectionCount);
}

static void test_concurrent_major_mark_slices_finish_one_collection(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;
    TZrSize stepCount = 0u;

    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);

    do {
        ZrCore_GarbageCollector_GcStep(g_state);
        ZrCore_GarbageCollector_GetStatsSnapshot(
                g_state->global, &snapshot);
        stepCount++;
    } while (snapshot.collectionPhase !=
                     ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE &&
             stepCount < 1024u);

    TEST_ASSERT_LESS_THAN_UINT64(1024u, stepCount);
    TEST_ASSERT_FALSE(
            g_state->global->garbageCollector->concurrentMajorActive);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.majorCollectionCount);
}

static void test_concurrent_mark_slice_does_not_pause_same_domain_mutator(void) {
    /* concurrent slice 期间同域 worker 应继续推进；仅 remark 可要求
     * safepoint，故先读取 epoch 再执行一个 slice 作比较。 */
    ZrConcurrentMutatorContext mutator;
    ZrTestThread thread;
    SZrGarbageCollectorStatsSnapshot snapshot;
    SZrGcDomainMutatorSnapshot beforeSlice;
    SZrGcDomainMutatorSnapshot afterSlice;

    for (TZrUInt32 index = 0u; index < 64u; ++index) {
        SZrObject *object = ZrCore_Object_New(g_state, ZR_NULL);
        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Object_Init(g_state, object);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
                g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    }
    concurrent_mutator_start(g_state, &mutator, &thread);
    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    test_atomic_store(&mutator.sawPause, 0);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &beforeSlice);

    ZrCore_GarbageCollector_GcStep(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_state->global, &snapshot);
    ZrCore_GcDomain_GetMutatorSnapshot(g_state, &afterSlice);

    /* BUG: 下列断言先于 concurrent_mutator_stop；一旦回归触发失败，
     * worker 仍运行并借用栈上 context，tearDown 会释放其 global。 */
    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT,
            snapshot.collectionPhase);
    TEST_ASSERT_EQUAL_UINT64(
            beforeSlice.safepointEpoch, afterSlice.safepointEpoch);
    TEST_ASSERT_FALSE(test_atomic_load(&mutator.sawPause));
    TEST_ASSERT_TRUE(test_atomic_load(&mutator.progress) > 0);
    concurrent_mutator_stop(&mutator, thread);
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

static void finish_concurrent_major(void) {
    for (TZrUInt32 step = 0u;
         g_state->global->garbageCollector->concurrentMajorActive &&
         step < 4096u;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }
    TEST_ASSERT_FALSE(
            g_state->global->garbageCollector->concurrentMajorActive);
}

static void run_concurrent_major_slice_max_objects_case(
        TZrUInt64 maxObjectsLimit,
        TZrSize expectedReferencedCount) {
    enum {
        fixtureObjectCount = 16,
        maxCleanupSteps = 4096
    };
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    SZrObject *preexistingRoot = ZR_NULL;
    SZrRawObject *preexistingRootRaw = ZR_NULL;
    SZrObject *fixtureObjects[fixtureObjectCount] = {ZR_NULL};
    SZrRawObject *fixtureRaws[fixtureObjectCount] = {ZR_NULL};
    TZrBool ignoredByTest[fixtureObjectCount] = {ZR_FALSE};
    SZrGcBudget budget;
    TZrSize ignoredCountBefore = collector->ignoredObjectCount;
    TZrSize createdCount = 0u;
    TZrSize liveAfterBeginCount = 0u;
    TZrSize initialGrayRootCount = 0u;
    TZrSize grayBeforeSliceCount = 0u;
    TZrSize grayHeadPrefixCount = 0u;
    TZrSize referencedAfterSliceCount = 0u;
    TZrSize waitingAfterSliceCount = 0u;
    TZrSize otherMarkStatusAfterSliceCount = 0u;
    TZrSize finishStepCount = 0u;
    TZrSize ignoredCountAfterCleanup;
    TZrBool budgetConfigured;
    TZrBool preexistingRootIgnored = ZR_FALSE;
    TZrBool preexistingRootQueued = ZR_FALSE;
    TZrBool majorBegan = ZR_FALSE;
    TZrBool sliceRan = ZR_FALSE;
    TZrBool majorFinished = ZR_FALSE;
    TZrBool fixtureTailMatchesInitialRootHead = ZR_FALSE;
    const SZrRawObject *initialGrayRootHead = ZR_NULL;

    ZrCore_GcBudget_Init(&budget);
    budget.maxObjects = maxObjectsLimit;
    /* SetBudget requires one configured dimension. For maxObjects=0, this
     * unused work limit keeps the budget valid; mark_slice only consumes the
     * object cap and leaves budget-step telemetry untouched. */
    if (maxObjectsLimit == 0u) {
        budget.maxWorkUnits = 1u;
    }
    budgetConfigured = ZrCore_Gc_SetBudget(g_state, &budget);

    ZrCore_GarbageCollector_SetWorkerCount(g_state->global, 1u);
    if (budgetConfigured) {
        preexistingRoot = ZrCore_Object_New(g_state, ZR_NULL);
        if (preexistingRoot != ZR_NULL) {
            ZrCore_Object_Init(g_state, preexistingRoot);
            preexistingRootRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(preexistingRoot);
            preexistingRootIgnored = ZrCore_GarbageCollector_IgnoreObject(
                    g_state, preexistingRootRaw);
        }
    }

    if (budgetConfigured && preexistingRootIgnored) {
        collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_GarbageCollector_ScheduleCollection(
                g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
        ZrCore_GarbageCollector_GcStep(g_state);
        majorBegan = collector->concurrentMajorActive &&
                     collector->collectionPhase ==
                             ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
    }
    if (majorBegan) {
        const SZrRawObject *grayCursor = collector->waitToScanObjectList;
        initialGrayRootHead = grayCursor;
        while (grayCursor != ZR_NULL) {
            if (grayCursor == preexistingRootRaw) {
                preexistingRootQueued = ZR_TRUE;
            }
            initialGrayRootCount++;
            grayCursor = grayCursor->gcList;
        }
    }

    /* Install roots after the initial snapshot. Each successful IgnoreObject
     * prepends one gray object, placing this fixture ahead of existing roots. */
    if (majorBegan) {
        for (TZrSize index = 0u; index < fixtureObjectCount; ++index) {
            fixtureObjects[index] = ZrCore_Object_New(g_state, ZR_NULL);
            if (fixtureObjects[index] == ZR_NULL) {
                break;
            }
            ZrCore_Object_Init(g_state, fixtureObjects[index]);
            fixtureRaws[index] = ZR_CAST_RAW_OBJECT_AS_SUPER(fixtureObjects[index]);
            createdCount++;
            if (collector_contains_object(collector, fixtureRaws[index])) {
                liveAfterBeginCount++;
            }
            ignoredByTest[index] = ZrCore_GarbageCollector_IgnoreObject(
                    g_state, fixtureRaws[index]);
            if (!ignoredByTest[index]) {
                break;
            }
        }
    }

    {
        const SZrRawObject *grayCursor = collector->waitToScanObjectList;
        for (TZrSize index = 0u; index < createdCount; ++index) {
            const SZrRawObject *expectedHead = fixtureRaws[createdCount - index - 1u];
            if (ZrCore_RawObject_IsWaitToScan(fixtureRaws[index])) {
                grayBeforeSliceCount++;
            }
            if (grayCursor != expectedHead) {
                break;
            }
            grayHeadPrefixCount++;
            grayCursor = grayCursor->gcList;
        }
        fixtureTailMatchesInitialRootHead =
                grayCursor == initialGrayRootHead;
    }

    if (majorBegan && createdCount == fixtureObjectCount &&
        liveAfterBeginCount == fixtureObjectCount &&
        initialGrayRootCount > 0u &&
        grayBeforeSliceCount == fixtureObjectCount &&
        grayHeadPrefixCount == fixtureObjectCount &&
        fixtureTailMatchesInitialRootHead &&
        budgetConfigured) {
        ZrCore_GarbageCollector_GcStep(g_state);
        sliceRan = ZR_TRUE;
        for (TZrSize index = 0u; index < fixtureObjectCount; ++index) {
            if (ZrCore_RawObject_IsMarkReferenced(fixtureRaws[index])) {
                referencedAfterSliceCount++;
            } else if (ZrCore_RawObject_IsWaitToScan(fixtureRaws[index])) {
                waitingAfterSliceCount++;
            } else {
                otherMarkStatusAfterSliceCount++;
            }
        }
    }

    while (collector->concurrentMajorActive &&
           finishStepCount < (TZrSize)maxCleanupSteps) {
        ZrCore_GarbageCollector_GcStep(g_state);
        finishStepCount++;
    }
    majorFinished = !collector->concurrentMajorActive;
    if (!majorFinished && collector->concurrentMajorActive) {
        /* Keep teardown safe even if the bounded drain unexpectedly stalls. */
        ZrCore_GarbageCollector_GcFull(g_state, ZR_FALSE);
    }

    for (TZrSize index = 0u; index < createdCount; ++index) {
        if (ignoredByTest[index]) {
            (void)ZrCore_GarbageCollector_UnignoreObject(
                    g_state->global, fixtureRaws[index]);
        }
    }
    if (preexistingRootIgnored) {
        (void)ZrCore_GarbageCollector_UnignoreObject(
                g_state->global, preexistingRootRaw);
    }
    ignoredCountAfterCleanup = collector->ignoredObjectCount;

    TEST_ASSERT_TRUE(budgetConfigured);
    TEST_ASSERT_TRUE(preexistingRootIgnored);
    TEST_ASSERT_TRUE(majorBegan);
    TEST_ASSERT_EQUAL_UINT64(fixtureObjectCount, createdCount);
    TEST_ASSERT_EQUAL_UINT64(fixtureObjectCount, liveAfterBeginCount);
    TEST_ASSERT_TRUE(initialGrayRootCount > 0u);
    TEST_ASSERT_TRUE(preexistingRootQueued);
    TEST_ASSERT_EQUAL_UINT64(fixtureObjectCount, grayBeforeSliceCount);
    TEST_ASSERT_EQUAL_UINT64(fixtureObjectCount, grayHeadPrefixCount);
    TEST_ASSERT_TRUE(fixtureTailMatchesInitialRootHead);
    TEST_ASSERT_TRUE(sliceRan);
    TEST_ASSERT_EQUAL_UINT64(expectedReferencedCount, referencedAfterSliceCount);
    TEST_ASSERT_EQUAL_UINT64(
            fixtureObjectCount - expectedReferencedCount,
            waitingAfterSliceCount);
    TEST_ASSERT_EQUAL_UINT64(0u, otherMarkStatusAfterSliceCount);
    TEST_ASSERT_TRUE(majorFinished);
    TEST_ASSERT_EQUAL_UINT64(ignoredCountBefore, ignoredCountAfterCleanup);
}

static void test_concurrent_major_slice_honors_max_objects(void) {
    run_concurrent_major_slice_max_objects_case(1u, 1u);
}

static void test_concurrent_major_slice_zero_max_objects_keeps_caller_limit(void) {
    run_concurrent_major_slice_max_objects_case(0u, 8u);
}

static void test_concurrent_major_slice_large_max_objects_keeps_caller_limit(void) {
    run_concurrent_major_slice_max_objects_case(64u, 8u);
}

static void test_concurrent_barrier_keeps_target_written_from_black_owner(void) {
    /* owner 已被 major 标黑后才写入 target；若并发写屏障漏标，
     * 完成收集时 target 会从 gcObjectList 消失。 */
    SZrObject *owner = ZrCore_Object_New(g_state, ZR_NULL);
    SZrObject *target = ZrCore_Object_New(g_state, ZR_NULL);
    SZrRawObject *targetRaw;
    SZrGarbageCollectorStatsSnapshot snapshot;
    SZrTypeValue key;
    SZrTypeValue targetValue;

    TEST_ASSERT_NOT_NULL(owner);
    TEST_ASSERT_NOT_NULL(target);
    ZrCore_Object_Init(g_state, owner);
    ZrCore_Object_Init(g_state, target);
    targetRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(target);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner)));
    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);

    for (TZrUInt32 step = 0u;
         !ZrCore_RawObject_IsMarkReferenced(
                 ZR_CAST_RAW_OBJECT_AS_SUPER(owner)) &&
         step < 1024u;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkReferenced(
            ZR_CAST_RAW_OBJECT_AS_SUPER(owner)));
    ZrCore_Value_InitAsInt(g_state, &key, 17);
    ZrCore_Value_InitAsRawObject(g_state, &targetValue, targetRaw);
    ZrCore_Object_SetValue(g_state, owner, &key, &targetValue);

    finish_concurrent_major();
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_state->global, &snapshot);

    TEST_ASSERT_TRUE(collector_contains_object(
            g_state->global->garbageCollector, targetRaw));
    TEST_ASSERT_TRUE(snapshot.concurrentBarrierCount > 0u);
}

static void test_concurrent_marker_and_mutator_serialize_object_storage(void) {
    ZrConcurrentMutatorContext mutator;
    ZrTestThread thread;
    SZrObject *owner = ZrCore_Object_New(g_state, ZR_NULL);

    TEST_ASSERT_NOT_NULL(owner);
    ZrCore_Object_Init(g_state, owner);
    for (TZrInt32 index = 0; index < 2048; ++index) {
        SZrTypeValue key;
        SZrTypeValue value;

        ZrCore_Value_InitAsInt(g_state, &key, index);
        ZrCore_Value_InitAsInt(g_state, &value, index * 3);
        ZrCore_Object_SetValue(g_state, owner, &key, &value);
    }
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(owner)));
    concurrent_mutator_start(g_state, &mutator, &thread);
    mutator.mutationOwner = owner;
    ZrCore_Value_InitAsInt(g_state, &mutator.mutationKey, 1024);
    ZrCore_Value_InitAsInt(g_state, &mutator.mutationValues[0], 7001);
    ZrCore_Value_InitAsInt(g_state, &mutator.mutationValues[1], 7002);
    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    test_atomic_store(&mutator.mutate, 1);
    for (TZrUInt32 attempt = 0u;
         test_atomic_load(&mutator.mutationCount) == 0 && attempt < 2000u;
         ++attempt) {
        test_sleep_ms(1u);
    }
    /* BUG: 回归导致 worker 无进度时，这个断言在 stop/join 之前跳转，
     * worker 仍会读取测试栈上的 context 和后续被释放的 global。 */
    TEST_ASSERT_TRUE(test_atomic_load(&mutator.mutationCount) > 0);

    finish_concurrent_major();
    test_atomic_store(&mutator.mutate, 0);

    concurrent_mutator_stop(&mutator, thread);
}

static void test_concurrent_major_telemetry_is_attributed_to_domain(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;
    SZrGcDomainIdentity identity = ZrCore_GcDomain_GetIdentity(g_state);

    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    finish_concurrent_major();
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_state->global, &snapshot);

    TEST_ASSERT_EQUAL_UINT64(identity.id, snapshot.domainId);
    TEST_ASSERT_EQUAL_UINT32(identity.generation, snapshot.domainGeneration);
    TEST_ASSERT_FALSE(snapshot.concurrentMajorActive);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.concurrentMajorCycleCount);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.concurrentMajorInitialPauseCount);
    TEST_ASSERT_TRUE(snapshot.concurrentMajorInitialPauseTotalUs > 0u);
    TEST_ASSERT_TRUE(snapshot.concurrentMajorMarkSliceCount > 0u);
    TEST_ASSERT_TRUE(snapshot.concurrentMajorMarkTotalUs > 0u);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.concurrentMajorRemarkPauseCount);
    TEST_ASSERT_TRUE(snapshot.concurrentMajorRemarkPauseTotalUs > 0u);
    TEST_ASSERT_TRUE(snapshot.safepointWaitCount >= 2u);
    TEST_ASSERT_TRUE(snapshot.safepointWaitTotalUs > 0u);
}

static void test_compaction_is_deferred_when_pause_budget_is_zero(void) {
    SZrObject *first = ZrCore_Object_New(g_state, ZR_NULL);
    SZrObject *second = ZrCore_Object_New(g_state, ZR_NULL);
    SZrRawObject *firstRaw;
    SZrRawObject *secondRaw;
    SZrGarbageCollector *collector =
            g_state->global->garbageCollector;
    SZrGarbageCollectorStatsSnapshot snapshot;
    SZrGcRootHandle firstRoot;
    SZrGcRootHandle secondRoot;

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    ZrCore_Object_Init(g_state, first);
    ZrCore_Object_Init(g_state, second);
    firstRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(first);
    secondRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(second);
    ZrCore_RawObject_SetStorageKind(
            firstRaw, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
    ZrCore_RawObject_SetStorageKind(
            secondRaw, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
    ZrCore_RawObject_SetRegionKind(
            firstRaw, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
    ZrCore_RawObject_SetRegionKind(
            secondRaw, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
    firstRaw->garbageCollectMark.regionId = 0xff01u;
    secondRaw->garbageCollectMark.regionId = 0xff02u;
    firstRaw->garbageCollectMark.regionDescriptorIndex = ZR_MAX_SIZE;
    secondRaw->garbageCollectMark.regionDescriptorIndex = ZR_MAX_SIZE;
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_state, firstRaw, &firstRoot));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            g_state, secondRaw, &secondRoot));
    collector->fragmentationCompactThreshold = 0u;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_SetPauseBudgetUs(
            g_state->global, 0u, 1000u);
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    finish_concurrent_major();
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_state->global, &snapshot);

    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.compactDeferredCount);
    TEST_ASSERT_EQUAL_UINT64(0u, snapshot.compactPauseCount);
    ZrCore_GcRootHandle_Release(g_state, &firstRoot);
    ZrCore_GcRootHandle_Release(g_state, &secondRoot);
}

static void test_remark_pause_is_domain_local(void) {
    SZrState *otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrConcurrentMutatorContext otherMutator;
    ZrTestThread otherThread;
    int progressBeforeRemark;
    int progressAfterRemark;

    TEST_ASSERT_NOT_NULL(otherState);
    for (TZrUInt32 index = 0u; index < 8192u; ++index) {
        SZrObject *object = ZrCore_Object_New(g_state, ZR_NULL);
        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Object_Init(g_state, object);
    }
    concurrent_mutator_start(otherState, &otherMutator, &otherThread);
    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    /* BUG: markDrained 若始终不成立，该循环没有上限，且其它 domain
     * worker 仍运行；测试套件无法以断言失败方式退出。 */
    while (!g_state->global->garbageCollector->concurrentMajorMarkDrained) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }

    progressBeforeRemark = test_atomic_load(&otherMutator.progress);
    ZrCore_GarbageCollector_GcStep(g_state);
    progressAfterRemark = test_atomic_load(&otherMutator.progress);

    TEST_ASSERT_TRUE(progressAfterRemark > progressBeforeRemark);
    concurrent_mutator_stop(&otherMutator, otherThread);
    ZrTests_Runtime_State_Destroy(otherState);
}

static void test_full_collection_cancels_active_concurrent_major(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;

    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    TEST_ASSERT_TRUE(
            g_state->global->garbageCollector->concurrentMajorActive);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_FALSE);
    ZrCore_GarbageCollector_GetStatsSnapshot(
            g_state->global, &snapshot);

    TEST_ASSERT_FALSE(
            g_state->global->garbageCollector->concurrentMajorActive);
    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE,
            snapshot.collectionPhase);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.fullCollectionCount);
}

static void test_heap_pressure_upgrades_active_major_to_full_compaction(void) {
    SZrGarbageCollectorStatsSnapshot snapshot;
    TZrSize stepCount = 0u;

    g_state->global->garbageCollector->gcMode =
            ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    TEST_ASSERT_TRUE(
            g_state->global->garbageCollector->concurrentMajorActive);

    ZrCore_GarbageCollector_SetHeapLimitBytes(g_state->global, 1u);
    do {
        ZrCore_GarbageCollector_CheckGc(g_state);
        ZrCore_GarbageCollector_GetStatsSnapshot(
                g_state->global, &snapshot);
        stepCount++;
    } while (snapshot.collectionPhase !=
                     ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE &&
             stepCount < 4096u);

    TEST_ASSERT_LESS_THAN_UINT64(4096u, stepCount);
    TEST_ASSERT_FALSE(
            g_state->global->garbageCollector->concurrentMajorActive);
    TEST_ASSERT_EQUAL_UINT64(1u, snapshot.fullCollectionCount);
    TEST_ASSERT_EQUAL_UINT64(0u, snapshot.majorCollectionCount);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL,
            snapshot.lastRequestedCollectionKind);
    TEST_ASSERT_TRUE(snapshot.compactPauseCount > 0u);
}

#include "test_gc_budget_constructor_defaults.inc"

/* TODO: CMake 当前只构建此目标，仓库内未见 CTest/suite 引用；
 * 核查 CI 是否单独执行，若无则接入常规测试入口。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_major_step_enters_concurrent_mark_without_finishing_cycle);
    RUN_TEST(test_concurrent_major_mark_slices_finish_one_collection);
    RUN_TEST(test_concurrent_major_slice_honors_max_objects);
    RUN_TEST(test_concurrent_major_slice_zero_max_objects_keeps_caller_limit);
    RUN_TEST(test_concurrent_major_slice_large_max_objects_keeps_caller_limit);
    RUN_TEST(test_concurrent_mark_slice_does_not_pause_same_domain_mutator);
    RUN_TEST(test_concurrent_barrier_keeps_target_written_from_black_owner);
    RUN_TEST(test_concurrent_marker_and_mutator_serialize_object_storage);
    RUN_TEST(test_concurrent_major_telemetry_is_attributed_to_domain);
    RUN_TEST(test_compaction_is_deferred_when_pause_budget_is_zero);
    RUN_TEST(test_remark_pause_is_domain_local);
    RUN_TEST(test_full_collection_cancels_active_concurrent_major);
    RUN_TEST(test_heap_pressure_upgrades_active_major_to_full_compaction);
    RUN_TEST(test_gc_budget_constructor_initializes_optional_state);
    return UNITY_END();
}
