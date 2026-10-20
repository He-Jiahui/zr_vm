/* Exercise the documented protected FullGC failure boundary, using a callback
 * owned by this fixture. This is not a claim that arbitrary host callbacks may
 * allocate or throw while the collector scans them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#else
#include <errno.h>
#include <pthread.h>
#endif

#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/native.h"
#include "zr_vm_core/state.h"
#include "gc/gc_domain_internal.h"

typedef struct Allocation {
    void *pointer;
    TZrSize size;
    TZrInt64 flag;
} Allocation;

static Allocation allocations[32768];
static size_t liveCount;
static int allocatorError;
static int armed;
static int throwOnScan;
static unsigned scanCount;
static int fullReturned;

static TZrPtr tracked_allocator(TZrPtr argument, TZrPtr old, TZrSize oldSize,
                               TZrSize newSize, TZrInt64 flag) {
    size_t index = 0;
    void *next;
    (void)argument;
    if (old != NULL) {
        for (; index < liveCount && allocations[index].pointer != old; ++index) {}
        if (index == liveCount || allocations[index].size != oldSize ||
            allocations[index].flag != flag) {
            allocatorError = 1;
            return NULL;
        }
    } else if (oldSize != 0) {
        allocatorError = 1;
        return NULL;
    }
    if (newSize == 0) {
        if (old != NULL) {
            free(old);
            allocations[index] = allocations[--liveCount];
        }
        return NULL;
    }
    next = realloc(old, newSize);
    if (next == NULL) return NULL;
    if (old == NULL) {
        if (liveCount == sizeof(allocations) / sizeof(allocations[0])) abort();
        index = liveCount++;
    }
    allocations[index].pointer = next;
    allocations[index].size = newSize;
    allocations[index].flag = flag;
    return next;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s (live=%zu allocatorError=%d)\n", \
                __LINE__, #condition, liveCount, allocatorError); \
        return 1; \
    } \
} while (0)

static void build_error(SZrState *state, TZrPtr argument) {
    (void)argument;
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
}

static void scan_callback(SZrState *state, SZrRawObject *object) {
    (void)object;
    if (!armed) return;
    ++scanCount;
    if (throwOnScan) {
        /* A real Error already belongs to state. Throw must not construct it
         * inside the collector, nor can this callback recurse through GC. */
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    }
}

static void run_full(SZrState *state, TZrPtr argument) {
    (void)argument;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    fullReturned = 1;
}

typedef struct LockProbe {
    SZrGcDomain *domain;
    int acquired;
    int lockError;
} LockProbe;

/* This worker never attaches a mutator or waits for a VM safepoint. A single
 * nonblocking attempt distinguishes a leaked recursive lock from an unlocked
 * lock; trying on the collector thread would incorrectly pass in both cases. */
#ifdef _WIN32
static DWORD WINAPI probe_worker(LPVOID argument) {
    LockProbe *probe = (LockProbe *)argument;
    if (TryEnterCriticalSection(&probe->domain->mutationLock)) {
        probe->acquired = 1;
        LeaveCriticalSection(&probe->domain->mutationLock);
    }
    return 0;
}
#else
static void *probe_worker(void *argument) {
    LockProbe *probe = (LockProbe *)argument;
    int result = pthread_mutex_trylock(&probe->domain->mutationLock);
    if (result == 0) {
        probe->acquired = 1;
        probe->lockError = pthread_mutex_unlock(&probe->domain->mutationLock);
    } else if (result != EBUSY) {
        probe->lockError = result;
    }
    return NULL;
}
#endif

static int probe_and_join(LockProbe *probe) {
#ifdef _WIN32
    HANDLE worker = CreateThread(NULL, 0, probe_worker, probe, 0, NULL);
    DWORD waitResult;
    if (worker == NULL) return 0;
    waitResult = WaitForSingleObject(worker, 5000u);
    if (waitResult != WAIT_OBJECT_0) {
        /* Never unwind a borrowed context while a worker might still use it.
         * No TerminateThread and no claim of normal VM cleanup in this path. */
        fprintf(stderr, "nonblocking lock probe did not join: %lu\n", (unsigned long)waitResult);
        fflush(stderr);
        _Exit(3);
    }
    return CloseHandle(worker) != 0;
#else
    pthread_t worker;
    if (pthread_create(&worker, NULL, probe_worker, probe) != 0) return 0;
    if (pthread_join(worker, NULL) != 0) {
        fprintf(stderr, "lock probe join failed\n");
        fflush(stderr);
        _Exit(3);
    }
    return 1;
#endif
}

static int same_scopes(const SZrGcDomainScopeSnapshot *a,
                       const SZrGcDomainScopeSnapshot *b) {
    return a->domain == b->domain &&
           a->identity.id == b->identity.id &&
           a->identity.generation == b->identity.generation &&
           a->mutatorId == b->mutatorId &&
           a->executionDepth == b->executionDepth &&
           a->nativeDepth == b->nativeDepth &&
           a->mutationDepth == b->mutationDepth &&
           a->nativeMode == b->nativeMode &&
           a->nativeEnteredFromInactive == b->nativeEnteredFromInactive;
}

static int run_case(int generational, int shouldThrow) {
    SZrGlobalState *global = ZrCore_GlobalState_New(tracked_allocator, NULL, 1, NULL);
    SZrState *state;
    SZrGarbageCollector *collector;
    struct SZrNativeData *native;
    SZrGcRootHandle nativeRoot;
    SZrGcRootHandle errorRoot;
    SZrRawObject *resolved = NULL;
    SZrRawObject *resolvedError = NULL;
    SZrGcDomainScopeSnapshot beforeScopes, afterScopes;
    SZrGcDomainMutatorSnapshot beforePause, afterPause;
    SZrExecutionBudget budget;
    TZrSize rootCount;
    TZrUInt32 pauseDepth;
    SZrState *pauseOwner;
    TZrBool immediate;
    EZrThreadStatus status;
    LockProbe probe;
    int joined;
    int clean;

    CHECK(global != NULL && ZrCore_GlobalState_IsInitialized(global));
    state = global->mainThreadState;
    collector = global->garbageCollector;
    /* Set mode before introducing the fixture object, with no concurrent GC. */
    collector->gcMode = generational ? ZR_GARBAGE_COLLECT_MODE_GENERATIONAL
                                    : ZR_GARBAGE_COLLECT_MODE_INCREMENTAL;
    CHECK(ZrCore_Exception_TryRun(state, build_error, NULL) == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(state->hasCurrentException && state->currentException.type == ZR_VALUE_TYPE_OBJECT);
    CHECK(ZrCore_GcRootHandle_Create(state, state->currentException.value.object, &errorRoot));
    native = (struct SZrNativeData *)ZrCore_RawObject_New(
            state, ZR_VALUE_TYPE_NATIVE_DATA, sizeof(struct SZrNativeData), ZR_FALSE);
    CHECK(native != NULL);
    native->valueLength = 0u;
    native->super.scanMarkGcFunction = scan_callback;
    CHECK(ZrCore_GcRootHandle_Create(state, &native->super, &nativeRoot));
    CHECK(scanCount == 0u && !armed);
    memset(&budget, 0, sizeof(budget));
    state->executionBudget = &budget;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_GcDomain_CaptureScopes(state, &beforeScopes);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &beforePause);
    rootCount = state->gcDomain->activeRootCount;
    pauseDepth = state->gcDomain->pauseDepth;
    pauseOwner = state->gcDomain->collectorState;
    immediate = collector->isImmediateGcFlag;
    CHECK(!immediate);
    CHECK(!beforePause.pauseRequested);
    CHECK(pauseDepth == 0u);
    CHECK(pauseOwner == NULL);
    CHECK(!budget.hasDeadline && budget.termination == ZR_EXECUTION_TERMINATION_NONE);

    throwOnScan = shouldThrow;
    armed = 1;
    status = ZrCore_Exception_TryRun(state, run_full, NULL);
    armed = 0;
    /* Capture every observation and join before assertions. In particular a
     * RED must not destroy a domain with its mutation lock still owned. */
    ZrCore_GcDomain_CaptureScopes(state, &afterScopes);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &afterPause);
    memset(&probe, 0, sizeof(probe));
    probe.domain = state->gcDomain;
    joined = probe_and_join(&probe);
    clean = joined && probe.acquired && !probe.lockError &&
            afterPause.pauseRequested == beforePause.pauseRequested &&
            state->gcDomain->pauseDepth == pauseDepth &&
            state->gcDomain->collectorState == pauseOwner &&
            budget.gcDepth == 0u && collector->isImmediateGcFlag == immediate &&
            state->gcDomain->activeRootCount == rootCount &&
            same_scopes(&beforeScopes, &afterScopes);
    fprintf(stderr, "mode=%s throw=%d scans=%u status=%d returned=%d "
            "lock=%d joined=%d pause=%d depth=%u budget=%u termination=%d immediate=%d clean=%d\n",
            generational ? "generational" : "incremental", shouldThrow, scanCount,
            (int)status, fullReturned, probe.acquired, joined,
            (int)afterPause.pauseRequested, (unsigned)state->gcDomain->pauseDepth,
            (unsigned)budget.gcDepth, (int)budget.termination,
            (int)collector->isImmediateGcFlag, clean);
    CHECK(clean); /* On RED exit without unsafe VM destruction; not leak credit. */
    CHECK(scanCount > 0u);
    CHECK(status == (shouldThrow ? ZR_THREAD_STATUS_RUNTIME_ERROR : ZR_THREAD_STATUS_FINE));
    CHECK(fullReturned == !shouldThrow);
    CHECK(state->exceptionRecoverPoint == NULL);
    CHECK(state->hasCurrentException);
    CHECK(state->currentExceptionStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(ZrCore_GcRootHandle_Resolve(state, &errorRoot, &resolvedError));
    CHECK(state->currentException.value.object == resolvedError);
    if (shouldThrow) CHECK(state->threadStatus == ZR_THREAD_STATUS_RUNTIME_ERROR);
    CHECK(!budget.hasDeadline && budget.termination == ZR_EXECUTION_TERMINATION_NONE);

    /* Reuse the same collector after removing only the injected failure. */
    throwOnScan = 0;
    budget.hasDeadline = ZR_FALSE;
    budget.termination = ZR_EXECUTION_TERMINATION_NONE;
    fullReturned = 0;
    scanCount = 0u;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    armed = 1;
    status = ZrCore_Exception_TryRun(state, run_full, NULL);
    armed = 0;
    CHECK(status == ZR_THREAD_STATUS_FINE && fullReturned && scanCount > 0u);
    CHECK(budget.gcDepth == 0u && !collector->isImmediateGcFlag);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &afterPause);
    CHECK(afterPause.pauseRequested == beforePause.pauseRequested);
    CHECK(state->gcDomain->pauseDepth == pauseDepth);
    CHECK(state->gcDomain->collectorState == pauseOwner);
    CHECK(ZrCore_GcRootHandle_Resolve(state, &nativeRoot, &resolved));
    CHECK(resolved != NULL && resolved->type == ZR_RAW_OBJECT_TYPE_NATIVE_DATA);
    resolved->scanMarkGcFunction = NULL;
    ZrCore_GcRootHandle_Release(state, &nativeRoot);
    ZrCore_GcRootHandle_Release(state, &errorRoot);
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->executionBudget = NULL;
    ZrCore_GlobalState_Free(global);
    CHECK(liveCount == 0u && !allocatorError);
    return 0;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    if (strcmp(argv[1], "incremental_throw") == 0) return run_case(0, 1);
    if (strcmp(argv[1], "generational_throw") == 0) return run_case(1, 1);
    if (strcmp(argv[1], "incremental_control") == 0) return run_case(0, 0);
    if (strcmp(argv[1], "generational_control") == 0) return run_case(1, 0);
    return 2;
}
