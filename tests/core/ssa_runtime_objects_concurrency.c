#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "ssa_runtime_objects_concurrency.h"
#include "ssa_runtime_objects_faults.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/state.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrCollectorThread;
typedef volatile LONG ZrCollectorAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrCollectorThread;
typedef _Atomic int ZrCollectorAtomic;
#endif

enum {
    ZR_TEST_COLLECTOR_PAUSE_TIMEOUT_MS = 2000,
    ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS = 5000,
    ZR_TEST_COLLECTOR_ROOT_GROUPS = 3
};

typedef struct ZrCompetingCollector {
    SZrState state;
    ZrCollectorAtomic start;
    ZrCollectorAtomic stop;
    ZrCollectorAtomic requested;
    ZrCollectorAtomic materializationReturned;
    ZrCollectorAtomic done;
    TZrBool collected;
    TZrBool collectedAfterReturn;
    TZrBool hookObserved;
    TZrBool pauseAcquired;
    TZrUInt64 collectionsBefore;
    TZrUInt64 collectionsAfter;
} ZrCompetingCollector;

typedef struct ZrCollectorCallerRoots {
    SZrAotGcRootSlot *slots;
    SZrAotGcRootMap maps[ZR_TEST_COLLECTOR_ROOT_GROUPS];
    SZrAotGcRootFrame frames[ZR_TEST_COLLECTOR_ROOT_GROUPS];
    TZrBool pushed[ZR_TEST_COLLECTOR_ROOT_GROUPS];
} ZrCollectorCallerRoots;

static int collector_atomic_load(ZrCollectorAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void collector_atomic_store(ZrCollectorAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void collector_atomic_init(ZrCollectorAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    *value = 0;
#else
    atomic_init(value, 0);
#endif
}

static void collector_sleep(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration = {0, 1000000L};
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static void collector_run(ZrCompetingCollector *context) {
    TZrUInt32 attempts;
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        if (collector_atomic_load(&context->start) ||
            collector_atomic_load(&context->stop)) break;
        collector_sleep();
    }
    if (collector_atomic_load(&context->start) &&
        !collector_atomic_load(&context->stop)) {
        collector_atomic_store(&context->requested, 1);
        /* Stay inactive until the hook: a running worker would park before
         * the materializer reaches its first barrier. An inactive attached
         * state may own a stop-the-world collection directly. */
        if (ZrCore_GcDomain_StopTheWorldBegin(
                    &context->state, ZR_TEST_COLLECTOR_PAUSE_TIMEOUT_MS, ZR_NULL)) {
            SZrGarbageCollector *collector = context->state.global->garbageCollector;
            TZrUInt64 before = collector->statsSnapshot.fullCollectionCount;
            context->pauseAcquired = ZR_TRUE;
            context->collectionsBefore = before;
            /* Current GC promotion/compaction preserves object addresses.
             * Assert the real protection boundary: no competing collection
             * may enter while attachment or temporary-root cleanup is live. */
            context->collectedAfterReturn =
                    collector_atomic_load(&context->materializationReturned) != 0;
            ZrCore_GarbageCollector_GcFull(&context->state, ZR_TRUE);
            context->collectionsAfter = collector->statsSnapshot.fullCollectionCount;
            context->collected = collector->statsSnapshot.fullCollectionCount == before + 1u;
            ZrCore_GcDomain_StopTheWorldEnd(&context->state);
        }
    }
    collector_atomic_store(&context->done, 1);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI collector_entry(LPVOID argument) {
    collector_run((ZrCompetingCollector *)argument);
    return 0;
}

static TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context) {
    *thread = CreateThread(ZR_NULL, 0u, collector_entry, context, 0u, ZR_NULL);
    return *thread != ZR_NULL;
}

static TZrBool collector_join(ZrCollectorThread thread) {
    DWORD result = WaitForSingleObject(thread, ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS);
    if (result != WAIT_OBJECT_0) {
        fputs("competing collector thread could not be joined\n", stderr);
        abort();
    }
    return CloseHandle(thread) != 0;
}
#else
static void *collector_entry(void *argument) {
    collector_run((ZrCompetingCollector *)argument);
    return ZR_NULL;
}

static TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context) {
    return pthread_create(thread, ZR_NULL, collector_entry, context) == 0;
}

static TZrBool collector_join(ZrCollectorThread thread) {
    if (pthread_join(thread, ZR_NULL) != 0) {
        fputs("competing collector thread could not be joined\n", stderr);
        abort();
    }
    return ZR_TRUE;
}
#endif

static void collector_barrier_hook(SZrState *state, void *argument) {
    ZrCompetingCollector *context = argument;
    TZrUInt32 attempts;
    collector_atomic_store(&context->start, 1);
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        SZrGcDomainMutatorSnapshot snapshot;
        ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
        if (collector_atomic_load(&context->requested) && snapshot.pauseRequested) {
            context->hookObserved = ZR_TRUE;
            return;
        }
        if (collector_atomic_load(&context->done)) return;
        collector_sleep();
    }
}

static TZrBool caller_root_push(SZrState *state, ZrCollectorCallerRoots *roots,
                               TZrUInt32 group, void *base) {
    if (roots->maps[group].rootCount == 0u) return ZR_TRUE;
    roots->pushed[group] = ZrCore_Gc_AotRootFramePush(
            state, &roots->frames[group], (TZrStackValuePointer)base, &roots->maps[group]);
    return roots->pushed[group];
}

static void caller_root_slot(SZrAotGcRootSlot *slot, size_t offset) {
    slot->locationKind = ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slot->frameByteOffset = (TZrUInt32)offset;
}

static TZrBool caller_roots_prepare(const SZrExecIrObjectMaterializationRequest *request,
                                   ZrCollectorCallerRoots *roots) {
    TZrUInt64 count = (TZrUInt64)request->valueCount + request->typeCount + request->target->capacity;
    TZrUInt32 index, used = 0u;
    if (count == 0u || count > SIZE_MAX / sizeof(*roots->slots) || count > UINT32_MAX ||
        request->valueCount > UINT32_MAX / sizeof(*request->values) ||
        request->typeCount > UINT32_MAX / sizeof(*request->types) ||
        request->target->capacity > UINT32_MAX / sizeof(*request->target->objects)) return ZR_FALSE;
    roots->slots = calloc((size_t)count, sizeof(*roots->slots));
    if (roots->slots == ZR_NULL) return ZR_FALSE;
    roots->maps[0].roots = roots->slots;
    for (index = 0u; index < request->valueCount; ++index) {
        if (request->values[index].isGarbageCollectable) {
            caller_root_slot(&roots->slots[used++],
                    (size_t)index * sizeof(*request->values) + offsetof(SZrTypeValue, value));
            ++roots->maps[0].rootCount;
        }
    }
    roots->maps[1].roots = &roots->slots[used];
    roots->maps[1].rootCount = request->typeCount;
    for (index = 0u; index < request->typeCount; ++index)
        caller_root_slot(&roots->slots[used++], (size_t)index * sizeof(*request->types) +
                         offsetof(SZrExecIrRuntimeTypeBinding, prototype));
    roots->maps[2].roots = &roots->slots[used];
    roots->maps[2].rootCount = request->target->count;
    for (index = 0u; index < request->target->capacity; ++index)
        caller_root_slot(&roots->slots[used++], (size_t)index * sizeof(*request->target->objects));
    return caller_root_push(request->state, roots, 0u, request->values) &&
           caller_root_push(request->state, roots, 1u, request->types) &&
           caller_root_push(request->state, roots, 2u, request->target->objects);
}

static TZrBool caller_roots_release(SZrState *state, ZrCollectorCallerRoots *roots) {
    TZrUInt32 group = ZR_TEST_COLLECTOR_ROOT_GROUPS;
    TZrBool success = ZR_TRUE;
    while (group != 0u) {
        --group;
        if (roots->pushed[group] && !ZrCore_Gc_AotRootFramePop(state, &roots->frames[group]))
            success = ZR_FALSE;
    }
    free(roots->slots);
    return success;
}

TZrBool ssa_runtime_objects_competing_collector(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    ZrCompetingCollector context;
    ZrCollectorCallerRoots roots = {0};
    ZrCollectorThread thread;
    TZrBool success = ZR_FALSE, attached = ZR_FALSE, entered = ZR_FALSE;
    TZrBool materialized = ZR_FALSE;
    const char *stage = "caller roots";
    TZrUInt32 attempts, priorRootDepth;
    if (request == ZR_NULL || request->state == ZR_NULL || request->target == ZR_NULL ||
        request->target->count > request->target->capacity ||
        (request->valueCount != 0u && request->values == ZR_NULL) ||
        (request->typeCount != 0u && request->types == ZR_NULL) ||
        (request->target->capacity != 0u && request->target->objects == ZR_NULL)) return ZR_FALSE;
    memset(&context, 0, sizeof(context));
    collector_atomic_init(&context.start);
    collector_atomic_init(&context.stop);
    collector_atomic_init(&context.requested);
    collector_atomic_init(&context.materializationReturned);
    collector_atomic_init(&context.done);
    priorRootDepth = ZrCore_Gc_AotRootFrameDepth(request->state);
    if (!caller_roots_prepare(request, &roots)) goto finish;
    stage = "caller enter";
    entered = ZrCore_GcDomain_MutatorEnter(request->state);
    if (!entered) goto finish;
    ZrCore_RawObject_Construct(&context.state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    context.state.global = request->state->global;
    stage = "worker attach";
    attached = ZrCore_GcDomain_MutatorAttach(request->state, &context.state);
    if (!attached) goto finish;
    stage = "worker start";
    if (!collector_start(&thread, &context)) goto finish;

    stage = "materialization/collection";
    ssa_runtime_objects_set_barrier_hook(collector_barrier_hook, &context);
    success = ssa_runtime_objects_materialize(request, diagnostic);
    collector_atomic_store(&context.materializationReturned, 1);
    materialized = success;
    ssa_runtime_objects_set_barrier_hook(ZR_NULL, ZR_NULL);
    /* Our extra RUNNING scope prevents the waiting collector from taking the
     * world between publication and registration of the new output roots. */
    roots.maps[2].rootCount = request->target->count;
    if (!roots.pushed[2] && !caller_root_push(request->state, &roots, 2u, request->target->objects))
        success = ZR_FALSE;
    if (!collector_atomic_load(&context.start)) collector_atomic_store(&context.stop, 1);
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        if (collector_atomic_load(&context.done)) break;
        (void)ZrCore_GcDomain_MutatorPoll(request->state);
        collector_sleep();
    }
    if (!collector_atomic_load(&context.done)) {
        /* Returning would let Unity tear down a domain still used by a live
         * collector. Fail the process instead of leaving a dangling worker. */
        fputs("competing collector did not finish within its bounded wait\n", stderr);
        abort();
    }
    if (!collector_join(thread)) success = ZR_FALSE;
    success = success && context.hookObserved && context.collected && context.collectedAfterReturn;
finish:
    if (attached) ZrCore_GcDomain_MutatorDetach(&context.state);
    if (!caller_roots_release(request->state, &roots)) success = ZR_FALSE;
    if (ZrCore_Gc_AotRootFrameDepth(request->state) != priorRootDepth) success = ZR_FALSE;
    if (entered) ZrCore_GcDomain_MutatorLeave(request->state);
    if (!success) {
        fprintf(stderr,
                "competing collector failed: stage=%s materialized=%d "
                "hook=%d pause=%d collected=%d afterReturn=%d collections=%llu->%llu "
                "roots=%u->%u diagnostic=%d\n",
                stage, (int)materialized, (int)context.hookObserved,
                (int)context.pauseAcquired, (int)context.collected, (int)context.collectedAfterReturn,
                (unsigned long long)context.collectionsBefore,
                (unsigned long long)context.collectionsAfter,
                (unsigned)priorRootDepth, (unsigned)ZrCore_Gc_AotRootFrameDepth(request->state),
                diagnostic != ZR_NULL ? (int)diagnostic->code : -1);
    }
    return success;
}
