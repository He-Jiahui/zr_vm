/* Real allocations must accrue nonnegative, saturating debt across mutators. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
typedef volatile LONG TestAtomic;
typedef HANDLE TestThread;
#else
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
typedef _Atomic int TestAtomic;
typedef pthread_t TestThread;
#endif
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

enum { WORKERS = 8, ITERATIONS = 20000, ALLOCATION_BYTES = 64 };
static TestAtomic liveBlocks, ready, start;

static int atomic_read(TestAtomic *value) {
#ifdef _WIN32
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void atomic_write(TestAtomic *value, int next) {
#ifdef _WIN32
    (void)InterlockedExchange(value, next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void atomic_add(TestAtomic *value, int delta) {
#ifdef _WIN32
    (void)InterlockedExchangeAdd(value, delta);
#else
    (void)atomic_fetch_add_explicit(value, delta, memory_order_acq_rel);
#endif
}

static void yield_thread(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}

static TZrPtr host_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                            TZrSize newSize, TZrInt64 flag) {
    void *next;
    (void)data; (void)oldSize; (void)flag;
    if (newSize == 0) {
        if (old != NULL) atomic_add(&liveBlocks, -1);
        free(old);
        return NULL;
    }
    next = realloc(old, newSize);
    if (next != NULL && old == NULL) atomic_add(&liveBlocks, 1);
    return next;
}

static int saturation(SZrGlobalState *global) {
    SZrState *state = global->mainThreadState;
    SZrGarbageCollector *collector = global->garbageCollector;
    void *block;
    int failed = 0;
    ZrCore_GarbageCollector_AddDebtSpace(global, ZR_MAX_MEMORY_OFFSET);
    if (collector->gcDebtSize != ZR_MAX_MEMORY_OFFSET) return 1;
    block = ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_OBJECT, 1);
    fprintf(stderr, "debt after allocation at maximum: %lld\n", (long long)collector->gcDebtSize);
    if (block == NULL || collector->gcDebtSize != ZR_MAX_MEMORY_OFFSET) failed = 1;
    collector->gcDebtSize = ZR_MAX_MEMORY_OFFSET;
    block = ZrCore_Memory_GcReallocate(state, block, 1, 8, ZR_MEMORY_NATIVE_TYPE_OBJECT);
    fprintf(stderr, "debt after growth at maximum: %lld\n", (long long)collector->gcDebtSize);
    if (block == NULL || collector->gcDebtSize != ZR_MAX_MEMORY_OFFSET) failed = 1;
    if (block != NULL) ZrCore_Memory_RawFreeWithType(global, block, 8, ZR_MEMORY_NATIVE_TYPE_OBJECT);
    collector->gcDebtSize = ZR_MAX_MEMORY_OFFSET;
    ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_MAX_MEMORY_OFFSET - 1);
    if (collector->gcDebtSize != 0) failed = 1;
    return failed;
}

typedef struct Worker {
    SZrState state;
    int failed;
} Worker;

static void worker_run(Worker *worker) {
    int entered = ZrCore_GcDomain_MutatorEnter(&worker->state);
    if (!entered) worker->failed = 1;
    atomic_add(&ready, 1);
    while (!atomic_read(&start)) yield_thread();
    if (!entered) return;
    for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
        void *block = ZrCore_Memory_GcMalloc(&worker->state,
                ZR_MEMORY_NATIVE_TYPE_OBJECT, ALLOCATION_BYTES);
        if (block == NULL) { worker->failed = 1; break; }
        ZrCore_Memory_RawFreeWithType(worker->state.global, block,
                ALLOCATION_BYTES, ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    ZrCore_GcDomain_MutatorLeave(&worker->state);
}

#ifdef _WIN32
static DWORD WINAPI worker_entry(LPVOID data) { worker_run((Worker *)data); return 0; }
#else
static void *worker_entry(void *data) { worker_run((Worker *)data); return NULL; }
#endif

static int concurrent_allocations(SZrGlobalState *global) {
    Worker workers[WORKERS] = {0};
    TestThread threads[WORKERS];
    int attached = 0, created = 0, failed = 0;
    TZrMemoryOffset debt;
    for (; attached < WORKERS; ++attached) {
        workers[attached].state.global = global;
        if (!ZrCore_GcDomain_MutatorAttach(global->mainThreadState,
                                          &workers[attached].state)) break;
    }
    for (; created < attached; ++created) {
#ifdef _WIN32
        threads[created] = CreateThread(NULL, 0, worker_entry, &workers[created], 0, NULL);
        if (threads[created] == NULL) break;
#else
        if (pthread_create(&threads[created], NULL, worker_entry, &workers[created]) != 0) break;
#endif
    }
    while (atomic_read(&ready) != created) yield_thread();
    /* All workers are registered and parked before resetting the baseline. */
    global->garbageCollector->gcDebtSize = 0;
    atomic_write(&start, 1);
    for (int index = 0; index < created; ++index) {
#ifdef _WIN32
        if (WaitForSingleObject(threads[index], INFINITE) != WAIT_OBJECT_0) abort();
        CloseHandle(threads[index]);
#else
        if (pthread_join(threads[index], NULL) != 0) abort();
#endif
        if (workers[index].failed) failed = 1;
    }
    debt = global->garbageCollector->gcDebtSize;
    fprintf(stderr, "concurrent debt actual=%lld expected=%lld\n", (long long)debt,
            (long long)WORKERS * ITERATIONS * ALLOCATION_BYTES);
    if (created != WORKERS || debt != (TZrMemoryOffset)WORKERS * ITERATIONS * ALLOCATION_BYTES)
        failed = 1;
    /* No assertion or early return may bypass join and detach. */
    for (int index = 0; index < attached; ++index)
        ZrCore_GcDomain_MutatorDetach(&workers[index].state);
    return failed;
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
    int failed;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2 || (strcmp(argv[1], "saturation") && strcmp(argv[1], "concurrent_allocations"))) return 2;
    global = ZrCore_GlobalState_New(host_allocator, NULL, 1, NULL);
    if (global == NULL) return 1;
    failed = strcmp(argv[1], "saturation") == 0 ? saturation(global) : concurrent_allocations(global);
    ZrCore_GlobalState_Free(global);
    if (atomic_read(&liveBlocks) != 0) {
        fprintf(stderr, "allocator leaked %d blocks\n", atomic_read(&liveBlocks));
        failed = 1;
    }
    return failed;
}
