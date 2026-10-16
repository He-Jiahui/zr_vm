#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
typedef HANDLE TestThread;
typedef volatile LONG TestAtomic;
static CRITICAL_SECTION allocationLock;
static int atomic_get(TestAtomic *value) { return (int)InterlockedCompareExchange(value, 0, 0); }
static void atomic_set(TestAtomic *value, int next) { (void)InterlockedExchange(value, next); }
static void atomic_increment(TestAtomic *value) { (void)InterlockedIncrement(value); }
static void sleep_ms(void) { Sleep(1); }
static void allocation_lock(void) { EnterCriticalSection(&allocationLock); }
static void allocation_unlock(void) { LeaveCriticalSection(&allocationLock); }
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t TestThread;
typedef _Atomic int TestAtomic;
static pthread_mutex_t allocationLock = PTHREAD_MUTEX_INITIALIZER;
static int atomic_get(TestAtomic *value) { return atomic_load_explicit(value, memory_order_acquire); }
static void atomic_set(TestAtomic *value, int next) { atomic_store_explicit(value, next, memory_order_release); }
static void atomic_increment(TestAtomic *value) { (void)atomic_fetch_add_explicit(value, 1, memory_order_acq_rel); }
static void sleep_ms(void) { struct timespec delay = {0, 1000000L}; (void)nanosleep(&delay, NULL); }
static void allocation_lock(void) { (void)pthread_mutex_lock(&allocationLock); }
static void allocation_unlock(void) { (void)pthread_mutex_unlock(&allocationLock); }
#endif

#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

typedef struct Allocation { void *pointer; TZrSize size; TZrInt64 flag; } Allocation;
static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError;
static TZrSize failArrayBytes;
static int reserveFailures;
static int failures;

#define EXPECT(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); ++failures; \
} } while (0)

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next = NULL;
    (void)data;
    allocation_lock();
    if (old != NULL) {
        for (; index < liveCount && allocations[index].pointer != old; ++index) {}
        if (index == liveCount || allocations[index].size != oldSize || allocations[index].flag != flag) {
            allocatorError = 1;
            goto done;
        }
    } else if (oldSize != 0) { allocatorError = 1; goto done; }
    if (newSize == 0) {
        if (old != NULL) { free(old); allocations[index] = allocations[--liveCount]; }
        goto done;
    }
    if (failArrayBytes != 0 && old == NULL && newSize == failArrayBytes &&
        flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        ++reserveFailures;
        fprintf(stderr, "ignored registry reserve OOM: category=%lld bytes=%zu ordinal=%d\n",
                (long long)flag, (size_t)newSize, reserveFailures);
        fflush(stderr);
        goto done;
    }
    next = realloc(old, newSize);
    if (next == NULL) goto done;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
done:
    allocation_unlock();
    return next;
}

static SZrRawObject *new_object(SZrState *state) {
    SZrObject *object = ZrCore_Object_New(state, NULL);
    EXPECT(object != NULL);
    if (object != NULL) ZrCore_Object_Init(state, object);
    return (SZrRawObject *)object;
}

static void explicit_root(SZrState *state, int addDuringPin) {
    SZrRawObject *object = new_object(state);
    SZrGcNativeCallPin pin = {0};
    TZrUInt32 originalFlags;
    TZrBool pinned;
    if (object == NULL) return;
    originalFlags = object->garbageCollectMark.pinFlags;
    if (!addDuringPin) EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, object));
    pinned = ZrCore_Gc_NativeCallPinObject(state, object, &pin);
    EXPECT(pinned);
    if (addDuringPin) EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, object));
    if (pinned) ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
    EXPECT(object->garbageCollectMark.pinFlags == originalFlags);
    EXPECT(ZrCore_GarbageCollector_UnignoreObject(state->global, object));
    EXPECT(!ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
}

static void temporary_roots(SZrState *state) {
    SZrRawObject *object = new_object(state);
    SZrGcRootLease first = {0}, second = {0}, empty = {0};
    SZrGarbageCollectionObjectMark before;
    TZrSize count = state->global->garbageCollector->ignoredObjectCount;
    if (object == NULL) return;
    before = object->garbageCollectMark;
    EXPECT(ZrCore_Gc_RootLeaseAcquire(state, NULL, &empty) && empty.object == NULL);
    EXPECT(ZrCore_Gc_RootLeaseAcquire(state, object, &first));
    EXPECT(ZrCore_Gc_RootLeaseAcquire(state, object, &second));
    EXPECT(!ZrCore_Gc_RootLeaseAcquire(state, object, &first));
    EXPECT(state->global->garbageCollector->ignoredObjectCount == count + 1);
    EXPECT(object->garbageCollectMark.temporaryRootRefCount == 2);
    EXPECT(object->garbageCollectMark.storageKind == before.storageKind);
    EXPECT(object->garbageCollectMark.regionKind == before.regionKind);
    EXPECT(object->garbageCollectMark.regionId == before.regionId);
    EXPECT(object->garbageCollectMark.regionDescriptorIndex == before.regionDescriptorIndex);
    EXPECT(object->garbageCollectMark.pinFlags == before.pinFlags);
    EXPECT(object->garbageCollectMark.escapeFlags == before.escapeFlags);
    EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, object));
    EXPECT(ZrCore_GarbageCollector_UnignoreObject(state->global, object));
    EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
    ZrCore_Gc_RootLeaseRelease(state->global, &first);
    ZrCore_Gc_RootLeaseRelease(state->global, &first);
    EXPECT(first.object == NULL && object->garbageCollectMark.temporaryRootRefCount == 1);
    EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, object));
    ZrCore_Gc_RootLeaseRelease(state->global, &second);
    EXPECT(second.object == NULL && object->garbageCollectMark.temporaryRootRefCount == 0);
    EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
    EXPECT(ZrCore_GarbageCollector_UnignoreObject(state->global, object));
    EXPECT(state->global->garbageCollector->ignoredObjectCount == count);
    /* A pre-existing explicit owner also survives both temporary releases. */
    EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, object));
    EXPECT(ZrCore_Gc_RootLeaseAcquire(state, object, &first));
    EXPECT(ZrCore_Gc_RootLeaseAcquire(state, object, &second));
    ZrCore_Gc_RootLeaseRelease(state->global, &second);
    ZrCore_Gc_RootLeaseRelease(state->global, &first);
    EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
    EXPECT(ZrCore_GarbageCollector_UnignoreObject(state->global, object));
    if (ZrCore_Gc_RootLeaseAcquire(state, object, &first)) {
        state->global->garbageCollector->stopGcFlag = ZR_FALSE;
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        state->global->garbageCollector->stopGcFlag = ZR_TRUE;
        EXPECT(first.object == object);
        EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, first.object));
        ZrCore_Gc_RootLeaseRelease(state->global, &first);
    } else ++failures;
}

#define DISTINCT_ROOTS_PER_WORKER 96
typedef struct Worker {
    SZrState state;
    SZrRawObject *object;
    int second;
    int failed;
    int protectedAfterFirstRelease;
    int distinct;
    SZrRawObject *objects[DISTINCT_ROOTS_PER_WORKER];
} Worker;
static TestAtomic stage;
static TestAtomic cancelled;

static int wait_stage(int target) {
    for (int attempt = 0; attempt < 5000; ++attempt) {
        if (atomic_get(&cancelled)) return 0;
        if (atomic_get(&stage) >= target) return 1;
        sleep_ms();
    }
    atomic_set(&cancelled, 1);
    return 0;
}

static void worker_run(Worker *worker) {
    SZrGcNativeCallPin pin = {0};
    int entered = 0;
    TZrBool pinned = ZR_FALSE;
    if (!ZrCore_GcDomain_MutatorEnter(&worker->state)) goto failed;
    entered = 1;
    if (worker->distinct) {
        SZrGcRootLease leases[DISTINCT_ROOTS_PER_WORKER] = {{0}};
        atomic_increment(&stage);
        if (!wait_stage(4)) goto failed;
        for (int round = 0; round < 12; ++round) {
            for (int index = 0; index < DISTINCT_ROOTS_PER_WORKER; ++index) {
                if (!ZrCore_Gc_RootLeaseAcquire(&worker->state, worker->objects[index], &leases[index])) {
                    worker->failed = 1;
                    goto distinct_cleanup;
                }
                if (!ZrCore_GarbageCollector_IsObjectIgnoredFast(worker->state.global, worker->objects[index]))
                    worker->failed = 1;
            }
            /* Interleaved deletes force swap-index repair while peers query/grow. */
            for (int parity = 0; parity < 2; ++parity)
                for (int index = parity; index < DISTINCT_ROOTS_PER_WORKER; index += 2)
                    ZrCore_Gc_RootLeaseRelease(worker->state.global, &leases[index]);
        }
distinct_cleanup:
        for (int index = 0; index < DISTINCT_ROOTS_PER_WORKER; ++index)
            ZrCore_Gc_RootLeaseRelease(worker->state.global, &leases[index]);
        goto cleanup;
    }
    if (worker->second && !wait_stage(1)) goto failed;
    pinned = ZrCore_Gc_NativeCallPinObject(&worker->state, worker->object, &pin);
    if (!pinned) goto failed;
    if (worker->second) {
        atomic_set(&stage, 2);
        if (!wait_stage(3)) goto failed;
        worker->protectedAfterFirstRelease =
                ZrCore_GarbageCollector_IsObjectIgnored(worker->state.global, worker->object) &&
                (worker->object->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0;
    } else {
        atomic_set(&stage, 1);
        if (!wait_stage(2)) goto failed;
        ZrCore_Gc_NativeCallUnpin(worker->state.global, &pin);
        pinned = ZR_FALSE;
        atomic_set(&stage, 3);
    }
    goto cleanup;
failed:
    worker->failed = 1;
    atomic_set(&cancelled, 1);
cleanup:
    if (pinned) ZrCore_Gc_NativeCallUnpin(worker->state.global, &pin);
    if (entered) ZrCore_GcDomain_MutatorLeave(&worker->state);
}

#ifdef _WIN32
static DWORD WINAPI worker_entry(LPVOID argument) { worker_run((Worker *)argument); return 0; }
static int start_thread(TestThread *thread, Worker *worker) {
    *thread = CreateThread(NULL, 0, worker_entry, worker, 0, NULL);
    return *thread != NULL;
}
static void join_thread(TestThread thread) {
    /* Never detach/free a worker while it is running; CTest bounds a deadlock. */
    EXPECT(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    CloseHandle(thread);
}
#else
static void *worker_entry(void *argument) { worker_run((Worker *)argument); return NULL; }
static int start_thread(TestThread *thread, Worker *worker) { return pthread_create(thread, NULL, worker_entry, worker) == 0; }
static void join_thread(TestThread thread) { EXPECT(pthread_join(thread, NULL) == 0); }
#endif

static void same_object_threads(SZrState *state) {
    Worker workers[2];
    TestThread threads[2];
    int attached = 0;
    int started = 0;
    SZrRawObject *object = new_object(state);
    TZrUInt32 originalFlags;
    if (object == NULL) return;
    originalFlags = object->garbageCollectMark.pinFlags;
    memset(workers, 0, sizeof(workers));
    atomic_set(&stage, 0);
    atomic_set(&cancelled, 0);
    for (int index = 0; index < 2; ++index) {
        Worker *worker = &workers[index];
        ZrCore_RawObject_Construct(&worker->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
        worker->state.global = state->global;
        worker->object = object;
        worker->second = index;
        if (!ZrCore_GcDomain_MutatorAttach(state, &worker->state)) {
            ++failures;
            goto cleanup;
        }
        ++attached;
    }
    for (int index = 0; index < 2; ++index) {
        if (!start_thread(&threads[index], &workers[index])) {
            ++failures;
            atomic_set(&cancelled, 1);
            goto cleanup;
        }
        ++started;
    }
cleanup:
    /* Join every started worker before touching its record or checking results. */
    for (int index = 0; index < started; ++index) join_thread(threads[index]);
    for (int index = 0; index < attached; ++index) ZrCore_GcDomain_MutatorDetach(&workers[index].state);
    if (started == 2) {
        EXPECT(!workers[0].failed && !workers[1].failed);
        EXPECT(workers[1].protectedAfterFirstRelease);
        EXPECT(!ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
        EXPECT(object->garbageCollectMark.pinFlags == originalFlags);
    }
}

static void distinct_root_threads(SZrState *state) {
    Worker workers[4];
    TestThread threads[4];
    int attached = 0, started = 0;
    TZrSize originalCount = state->global->garbageCollector->ignoredObjectCount;
    memset(workers, 0, sizeof(workers));
    atomic_set(&stage, 0);
    atomic_set(&cancelled, 0);
    for (int index = 0; index < 4; ++index) {
        Worker *worker = &workers[index];
        worker->distinct = 1;
        for (int item = 0; item < DISTINCT_ROOTS_PER_WORKER; ++item) {
            worker->objects[item] = new_object(state);
            if (worker->objects[item] == NULL) goto cleanup;
        }
        ZrCore_RawObject_Construct(&worker->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
        worker->state.global = state->global;
        if (!ZrCore_GcDomain_MutatorAttach(state, &worker->state)) { ++failures; goto cleanup; }
        ++attached;
    }
    for (int index = 0; index < 4; ++index) {
        if (!start_thread(&threads[index], &workers[index])) {
            ++failures;
            atomic_set(&cancelled, 1);
            goto cleanup;
        }
        ++started;
    }
cleanup:
    for (int index = 0; index < started; ++index) join_thread(threads[index]);
    for (int index = 0; index < attached; ++index) ZrCore_GcDomain_MutatorDetach(&workers[index].state);
    for (int index = 0; index < started; ++index) {
        EXPECT(!workers[index].failed);
        for (int item = 0; item < DISTINCT_ROOTS_PER_WORKER; ++item) {
            SZrRawObject *object = workers[index].objects[item];
            EXPECT(!ZrCore_GarbageCollector_IsObjectIgnored(state->global, object));
            EXPECT(object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE);
            EXPECT(object->garbageCollectMark.temporaryRootRefCount == 0);
        }
    }
    EXPECT(state->global->garbageCollector->ignoredObjectCount == originalCount);
}

static void reserve_oom(SZrState *state) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    SZrRawObject *warm = new_object(state);
    SZrRawObject *target = new_object(state);
    SZrGcNativeCallPin warmPin = {0};
    SZrGcNativeCallPin pin = {0};
    SZrRawObject **roots = NULL;
    SZrRawObject **oldTable;
    SZrGarbageCollectionObjectMark before;
    TZrSize oldCount, oldCapacity;
    TZrBool pinned;
    if (warm == NULL || target == NULL) return;
    /* Establish the pinned region before arming the registry-array fault. */
    pinned = ZrCore_Gc_NativeCallPinObject(state, warm, &warmPin);
    EXPECT(pinned);
    if (!pinned) return;
    ZrCore_Gc_NativeCallUnpin(state->global, &warmPin);
    oldCapacity = collector->ignoredObjectCapacity;
    EXPECT(oldCapacity > 0 && collector->ignoredObjectCount == 0);
    if (oldCapacity == 0 || collector->ignoredObjectCount != 0) return;
    roots = (SZrRawObject **)calloc((size_t)oldCapacity, sizeof(*roots));
    EXPECT(roots != NULL);
    if (roots == NULL) return;
    for (TZrSize index = 0; index < oldCapacity; ++index) {
        roots[index] = new_object(state);
        if (roots[index] == NULL || !ZrCore_GarbageCollector_IgnoreObject(state, roots[index])) {
            ++failures;
            goto cleanup;
        }
    }
    oldCount = collector->ignoredObjectCount;
    oldTable = collector->ignoredObjects;
    before = target->garbageCollectMark;
    /* Current registry growth seam doubles pointer capacity; this selects its
     * ARRAY allocation, not an object or GC-region descriptor allocation. */
    failArrayBytes = oldCapacity * 2 * sizeof(*oldTable);
    pinned = ZrCore_Gc_NativeCallPinObject(state, target, &pin);
    failArrayBytes = 0;
    EXPECT(reserveFailures == 1 && !pinned && pin.object == NULL);
    EXPECT(collector->ignoredObjects == oldTable);
    EXPECT(collector->ignoredObjectCount == oldCount && collector->ignoredObjectCapacity == oldCapacity);
    EXPECT(target->garbageCollectMark.ignoredRegistryIndex == before.ignoredRegistryIndex);
    EXPECT(target->garbageCollectMark.pinFlags == before.pinFlags);
    EXPECT(target->garbageCollectMark.temporaryRootRefCount == before.temporaryRootRefCount);
    EXPECT(target->garbageCollectMark.nativeCallPinRefCount == before.nativeCallPinRefCount);
    EXPECT(target->garbageCollectMark.explicitIgnoredRoot == before.explicitIgnoredRoot);
    EXPECT(target->garbageCollectMark.nativeCallOwnsPinKind == before.nativeCallOwnsPinKind);
    EXPECT(target->garbageCollectMark.storageKind == before.storageKind);
    EXPECT(target->garbageCollectMark.heapGenerationKind == before.heapGenerationKind);
    EXPECT(target->garbageCollectMark.regionKind == before.regionKind);
    EXPECT(target->garbageCollectMark.regionId == before.regionId);
    EXPECT(target->garbageCollectMark.regionDescriptorIndex == before.regionDescriptorIndex);
    EXPECT(target->garbageCollectMark.escapeFlags == before.escapeFlags);
    EXPECT(target->garbageCollectMark.anchorScopeDepth == before.anchorScopeDepth);
    EXPECT(target->garbageCollectMark.promotionReason == before.promotionReason);
    for (TZrSize index = 0; index < oldCount; ++index) {
        EXPECT(collector->ignoredObjects[index] == roots[index]);
        EXPECT(roots[index]->garbageCollectMark.ignoredRegistryIndex == index);
        EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, roots[index]));
    }
    if (pinned) ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    pinned = ZrCore_Gc_NativeCallPinObject(state, target, &pin);
    EXPECT(pinned);
    if (pinned) {
        EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, target));
        ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    }
cleanup:
    failArrayBytes = 0;
    for (TZrSize index = 0; index < oldCapacity; ++index) {
        if (roots[index] != NULL) (void)ZrCore_GarbageCollector_UnignoreObject(state->global, roots[index]);
    }
    free(roots);
}


/* Fill the real descriptor table using live native pins. No collector field is
 * forged; all object allocations finish before the descriptor growth fault. */
static void region_reserve_oom(SZrState *state) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    SZrRawObject *target = new_object(state);
    SZrRawObject **objects = NULL;
    SZrGcNativeCallPin *pins = NULL;
    SZrGcNativeCallPin pin = {0};
    SZrGcRootLease lease = {0};
    SZrGarbageCollectionObjectMark before;
    SZrGarbageCollectRegionDescriptor *savedRegions = NULL;
    SZrGarbageCollectRegionDescriptor *oldRegions;
    SZrRawObject **oldRoots;
    TZrSize poolCount, used = 0, oldCount, oldCapacity, oldRootCount, oldRootCapacity;
    TZrUInt32 oldNextId;
    int failuresBefore = reserveFailures;
    TZrBool pinned;
    if (target == NULL) return;
    EXPECT(collector->regionCapacity > 0 && collector->regionCapacity < 1024);
    if (collector->regionCapacity == 0 || collector->regionCapacity >= 1024) return;
    /* Existing ordinary and explicit owners must survive the failed pin. */
    if (!ZrCore_Gc_RootLeaseAcquire(state, target, &lease)) { ++failures; return; }
    EXPECT(ZrCore_GarbageCollector_IgnoreObject(state, target));
    poolCount = collector->regionCapacity * 2 + 16;
    objects = (SZrRawObject **)calloc((size_t)poolCount, sizeof(*objects));
    pins = (SZrGcNativeCallPin *)calloc((size_t)poolCount, sizeof(*pins));
    EXPECT(objects != NULL && pins != NULL);
    if (objects == NULL || pins == NULL) goto cleanup;
    for (TZrSize i = 0; i < poolCount; ++i) {
        objects[i] = new_object(state);
        if (objects[i] == NULL) goto cleanup;
    }
    while (collector->regionCount < collector->regionCapacity && used < poolCount) {
        if (!ZrCore_Gc_NativeCallPinObject(state, objects[used], &pins[used])) {
            ++failures;
            goto cleanup;
        }
        ++used;
    }
    EXPECT(collector->regionCount == collector->regionCapacity);
    if (collector->regionCount != collector->regionCapacity) goto cleanup;
    /* Live pinned descriptors cannot be reused. Target root insertion already
     * exists, so the armed ARRAY request can only be region metadata growth. */
    oldCount = collector->regionCount;
    oldCapacity = collector->regionCapacity;
    oldRegions = collector->regions;
    oldNextId = collector->nextRegionId;
    oldRoots = collector->ignoredObjects;
    oldRootCount = collector->ignoredObjectCount;
    oldRootCapacity = collector->ignoredObjectCapacity;
    before = target->garbageCollectMark;
    savedRegions = (SZrGarbageCollectRegionDescriptor *)malloc(
            (size_t)oldCount * sizeof(*savedRegions));
    EXPECT(savedRegions != NULL);
    if (savedRegions == NULL) goto cleanup;
    memcpy(savedRegions, oldRegions, (size_t)oldCount * sizeof(*savedRegions));
    failArrayBytes = oldCapacity * 2 * sizeof(*oldRegions);
    fprintf(stderr, "arm region metadata reserve: count=%zu capacity=%zu bytes=%zu\n",
            (size_t)oldCount, (size_t)oldCapacity, (size_t)failArrayBytes);
    pinned = ZrCore_Gc_NativeCallPinObject(state, target, &pin);
    failArrayBytes = 0;
    EXPECT(reserveFailures == failuresBefore + 1 && !pinned);
    EXPECT(pin.object == NULL && pin.pinKind == ZR_GARBAGE_COLLECT_PIN_KIND_NONE &&
           !pin.ignoredAddedByCaller && !pin.pinKindAddedByCaller);
    EXPECT(lease.object == target);
    EXPECT(memcmp(&before, &target->garbageCollectMark, sizeof(before)) == 0);
    EXPECT(collector->regions == oldRegions && collector->regionCount == oldCount &&
           collector->regionCapacity == oldCapacity && collector->nextRegionId == oldNextId);
    EXPECT(memcmp(savedRegions, collector->regions,
                  (size_t)oldCount * sizeof(*savedRegions)) == 0);
    EXPECT(collector->ignoredObjects == oldRoots && collector->ignoredObjectCount == oldRootCount &&
           collector->ignoredObjectCapacity == oldRootCapacity);
    EXPECT(ZrCore_GarbageCollector_IsObjectIgnored(state->global, target));
    if (pinned) ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    pinned = ZrCore_Gc_NativeCallPinObject(state, target, &pin);
    EXPECT(pinned && pin.object == target);
    if (pinned) {
        EXPECT(target->garbageCollectMark.nativeCallPinRefCount == 1 &&
               target->garbageCollectMark.temporaryRootRefCount == 2);
        ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    }
    EXPECT(target->garbageCollectMark.nativeCallPinRefCount == 0 &&
           target->garbageCollectMark.temporaryRootRefCount == 1 &&
           target->garbageCollectMark.explicitIgnoredRoot);
cleanup:
    failArrayBytes = 0;
    for (TZrSize i = 0; i < used; ++i)
        ZrCore_Gc_NativeCallUnpin(state->global, &pins[i]);
    ZrCore_Gc_RootLeaseRelease(state->global, &lease);
    EXPECT(ZrCore_GarbageCollector_UnignoreObject(state->global, target));
    EXPECT(!ZrCore_GarbageCollector_IsObjectIgnored(state->global, target));
    free(savedRegions);
    free(pins);
    free(objects);
}

int main(int argc, char **argv) {
    SZrGlobalState *global;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    InitializeCriticalSection(&allocationLock);
#endif
    if (argc != 2) return 2;
    global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    EXPECT(global != NULL);
    if (global != NULL) {
        /* These tests retain native fixture addresses and do not request GC. */
        global->garbageCollector->stopGcFlag = ZR_TRUE;
        if (strcmp(argv[1], "explicit_before_pin") == 0) explicit_root(global->mainThreadState, 0);
        else if (strcmp(argv[1], "explicit_during_pin") == 0) explicit_root(global->mainThreadState, 1);
        else if (strcmp(argv[1], "same_object_threads") == 0) same_object_threads(global->mainThreadState);
        else if (strcmp(argv[1], "temporary_roots") == 0) temporary_roots(global->mainThreadState);
        else if (strcmp(argv[1], "distinct_root_threads") == 0) distinct_root_threads(global->mainThreadState);
        else if (strcmp(argv[1], "reserve_oom") == 0) reserve_oom(global->mainThreadState);
        else if (strcmp(argv[1], "region_reserve_oom") == 0) region_reserve_oom(global->mainThreadState);
        else ++failures;
        ZrCore_GlobalState_Free(global);
    }
    EXPECT(liveCount == 0 && !allocatorError);
#ifdef _WIN32
    DeleteCriticalSection(&allocationLock);
#else
    (void)pthread_mutex_destroy(&allocationLock);
#endif
    return failures == 0 ? 0 : 1;
}
