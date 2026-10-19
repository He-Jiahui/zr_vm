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
typedef HANDLE ZrIgnoredRootThread;
typedef volatile LONG ZrIgnoredRootAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrIgnoredRootThread;
typedef _Atomic int ZrIgnoredRootAtomic;
#endif

#define ZR_IGNORED_ROOT_WORKER_COUNT 8u
#define ZR_IGNORED_ROOT_OBJECTS_PER_WORKER 128u

typedef struct SZrIgnoredRootWorker {
    SZrState state;
    SZrRawObject **objects;
    TZrSize objectCount;
    ZrIgnoredRootAtomic *readyCount;
    ZrIgnoredRootAtomic *start;
    ZrIgnoredRootAtomic failed;
    TZrBool removeRoots;
} SZrIgnoredRootWorker;

static SZrState *g_state;

static int ignored_root_atomic_load(ZrIgnoredRootAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void ignored_root_atomic_store(ZrIgnoredRootAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void ignored_root_atomic_increment(ZrIgnoredRootAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedIncrement(value);
#else
    (void)atomic_fetch_add_explicit(value, 1, memory_order_acq_rel);
#endif
}

static void ignored_root_sleep_one_ms(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration;
    duration.tv_sec = 0;
    duration.tv_nsec = 1000000L;
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static void ignored_root_worker_run(SZrIgnoredRootWorker *worker) {
    if (!ZrCore_GcDomain_MutatorEnter(&worker->state)) {
        ignored_root_atomic_store(&worker->failed, 1);
        ignored_root_atomic_increment(worker->readyCount);
        return;
    }

    ignored_root_atomic_increment(worker->readyCount);
    while (!ignored_root_atomic_load(worker->start)) {
        ignored_root_sleep_one_ms();
    }

    for (TZrSize index = 0u; index < worker->objectCount; ++index) {
        TZrBool succeeded = worker->removeRoots
                ? ZrCore_GarbageCollector_UnignoreObject(
                          worker->state.global, worker->objects[index])
                : ZrCore_GarbageCollector_IgnoreObject(
                          &worker->state, worker->objects[index]);
        if (!succeeded) {
            ignored_root_atomic_store(&worker->failed, 1);
            break;
        }
    }
    ZrCore_GcDomain_MutatorLeave(&worker->state);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI ignored_root_worker_entry(LPVOID argument) {
    ignored_root_worker_run((SZrIgnoredRootWorker *)argument);
    return 0;
}

static ZrIgnoredRootThread ignored_root_thread_start(SZrIgnoredRootWorker *worker) {
    return CreateThread(ZR_NULL, 0u, ignored_root_worker_entry, worker, 0u, ZR_NULL);
}

static int ignored_root_thread_join(ZrIgnoredRootThread thread) {
    DWORD result = WaitForSingleObject(thread, 10000u);
    CloseHandle(thread);
    return result == WAIT_OBJECT_0;
}
#else
static void *ignored_root_worker_entry(void *argument) {
    ignored_root_worker_run((SZrIgnoredRootWorker *)argument);
    return ZR_NULL;
}

static int ignored_root_thread_start(
        ZrIgnoredRootThread *thread, SZrIgnoredRootWorker *worker) {
    return pthread_create(thread, ZR_NULL, ignored_root_worker_entry, worker) == 0;
}

static int ignored_root_thread_join(ZrIgnoredRootThread thread) {
    return pthread_join(thread, ZR_NULL) == 0;
}
#endif

static SZrObject *ignored_root_create_object(void) {
    SZrObject *object = ZrCore_Object_New(g_state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    return object;
}

static void ignored_root_worker_init(
        SZrIgnoredRootWorker *worker,
        SZrRawObject **objects,
        TZrSize objectCount,
        ZrIgnoredRootAtomic *readyCount,
        ZrIgnoredRootAtomic *start,
        TZrBool removeRoots) {
    memset(worker, 0, sizeof(*worker));
    ZrCore_RawObject_Construct(
            &worker->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    worker->state.global = g_state->global;
    worker->objects = objects;
    worker->objectCount = objectCount;
    worker->readyCount = readyCount;
    worker->start = start;
    worker->removeRoots = removeRoots;
#if defined(ZR_PLATFORM_WIN)
    worker->failed = 0;
#else
    atomic_init(&worker->failed, 0);
#endif
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorAttach(g_state, &worker->state));
}

static void ignored_root_workers_detach(
        SZrIgnoredRootWorker *workers,
        ZrIgnoredRootThread *threads,
        TZrSize startedCount) {
    for (TZrSize index = 0u; index < startedCount; ++index) {
        TEST_ASSERT_TRUE(ignored_root_thread_join(threads[index]));
        ZrCore_GcDomain_MutatorDetach(&workers[index].state);
        TEST_ASSERT_EQUAL_INT(0, ignored_root_atomic_load(&workers[index].failed));
    }
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

static void test_overlapping_native_pins_keep_root_until_last_unpin(void) {
    SZrObject *object = ignored_root_create_object();
    SZrRawObject *rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    SZrGcNativeCallPin firstPin;
    SZrGcNativeCallPin secondPin;
    TZrUInt32 originalPinFlags = rawObject->garbageCollectMark.pinFlags;

    memset(&firstPin, 0, sizeof(firstPin));
    memset(&secondPin, 0, sizeof(secondPin));
    TEST_ASSERT_TRUE(ZrCore_Gc_NativeCallPinObject(g_state, rawObject, &firstPin));
    TEST_ASSERT_TRUE(ZrCore_Gc_NativeCallPinObject(g_state, rawObject, &secondPin));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(g_state->global, rawObject));

    /* The first token added the shared flags/root, but its lifetime can end
     * before the overlapping second token on another native call. */
    ZrCore_Gc_NativeCallUnpin(g_state->global, &firstPin);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(g_state->global, rawObject));
    TEST_ASSERT_EQUAL_UINT32(
            originalPinFlags | ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE,
            rawObject->garbageCollectMark.pinFlags);

    ZrCore_Gc_NativeCallUnpin(g_state->global, &secondPin);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(g_state->global, rawObject));
    TEST_ASSERT_EQUAL_UINT32(originalPinFlags, rawObject->garbageCollectMark.pinFlags);
}

static void test_concurrent_ignore_grow_and_unignore_preserve_distinct_roots(void) {
    const TZrSize workerCount = ZR_IGNORED_ROOT_WORKER_COUNT;
    const TZrSize objectsPerWorker = ZR_IGNORED_ROOT_OBJECTS_PER_WORKER;
    const TZrSize totalObjects = workerCount * objectsPerWorker;
    SZrIgnoredRootWorker workers[ZR_IGNORED_ROOT_WORKER_COUNT];
    ZrIgnoredRootThread threads[ZR_IGNORED_ROOT_WORKER_COUNT];
    SZrRawObject *objects[ZR_IGNORED_ROOT_WORKER_COUNT *
                          ZR_IGNORED_ROOT_OBJECTS_PER_WORKER];
    ZrIgnoredRootAtomic readyCount;
    ZrIgnoredRootAtomic start;
    TZrSize startedCount = 0u;

#if defined(ZR_PLATFORM_WIN)
    readyCount = 0;
    start = 0;
#else
    atomic_init(&readyCount, 0);
    atomic_init(&start, 0);
#endif

    /* Preallocate unique objects so contention is limited to registry access. */
    for (TZrSize index = 0u; index < totalObjects; ++index) {
        objects[index] = ZR_CAST_RAW_OBJECT_AS_SUPER(ignored_root_create_object());
    }
    for (TZrSize index = 0u; index < workerCount; ++index) {
        ignored_root_worker_init(
                &workers[index],
                &objects[index * objectsPerWorker],
                objectsPerWorker,
                &readyCount,
                &start,
                ZR_FALSE);
#if defined(ZR_PLATFORM_WIN)
        threads[startedCount] = ignored_root_thread_start(&workers[index]);
        TEST_ASSERT_NOT_NULL(threads[startedCount]);
#else
        TEST_ASSERT_EQUAL_INT(
                0, ignored_root_thread_start(&threads[startedCount], &workers[index]));
#endif
        startedCount++;
    }

    while (ignored_root_atomic_load(&readyCount) < (int)workerCount) {
        ignored_root_sleep_one_ms();
    }
    ignored_root_atomic_store(&start, 1);
    ignored_root_workers_detach(workers, threads, startedCount);

    for (TZrSize index = 0u; index < totalObjects; ++index) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(
                g_state->global, objects[index]));
    }

    /* Reattach the worker records and race concurrent swap-removal. */
#if defined(ZR_PLATFORM_WIN)
    readyCount = 0;
    start = 0;
#else
    atomic_store_explicit(&readyCount, 0, memory_order_release);
    atomic_store_explicit(&start, 0, memory_order_release);
#endif
    startedCount = 0u;
    for (TZrSize index = 0u; index < workerCount; ++index) {
        /* MutatorDetach above retired each stack state; reattach before reuse. */
        TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorAttach(g_state, &workers[index].state));
        workers[index].removeRoots = ZR_TRUE;
#if defined(ZR_PLATFORM_WIN)
        workers[index].failed = 0;
#else
        atomic_store_explicit(&workers[index].failed, 0, memory_order_release);
#endif
#if defined(ZR_PLATFORM_WIN)
        threads[startedCount] = ignored_root_thread_start(&workers[index]);
        TEST_ASSERT_NOT_NULL(threads[startedCount]);
#else
        TEST_ASSERT_EQUAL_INT(
                0, ignored_root_thread_start(&threads[startedCount], &workers[index]));
#endif
        startedCount++;
    }
    while (ignored_root_atomic_load(&readyCount) < (int)workerCount) {
        ignored_root_sleep_one_ms();
    }
    ignored_root_atomic_store(&start, 1);
    ignored_root_workers_detach(workers, threads, startedCount);
    for (TZrSize index = 0u; index < totalObjects; ++index) {
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
                g_state->global, objects[index]));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_overlapping_native_pins_keep_root_until_last_unpin);
    RUN_TEST(test_concurrent_ignore_grow_and_unignore_preserve_distinct_roots);
    return UNITY_END();
}
