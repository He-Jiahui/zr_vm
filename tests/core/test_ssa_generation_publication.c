#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_core/hotpatch_generation.h"
#include "zr_vm_core/hotpatch_publish.h"
#include "zr_vm_core/hotpatch_retire.h"

#include <stdio.h>
#include <string.h>

/* The registered stress case stays short; standalone race-sanitizer runs may
 * override this to exercise a longer publication sequence. */
#ifndef ZR_GENERATION_STRESS_PUBLISH_COUNT
#define ZR_GENERATION_STRESS_PUBLISH_COUNT 100u
#endif

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrGenerationTestThread;
typedef volatile LONG ZrGenerationTestAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrGenerationTestThread;
typedef _Atomic int ZrGenerationTestAtomic;
#endif

enum { ZR_GENERATION_RESOLVE_WAIT_MILLISECONDS = 1000 };

static void make_validated(SZrValidatedHotPatch *v, SZrArtifactExecIrView *a,
                           SZrHotPatchCapabilityManifest *m,
                           TZrUInt64 content, TZrUInt64 module);

typedef struct ZrGenerationResolveWorker {
    const SZrHotPatchGenerationManager *manager;
    const SZrHotPatchGenerationHandle *handle;
    ZrGenerationTestAtomic entered;
    ZrGenerationTestAtomic completed;
    EZrHotPatchGenerationStatus status;
    SZrHotPatchVersionView view;
    SZrHotPatchGenerationDiagnostic diagnostic;
} ZrGenerationResolveWorker;

static int generation_test_atomic_load(ZrGenerationTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void generation_test_atomic_store(ZrGenerationTestAtomic *value,
                                         int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void generation_test_atomic_init(ZrGenerationTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, 0);
#else
    atomic_init(value, 0);
#endif
}

static void generation_test_sleep_one_millisecond(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration = {0, 1000000L};
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static void generation_resolve_worker_run(ZrGenerationResolveWorker *worker) {
    generation_test_atomic_store(&worker->entered, 1);
    worker->status = ZrCore_HotPatch_Generation_Resolve(
            worker->manager, worker->handle, &worker->view,
            &worker->diagnostic);
    generation_test_atomic_store(&worker->completed, 1);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI generation_resolve_worker_entry(LPVOID argument) {
    generation_resolve_worker_run((ZrGenerationResolveWorker *)argument);
    return 0;
}

static TZrBool generation_resolve_worker_start(
        ZrGenerationTestThread *thread, ZrGenerationResolveWorker *worker) {
    *thread = CreateThread(ZR_NULL, 0u,
                           generation_resolve_worker_entry, worker, 0u,
                           ZR_NULL);
    return (TZrBool)(*thread != ZR_NULL);
}

static TZrBool generation_resolve_worker_join(ZrGenerationTestThread thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (TZrBool)(result == WAIT_OBJECT_0);
}
#else
static void *generation_resolve_worker_entry(void *argument) {
    generation_resolve_worker_run((ZrGenerationResolveWorker *)argument);
    return ZR_NULL;
}

static TZrBool generation_resolve_worker_start(
        ZrGenerationTestThread *thread, ZrGenerationResolveWorker *worker) {
    return (TZrBool)(pthread_create(thread, ZR_NULL,
                                    generation_resolve_worker_entry,
                                    worker) == 0);
}

static TZrBool generation_resolve_worker_join(ZrGenerationTestThread thread) {
    return (TZrBool)(pthread_join(thread, ZR_NULL) == 0);
}
#endif

static TZrBool generation_wait_for_worker_flag(ZrGenerationTestAtomic *flag) {
#if defined(ZR_PLATFORM_WIN)
    ULONGLONG deadline = GetTickCount64() +
            (ULONGLONG)ZR_GENERATION_RESOLVE_WAIT_MILLISECONDS;
    while (!generation_test_atomic_load(flag) &&
           GetTickCount64() < deadline) {
        generation_test_sleep_one_millisecond();
    }
#else
    struct timespec deadline;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return ZR_FALSE;
    deadline.tv_sec += ZR_GENERATION_RESOLVE_WAIT_MILLISECONDS / 1000u;
    deadline.tv_nsec +=
            (long)(ZR_GENERATION_RESOLVE_WAIT_MILLISECONDS % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    while (!generation_test_atomic_load(flag)) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
            now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec &&
             now.tv_nsec >= deadline.tv_nsec)) {
            break;
        }
        generation_test_sleep_one_millisecond();
    }
#endif
    return (TZrBool)generation_test_atomic_load(flag);
}

static int test_resolve_lock_gate_bounded_schedule_check(void) {
    SZrHotPatchGenerationManager manager;
    SZrHotPatchVersionRecord records[2];
    SZrHotPatchGenerationDiagnostic diagnostic;
    SZrValidatedHotPatch validated;
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityManifest manifest;
    SZrHotPatchGenerationHandle prepared, lease;
    ZrGenerationResolveWorker worker;
    ZrGenerationTestThread thread;
    TZrBool started = ZR_FALSE;
    TZrBool enteredWhileLocked = ZR_FALSE;
    TZrBool joined = ZR_FALSE;
    TZrBool completedWhileLocked = ZR_FALSE;
    TZrBool locked = ZR_FALSE;
    int failures = 0;

#define CHECK_RESOLVE_GATE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Resolve snapshot lock check failed at line %d: %s\n", \
                    __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

    memset(&prepared, 0, sizeof(prepared));
    memset(&lease, 0, sizeof(lease));
    memset(&worker, 0, sizeof(worker));
    if (ZrCore_HotPatch_GenerationManager_Init(
                &manager, records, 2u, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        return 1;
    }
    make_validated(&validated, &artifact, &manifest, 301u, 7u);
    if (ZrCore_HotPatch_Generation_Prepare(
                &manager, &validated, 7u, &prepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_Publish(
                &manager, &prepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_AcquireActive(
                &manager, &lease, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        ZrCore_HotPatch_GenerationManager_Deinit(&manager);
        return 1;
    }

    worker.manager = &manager;
    worker.handle = &lease;
    generation_test_atomic_init(&worker.entered);
    generation_test_atomic_init(&worker.completed);
    while (atomic_flag_test_and_set_explicit(&manager.lock,
                                              memory_order_acquire)) {
        generation_test_sleep_one_millisecond();
    }
    locked = ZR_TRUE;
    started = generation_resolve_worker_start(&thread, &worker);
    if (started) {
        enteredWhileLocked = generation_wait_for_worker_flag(&worker.entered);
        /* The entered flag is set immediately before the Resolve call, leaving
         * a scheduling window in which the worker may still be preempted. This
         * bounded observation complements the publish/resolve stress case. */
        completedWhileLocked = generation_wait_for_worker_flag(
                &worker.completed);
    }
    if (locked) {
        atomic_flag_clear_explicit(&manager.lock, memory_order_release);
        locked = ZR_FALSE;
    }
    if (started) joined = generation_resolve_worker_join(thread);

    CHECK_RESOLVE_GATE(started);
    CHECK_RESOLVE_GATE(enteredWhileLocked);
    CHECK_RESOLVE_GATE(generation_test_atomic_load(&worker.entered));
    CHECK_RESOLVE_GATE(!completedWhileLocked);
    CHECK_RESOLVE_GATE(joined);
    CHECK_RESOLVE_GATE(generation_test_atomic_load(&worker.completed));
    CHECK_RESOLVE_GATE(worker.status == ZR_HOT_PATCH_GENERATION_OK);
    CHECK_RESOLVE_GATE(worker.view.generation == lease.generation);
    CHECK_RESOLVE_GATE(worker.view.moduleHash == 7u);
    CHECK_RESOLVE_GATE(worker.view.contentHash == 301u);
    CHECK_RESOLVE_GATE(worker.view.publicContractHash == 55u);
    CHECK_RESOLVE_GATE(worker.view.state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_RESOLVE_GATE(worker.view.leaseCount == 1u);
    CHECK_RESOLVE_GATE(ZrCore_HotPatch_Generation_Release(
                &manager, &lease, &diagnostic) ==
            ZR_HOT_PATCH_GENERATION_OK);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
#undef CHECK_RESOLVE_GATE
    return failures;
}

typedef struct ZrGenerationResolveStressWorker {
    SZrHotPatchGenerationManager *manager;
    ZrGenerationTestAtomic ready;
    ZrGenerationTestAtomic resumeFirstResolve;
    ZrGenerationTestAtomic firstResolveComplete;
    ZrGenerationTestAtomic stop;
    TZrUInt32 resolveCount;
    TZrUInt32 retiredViewCount;
    EZrHotPatchGenerationStatus lastStatus;
    TZrUInt64 lastGeneration;
    TZrUInt64 lastContentHash;
    EZrHotPatchVersionState lastState;
    TZrUInt32 lastLeaseCount;
    int failures;
} ZrGenerationResolveStressWorker;

static void generation_resolve_stress_worker_run(
        ZrGenerationResolveStressWorker *worker) {
    TZrBool firstResolve = ZR_TRUE;
    while (!generation_test_atomic_load(&worker->stop)) {
        SZrHotPatchGenerationHandle lease;
        SZrHotPatchVersionView view;
        SZrHotPatchGenerationDiagnostic diagnostic;
        EZrHotPatchGenerationStatus status;
        memset(&lease, 0, sizeof(lease));
        memset(&view, 0, sizeof(view));
        status = ZrCore_HotPatch_Generation_AcquireActive(
                worker->manager, &lease, &diagnostic);
        if (status != ZR_HOT_PATCH_GENERATION_OK) {
            ++worker->failures;
            break;
        }
        if (firstResolve) {
            generation_test_atomic_store(&worker->ready, 1);
            while (!generation_test_atomic_load(
                           &worker->resumeFirstResolve) &&
                   !generation_test_atomic_load(&worker->stop)) {
                generation_test_sleep_one_millisecond();
            }
            if (generation_test_atomic_load(&worker->stop)) {
                if (ZrCore_HotPatch_Generation_Release(
                            worker->manager, &lease, &diagnostic) !=
                        ZR_HOT_PATCH_GENERATION_OK) {
                    ++worker->failures;
                }
                break;
            }
        }
        status = ZrCore_HotPatch_Generation_Resolve(
                worker->manager, &lease, &view, &diagnostic);
        worker->lastStatus = status;
        worker->lastGeneration = view.generation;
        worker->lastContentHash = view.contentHash;
        worker->lastState = view.state;
        worker->lastLeaseCount = view.leaseCount;
        if (status != ZR_HOT_PATCH_GENERATION_OK ||
            view.generation != lease.generation ||
            view.moduleHash != 7u ||
            view.contentHash != view.generation + 300u ||
            view.publicContractHash != 55u || view.targetProfile != 2u ||
            (view.state != ZR_HOT_PATCH_VERSION_ACTIVE &&
             view.state != ZR_HOT_PATCH_VERSION_RETIRED) ||
            view.leaseCount == 0u) {
            ++worker->failures;
        }
        if (status == ZR_HOT_PATCH_GENERATION_OK &&
            view.state == ZR_HOT_PATCH_VERSION_RETIRED) {
            ++worker->retiredViewCount;
        }
        if (ZrCore_HotPatch_Generation_Release(
                    worker->manager, &lease, &diagnostic) !=
                ZR_HOT_PATCH_GENERATION_OK) {
            ++worker->failures;
            break;
        }
        ++worker->resolveCount;
        if (firstResolve) {
            firstResolve = ZR_FALSE;
            generation_test_atomic_store(&worker->firstResolveComplete, 1);
        }
        if (!generation_test_atomic_load(&worker->ready)) {
            generation_test_atomic_store(&worker->ready, 1);
        }
    }
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI generation_resolve_stress_worker_entry(LPVOID argument) {
    generation_resolve_stress_worker_run(
            (ZrGenerationResolveStressWorker *)argument);
    return 0;
}

static TZrBool generation_resolve_stress_worker_start(
        ZrGenerationTestThread *thread,
        ZrGenerationResolveStressWorker *worker) {
    *thread = CreateThread(ZR_NULL, 0u,
                           generation_resolve_stress_worker_entry, worker, 0u,
                           ZR_NULL);
    return (TZrBool)(*thread != ZR_NULL);
}

static TZrBool generation_resolve_stress_worker_join(
        ZrGenerationTestThread thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (TZrBool)(result == WAIT_OBJECT_0);
}
#else
static void *generation_resolve_stress_worker_entry(void *argument) {
    generation_resolve_stress_worker_run(
            (ZrGenerationResolveStressWorker *)argument);
    return ZR_NULL;
}

static TZrBool generation_resolve_stress_worker_start(
        ZrGenerationTestThread *thread,
        ZrGenerationResolveStressWorker *worker) {
    return (TZrBool)(pthread_create(thread, ZR_NULL,
                                    generation_resolve_stress_worker_entry,
                                    worker) == 0);
}

static TZrBool generation_resolve_stress_worker_join(
        ZrGenerationTestThread thread) {
    return (TZrBool)(pthread_join(thread, ZR_NULL) == 0);
}
#endif

static int test_concurrent_resolve_and_publish_snapshots(void) {
    enum {
        PUBLISH_COUNT = ZR_GENERATION_STRESS_PUBLISH_COUNT,
        MAX_ATTEMPTS = PUBLISH_COUNT * 20u
    };
    SZrHotPatchGenerationManager manager;
    SZrHotPatchVersionRecord records[4];
    SZrHotPatchGenerationDiagnostic diagnostic;
    SZrValidatedHotPatch validated;
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityManifest manifest;
    SZrHotPatchGenerationHandle prepared;
    ZrGenerationResolveStressWorker worker;
    ZrGenerationTestThread thread;
    TZrUInt32 published = 0u;
    TZrUInt32 attempts = 0u;
    TZrUInt32 capacityRetries = 0u;
    TZrUInt32 collected = 0u;
    TZrBool started = ZR_FALSE;
    TZrBool joined = ZR_FALSE;
    int failures = 0;

    memset(&prepared, 0, sizeof(prepared));
    memset(&worker, 0, sizeof(worker));
    if (ZrCore_HotPatch_GenerationManager_Init(
                &manager, records, 4u, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        return 1;
    }
    make_validated(&validated, &artifact, &manifest, 301u, 7u);
    if (ZrCore_HotPatch_Generation_Prepare(
                &manager, &validated, 7u, &prepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_Publish(
                &manager, &prepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        fputs("Resolve stress setup could not prepare/publish generation 1\n",
              stderr);
        ZrCore_HotPatch_GenerationManager_Deinit(&manager);
        return 1;
    }

    worker.manager = &manager;
    generation_test_atomic_init(&worker.ready);
    generation_test_atomic_init(&worker.resumeFirstResolve);
    generation_test_atomic_init(&worker.firstResolveComplete);
    generation_test_atomic_init(&worker.stop);
    started = generation_resolve_stress_worker_start(&thread, &worker);
    if (!started || !generation_wait_for_worker_flag(&worker.ready)) {
        fprintf(stderr, "Resolve stress reader startup failed: started=%d ready=%d\n",
                (int)started, generation_test_atomic_load(&worker.ready));
        failures = 1;
        goto cleanup;
    }

    while (published < PUBLISH_COUNT && attempts < MAX_ATTEMPTS) {
        TZrUInt64 nextGeneration = (TZrUInt64)published + 2u;
        EZrHotPatchGenerationStatus status;
        ++attempts;
        validated.contentHash = 300u + nextGeneration;
        validated.patchId = nextGeneration;
        manifest.patchId = nextGeneration;
        status = ZrCore_HotPatch_Generation_Prepare(
                &manager, &validated, 7u, &prepared, &diagnostic);
        if (status == ZR_HOT_PATCH_GENERATION_CAPACITY) {
            ++capacityRetries;
            if (ZrCore_HotPatch_Generation_CollectRetired(
                        &manager, &collected, &diagnostic) !=
                    ZR_HOT_PATCH_GENERATION_OK) {
                fprintf(stderr,
                        "Resolve stress collect on full capacity failed: published=%u attempts=%u status=%d\n",
                        published, attempts, (int)diagnostic.status);
                ++failures;
                break;
            }
            generation_test_sleep_one_millisecond();
            continue;
        }
        if (status != ZR_HOT_PATCH_GENERATION_OK ||
            prepared.generation != nextGeneration ||
            ZrCore_HotPatch_Generation_Publish(
                    &manager, &prepared, &diagnostic) !=
                ZR_HOT_PATCH_GENERATION_OK) {
            fprintf(stderr,
                    "Resolve stress publish failed: published=%u attempts=%u prepare/publish status=%d diagnostic=%d generation=%llu expected=%llu\n",
                    published, attempts, (int)status, (int)diagnostic.status,
                    (unsigned long long)prepared.generation,
                    (unsigned long long)nextGeneration);
            ++failures;
            break;
        }
        ++published;
        if (published == 1u) {
            generation_test_atomic_store(&worker.resumeFirstResolve, 1);
            if (!generation_wait_for_worker_flag(
                        &worker.firstResolveComplete)) {
                fprintf(stderr,
                        "Resolve stress first retired snapshot timed out before reader acknowledgement\n");
                ++failures;
            }
        }
        if (ZrCore_HotPatch_Generation_CollectRetired(
                    &manager, &collected, &diagnostic) !=
                ZR_HOT_PATCH_GENERATION_OK) {
            ++failures;
            break;
        }
    }
    if (published != PUBLISH_COUNT) {
        fprintf(stderr,
                "Resolve stress publication bound exhausted: published=%u expected=%u attempts=%u\n",
                published, (TZrUInt32)PUBLISH_COUNT, attempts);
        ++failures;
    }

cleanup:
    generation_test_atomic_store(&worker.stop, 1);
    if (started) joined = generation_resolve_stress_worker_join(thread);
    if (!joined || worker.resolveCount == 0u || worker.retiredViewCount == 0u ||
        worker.failures != 0) {
        fprintf(stderr,
                "Resolve stress reader result: joined=%d resolves=%u retired=%u failures=%d lastStatus=%d generation=%llu content=%llu state=%u leases=%u\n",
                (int)joined, worker.resolveCount, worker.retiredViewCount,
                worker.failures, (int)worker.lastStatus,
                (unsigned long long)worker.lastGeneration,
                (unsigned long long)worker.lastContentHash,
                (unsigned)worker.lastState, worker.lastLeaseCount);
        ++failures;
    }
    if (ZrCore_HotPatch_Generation_CollectRetired(
                &manager, &collected, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        ++failures;
    }
    fprintf(stderr,
            "Resolve stress metrics: published=%u attempts=%u capacity-retries=%u reader-resolves=%u retired-views=%u worker-failures=%d\n",
            published, attempts, capacityRetries, worker.resolveCount,
            worker.retiredViewCount, worker.failures);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return failures;
}

static void make_validated(SZrValidatedHotPatch *v, SZrArtifactExecIrView *a,
                           SZrHotPatchCapabilityManifest *m,
                           TZrUInt64 content, TZrUInt64 module) {
    memset(v, 0, sizeof(*v)); memset(a, 0, sizeof(*a)); memset(m, 0, sizeof(*m));
    a->moduleHash = module; a->buffer = (const TZrByte *)"x"; a->bufferLength = 1u;
    m->publicContractHash = 55u;
    m->patchId = content;
    v->artifact = a; v->manifest = m; v->contentHash = content;
    v->contentBytes = a->buffer; v->contentLength = a->bufferLength;
    v->patchId = m->patchId; v->publicContractHash = m->publicContractHash;
    v->targetProfile = 2u; v->signatureVerified = ZR_TRUE;
    v->immutableContent = ZR_TRUE;
}

static int test_cross_manager_handles_are_rejected(void) {
    SZrHotPatchGenerationManager ownerManager, otherManager;
    SZrHotPatchVersionRecord ownerRecords[2], otherRecords[2];
    SZrHotPatchGenerationDiagnostic diagnostic;
    SZrValidatedHotPatch ownerValidated, otherValidated;
    SZrArtifactExecIrView ownerArtifact, otherArtifact;
    SZrHotPatchCapabilityManifest ownerManifest, otherManifest;
    SZrHotPatchGenerationHandle ownerPrepared, otherPrepared, ownerLease;
    SZrHotPatchVersionView view;
    EZrHotPatchGenerationStatus status;
    int failures = 0;

#define CHECK_HANDLE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "cross-manager handle check failed at line %d: %s\n", \
                    __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

    if (ZrCore_HotPatch_GenerationManager_Init(
                &ownerManager, ownerRecords, 2u, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_GenerationManager_Init(
                &otherManager, otherRecords, 2u, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        return 1;
    }

    make_validated(&ownerValidated, &ownerArtifact, &ownerManifest, 101u, 7u);
    make_validated(&otherValidated, &otherArtifact, &otherManifest, 202u, 7u);
    if (ZrCore_HotPatch_Generation_Prepare(
                &ownerManager, &ownerValidated, 7u, &ownerPrepared,
                &diagnostic) != ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_Prepare(
                &otherManager, &otherValidated, 7u, &otherPrepared,
                &diagnostic) != ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_Publish(
                &otherManager, &otherPrepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        return 1;
    }

    /* Both managers use generation 1, so ownership must be checked before
     * interpreting a record's fields or changing either manager. */
    status = ZrCore_HotPatch_Generation_Publish(
            &otherManager, &ownerPrepared, &diagnostic);
    CHECK_HANDLE(status == ZR_HOT_PATCH_GENERATION_NOT_PREPARED);
    CHECK_HANDLE(diagnostic.status == ZR_HOT_PATCH_GENERATION_NOT_PREPARED);
    CHECK_HANDLE(diagnostic.expectedGeneration == ownerPrepared.generation);
    CHECK_HANDLE(diagnostic.actualGeneration == 0u);
    CHECK_HANDLE(ownerPrepared.record == &ownerRecords[0]);
    CHECK_HANDLE(ownerPrepared.leased == ZR_FALSE);
    CHECK_HANDLE(ownerManager.count == 1u);
    CHECK_HANDLE(ownerRecords[0].state == ZR_HOT_PATCH_VERSION_PREPARED);
    CHECK_HANDLE(atomic_load_explicit(&ownerManager.active,
                                      memory_order_acquire) == ZR_NULL);
    CHECK_HANDLE(otherRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(otherManager.count == 1u);
    CHECK_HANDLE(atomic_load_explicit(&otherManager.active,
                                      memory_order_acquire) == &otherRecords[0]);

    if (ZrCore_HotPatch_Generation_Publish(
                &ownerManager, &ownerPrepared, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK ||
        ZrCore_HotPatch_Generation_AcquireActive(
                &ownerManager, &ownerLease, &diagnostic) !=
            ZR_HOT_PATCH_GENERATION_OK) {
        return failures + 1;
    }
    CHECK_HANDLE(ownerRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(atomic_load_explicit(&ownerManager.active,
                                      memory_order_acquire) == &ownerRecords[0]);

    memset(&view, 0, sizeof(view));
    status = ZrCore_HotPatch_Generation_Resolve(
            &otherManager, &ownerLease, &view, &diagnostic);
    CHECK_HANDLE(status == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    CHECK_HANDLE(diagnostic.status == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    CHECK_HANDLE(diagnostic.expectedGeneration == ownerLease.generation);
    CHECK_HANDLE(diagnostic.actualGeneration == 0u);
    CHECK_HANDLE(atomic_load_explicit(&ownerRecords[0].leaseCount,
                                     memory_order_acquire) == 1u);
    CHECK_HANDLE(ownerRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(atomic_load_explicit(&ownerManager.active,
                                      memory_order_acquire) == &ownerRecords[0]);
    CHECK_HANDLE(ownerManager.count == 1u);
    CHECK_HANDLE(atomic_load_explicit(&otherRecords[0].leaseCount,
                                     memory_order_acquire) == 0u);
    CHECK_HANDLE(otherRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(otherManager.count == 1u);

    status = ZrCore_HotPatch_Generation_Release(
            &otherManager, &ownerLease, &diagnostic);
    CHECK_HANDLE(status == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    CHECK_HANDLE(diagnostic.status == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    CHECK_HANDLE(diagnostic.expectedGeneration == ownerLease.generation);
    CHECK_HANDLE(diagnostic.actualGeneration == 0u);
    CHECK_HANDLE(ownerLease.leased == ZR_TRUE);
    CHECK_HANDLE(ownerLease.record == &ownerRecords[0]);
    CHECK_HANDLE(atomic_load_explicit(&ownerRecords[0].leaseCount,
                                     memory_order_acquire) == 1u);
    CHECK_HANDLE(ownerRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(atomic_load_explicit(&ownerManager.active,
                                      memory_order_acquire) == &ownerRecords[0]);
    CHECK_HANDLE(otherRecords[0].state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_HANDLE(ownerManager.count == 1u);
    CHECK_HANDLE(otherManager.count == 1u);

    status = ZrCore_HotPatch_Generation_Resolve(
            &ownerManager, &ownerLease, &view, &diagnostic);
    CHECK_HANDLE(status == ZR_HOT_PATCH_GENERATION_OK);
    CHECK_HANDLE(view.contentHash == 101u);
    status = ZrCore_HotPatch_Generation_Release(
            &ownerManager, &ownerLease, &diagnostic);
    CHECK_HANDLE(status == ZR_HOT_PATCH_GENERATION_OK);
    CHECK_HANDLE(atomic_load_explicit(&ownerRecords[0].leaseCount,
                                     memory_order_acquire) == 0u);

    ZrCore_HotPatch_GenerationManager_Deinit(&otherManager);
    ZrCore_HotPatch_GenerationManager_Deinit(&ownerManager);
#undef CHECK_HANDLE
    return failures;
}

int main(void) {
    SZrHotPatchGenerationManager manager;
    SZrHotPatchVersionRecord records[3];
    SZrHotPatchGenerationDiagnostic d;
    SZrValidatedHotPatch v1, v2;
    SZrArtifactExecIrView a1, a2;
    SZrHotPatchCapabilityManifest m1, m2;
    SZrHotPatchGenerationHandle p1, p2, oldFrame, newCall;
    SZrHotPatchVersionView view;
    TZrUInt32 collected;
    EZrHotPatchGenerationStatus staleAcquireStatus;
    TZrBool managerInitialized = ZR_FALSE;
    TZrBool oldFrameLease = ZR_FALSE;
    TZrBool newCallLease = ZR_FALSE;
    int failures = 0;

#define CHECK_GENERATION_STATUS(expression, expected) \
    do { \
        EZrHotPatchGenerationStatus actualStatus_ = (expression); \
        if (actualStatus_ != (expected)) { \
            fprintf(stderr, \
                    "generation check failed at line %d: %s expected status %d, got %d\n", \
                    __LINE__, #expression, (int)(expected), \
                    (int)actualStatus_); \
            ++failures; \
            goto cleanup; \
        } \
    } while (0)

#define CHECK_GENERATION_VALUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, \
                    "generation check failed at line %d: %s\n", \
                    __LINE__, #condition); \
            ++failures; \
            goto cleanup; \
        } \
    } while (0)

    if (test_concurrent_resolve_and_publish_snapshots() != 0) return 1;
    if (test_resolve_lock_gate_bounded_schedule_check() != 0) return 1;
    if (test_cross_manager_handles_are_rejected() != 0) return 1;

    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_GenerationManager_Init(
                    &manager, records, 3u, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    managerInitialized = ZR_TRUE;
    make_validated(&v1, &a1, &m1, 101u, 7u);
    make_validated(&v2, &a2, &m2, 202u, 7u);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Prepare(
                    &manager, &v1, 7u, &p1, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_PublishPrepared(&manager, &p1, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_AcquireActive(
                    &manager, &oldFrame, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    oldFrameLease = ZR_TRUE;
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Prepare(
                    &manager, &v2, 7u, &p2, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Publish(&manager, &p2, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_AcquireActive(
                    &manager, &newCall, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    newCallLease = ZR_TRUE;
    CHECK_GENERATION_VALUE(oldFrame.generation != newCall.generation);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Resolve(
                    &manager, &oldFrame, &view, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_VALUE(view.contentHash == 101u &&
                           view.state == ZR_HOT_PATCH_VERSION_RETIRED);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Resolve(
                    &manager, &newCall, &view, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_VALUE(view.contentHash == 202u &&
                           view.state == ZR_HOT_PATCH_VERSION_ACTIVE);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Release(&manager, &newCall, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    newCallLease = ZR_FALSE;
    collected = 0u;
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_RetireCollect(&manager, &collected, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_VALUE(collected == 0u);
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_Generation_Release(&manager, &oldFrame, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    oldFrameLease = ZR_FALSE;
    CHECK_GENERATION_STATUS(
            ZrCore_HotPatch_CollectRetired(&manager, &collected, &d),
            ZR_HOT_PATCH_GENERATION_OK);
    CHECK_GENERATION_VALUE(collected == 1u);
    staleAcquireStatus = ZrCore_HotPatch_Generation_Acquire(
            &manager, 1u, &newCall, &d);
    newCallLease = (TZrBool)newCall.leased;
    CHECK_GENERATION_STATUS(
            staleAcquireStatus,
            ZR_HOT_PATCH_GENERATION_STALE_LINK);

cleanup:
    if (managerInitialized) {
        if (newCallLease &&
            ZrCore_HotPatch_Generation_Release(
                    &manager, &newCall, &d) != ZR_HOT_PATCH_GENERATION_OK) {
            fputs("cleanup could not release the new-call lease\n", stderr);
            ++failures;
        }
        if (oldFrameLease &&
            ZrCore_HotPatch_Generation_Release(
                    &manager, &oldFrame, &d) != ZR_HOT_PATCH_GENERATION_OK) {
            fputs("cleanup could not release the old-frame lease\n", stderr);
            ++failures;
        }
        ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    }
#undef CHECK_GENERATION_VALUE
#undef CHECK_GENERATION_STATUS
    return failures;
}
