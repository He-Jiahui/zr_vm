/* Prepared-candidate cancellation fixtures. Included after make_validated. */
#include <stdlib.h>

enum { ZR_DISCARD_TEST_SLOTS = 4, ZR_DISCARD_TEST_REPETITIONS = 16 };

typedef struct ZrDiscardFixture {
    SZrHotPatchGenerationManager manager;
    SZrHotPatchVersionRecord records[ZR_DISCARD_TEST_SLOTS];
    SZrValidatedHotPatch validated;
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityManifest manifest;
    SZrHotPatchGenerationHandle leases[2];
    TZrBool initialized;
} ZrDiscardFixture;

typedef struct ZrDiscardSnapshot {
    SZrHotPatchVersionRecord *active;
    SZrHotPatchVersionRecord *records;
    TZrUInt64 next;
    TZrUInt32 capacity, count;
    SZrHotPatchVersionView views[ZR_DISCARD_TEST_SLOTS];
    uint_fast32_t leases[ZR_DISCARD_TEST_SLOTS];
} ZrDiscardSnapshot;

static int discard_fixture_init(ZrDiscardFixture *f, TZrUInt32 capacity) {
    memset(f, 0, sizeof(*f));
    if (ZrCore_HotPatch_GenerationManager_Init(
                &f->manager, f->records, capacity, ZR_NULL) !=
            ZR_HOT_PATCH_GENERATION_OK) return 1;
    f->initialized = ZR_TRUE;
    make_validated(&f->validated, &f->artifact, &f->manifest, 101u, 7u);
    return 0;
}

static int discard_fixture_cleanup(ZrDiscardFixture *f) {
    int failures = 0;
    if (!f->initialized) return 0;
    for (TZrUInt32 i = 0u; i < 2u; ++i) {
        if (f->leases[i].leased &&
            ZrCore_HotPatch_Generation_Release(
                    &f->manager, &f->leases[i], ZR_NULL) !=
                ZR_HOT_PATCH_GENERATION_OK) {
            fputs("discard cleanup could not release real lease\n", stderr);
            ++failures;
        }
    }
    ZrCore_HotPatch_GenerationManager_Deinit(&f->manager);
    return failures;
}

static void discard_snapshot(const ZrDiscardFixture *f, ZrDiscardSnapshot *s) {
    s->active = atomic_load_explicit(&f->manager.active, memory_order_acquire);
    s->records = f->manager.records;
    s->next = atomic_load_explicit(&f->manager.nextGeneration,
                                   memory_order_relaxed);
    s->capacity = f->manager.capacity;
    s->count = f->manager.count;
    for (TZrUInt32 i = 0u; i < s->capacity; ++i) {
        const SZrHotPatchVersionRecord *r = &f->records[i];
        s->views[i].generation = r->generation;
        s->views[i].moduleHash = r->moduleHash;
        s->views[i].contentHash = r->contentHash;
        s->views[i].publicContractHash = r->publicContractHash;
        s->views[i].targetProfile = r->targetProfile;
        s->views[i].state = (EZrHotPatchVersionState)r->state;
        s->leases[i] = atomic_load_explicit(&r->leaseCount,
                                           memory_order_relaxed);
    }
}

static TZrBool discard_snapshot_matches(const ZrDiscardFixture *f,
                                        const ZrDiscardSnapshot *s) {
    if (atomic_load_explicit(&f->manager.active, memory_order_acquire) !=
                s->active || f->manager.records != s->records ||
        atomic_load_explicit(&f->manager.nextGeneration,
                             memory_order_relaxed) != s->next ||
        f->manager.capacity != s->capacity || f->manager.count != s->count)
        return ZR_FALSE;
    for (TZrUInt32 i = 0u; i < s->capacity; ++i) {
        const SZrHotPatchVersionRecord *r = &f->records[i];
        const SZrHotPatchVersionView *v = &s->views[i];
        if (r->generation != v->generation || r->moduleHash != v->moduleHash ||
            r->contentHash != v->contentHash ||
            r->publicContractHash != v->publicContractHash ||
            r->targetProfile != v->targetProfile || r->state != v->state ||
            atomic_load_explicit(&r->leaseCount, memory_order_relaxed) !=
                s->leases[i]) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool discard_handle_equal(const SZrHotPatchGenerationHandle *a,
                                    const SZrHotPatchGenerationHandle *b) {
    return (TZrBool)(a->record == b->record && a->generation == b->generation &&
                    a->leased == b->leased);
}

static TZrBool discard_record_is_clear(const SZrHotPatchVersionRecord *r) {
    return (TZrBool)(r->generation == 0u && r->moduleHash == 0u &&
                    r->contentHash == 0u && r->publicContractHash == 0u &&
                    r->targetProfile == 0u && r->state == ZR_HOT_PATCH_VERSION_FREE &&
                    atomic_load_explicit(&r->leaseCount,
                                         memory_order_relaxed) == 0u);
}

#define DISCARD_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "discard check failed at line %d: %s\n", \
                    __LINE__, #condition); \
            ++failures; \
            goto cleanup; \
        } \
    } while (0)

static int test_discard_slot_reuse_and_aba(void) {
    ZrDiscardFixture f;
    SZrHotPatchGenerationHandle prepared, oldCopy, replacement, empty = {0};
    SZrHotPatchGenerationDiagnostic d;
    ZrDiscardSnapshot before;
    TZrUInt32 collected;
    TZrUInt64 generation;
    int failures = 0;
    if (discard_fixture_init(&f, 1u) != 0) return 1;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &prepared, &d) ==
            ZR_HOT_PATCH_GENERATION_OK);
    oldCopy = prepared;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_CollectRetired(
            &f.manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(collected == 0u);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &replacement, &d) ==
            ZR_HOT_PATCH_GENERATION_CAPACITY);
    discard_snapshot(&f, &before);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &prepared, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(d.status == ZR_HOT_PATCH_GENERATION_OK &&
                  d.expectedGeneration == 0u &&
                  d.actualGeneration == oldCopy.generation && d.leaseCount == 0u);
    DISCARD_CHECK(discard_record_is_clear(&f.records[0]));
    DISCARD_CHECK(discard_handle_equal(&prepared, &empty));
    DISCARD_CHECK(f.manager.count == before.count &&
                  atomic_load(&f.manager.nextGeneration) == before.next &&
                  atomic_load(&f.manager.active) == before.active);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &prepared, &d) == ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &oldCopy, &d) == ZR_HOT_PATCH_GENERATION_NOT_PREPARED);
    DISCARD_CHECK(d.actualGeneration == 0u &&
                  d.expectedGeneration == oldCopy.generation);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &replacement, &d) ==
            ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(replacement.record == oldCopy.record &&
                  replacement.generation > oldCopy.generation);
    discard_snapshot(&f, &before);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &oldCopy, &d) == ZR_HOT_PATCH_GENERATION_NOT_PREPARED);
    DISCARD_CHECK(d.actualGeneration == replacement.generation &&
                  d.expectedGeneration == oldCopy.generation);
    DISCARD_CHECK(discard_snapshot_matches(&f, &before));
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Publish(
            &f.manager, &oldCopy, &d) == ZR_HOT_PATCH_GENERATION_NOT_PREPARED);
    DISCARD_CHECK(discard_snapshot_matches(&f, &before));
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Publish(
            &f.manager, &replacement, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(atomic_load(&f.manager.active) == replacement.record);
cleanup:
    failures += discard_fixture_cleanup(&f);
    if (failures != 0) return failures;

    if (discard_fixture_init(&f, 1u) != 0) return 1;
    generation = 0u;
    for (TZrUInt32 i = 0u; i < ZR_DISCARD_TEST_REPETITIONS; ++i) {
        DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
                &f.manager, &f.validated, 7u, &prepared, &d) ==
                ZR_HOT_PATCH_GENERATION_OK);
        DISCARD_CHECK(prepared.generation > generation);
        generation = prepared.generation;
        DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
                &f.manager, &prepared, ZR_NULL) == ZR_HOT_PATCH_GENERATION_OK);
        DISCARD_CHECK(f.manager.count == 1u &&
                      atomic_load(&f.manager.nextGeneration) == generation &&
                      atomic_load(&f.manager.active) == ZR_NULL);
    }
    return discard_fixture_cleanup(&f);
}

static int discard_expect_unchanged(ZrDiscardFixture *f,
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *handle, EZrHotPatchGenerationStatus expected,
        TZrUInt64 expectedGeneration, TZrUInt64 actualGeneration,
        TZrUInt32 leases) {
    ZrDiscardSnapshot before;
    SZrHotPatchGenerationHandle original = {0};
    SZrHotPatchGenerationDiagnostic d;
    discard_snapshot(f, &before);
    if (handle) original = *handle;
    EZrHotPatchGenerationStatus status = ZrCore_HotPatch_Generation_DiscardPrepared(
            manager, handle, &d);
    if (status != expected || d.status != expected ||
        d.expectedGeneration != expectedGeneration ||
        d.actualGeneration != actualGeneration || d.leaseCount != leases ||
        !discard_snapshot_matches(f, &before) ||
        (handle && !discard_handle_equal(handle, &original))) {
        fprintf(stderr, "discard rejection expected=%d got=%d\n",
                (int)expected, (int)status);
        return 1;
    }
    return 0;
}

static int test_discard_guards_and_live_versions(void) {
    ZrDiscardFixture f, other;
    SZrHotPatchGenerationHandle old, active, prepared, foreign, wrong, empty = {0};
    SZrHotPatchGenerationDiagnostic d;
    SZrHotPatchVersionView view;
    ZrDiscardSnapshot before, otherBefore;
    TZrUInt32 collected;
    int failures = 0;
    memset(&other, 0, sizeof(other));
    if (discard_fixture_init(&f, 4u) != 0) return 1;
    DISCARD_CHECK(discard_fixture_init(&other, 4u) == 0);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &old, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Publish(
            &f.manager, &old, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_AcquireActive(
            &f.manager, &f.leases[0], &d) == ZR_HOT_PATCH_GENERATION_OK);
    f.validated.contentHash = 202u;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &active, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Publish(
            &f.manager, &active, &d) == ZR_HOT_PATCH_GENERATION_OK);
    f.validated.contentHash = 303u;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &prepared, &d) == ZR_HOT_PATCH_GENERATION_OK);
    for (TZrUInt32 i = 0u; i < 3u; ++i) {
        DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
                &other.manager, &other.validated, 7u, &foreign, &d) ==
                ZR_HOT_PATCH_GENERATION_OK);
    }
    DISCARD_CHECK(foreign.generation == prepared.generation);
    discard_snapshot(&other, &otherBefore);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &foreign,
            ZR_HOT_PATCH_GENERATION_NOT_PREPARED, foreign.generation, 0u, 0u) == 0);
    DISCARD_CHECK(discard_snapshot_matches(&other, &otherBefore));
    DISCARD_CHECK(discard_expect_unchanged(&f, ZR_NULL, &prepared,
            ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, 0u, 0u, 0u) == 0);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, ZR_NULL,
            ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, 0u, 0u, 0u) == 0);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &empty,
            ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, 0u, 0u, 0u) == 0);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &active,
            ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
            active.generation, active.generation, 0u) == 0);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &old,
            ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
            old.generation, old.generation, 0u) == 0);
    wrong = prepared;
    ++wrong.generation;
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &wrong,
            ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
            wrong.generation, prepared.generation, 0u) == 0);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Acquire(
            &f.manager, prepared.generation, &f.leases[1], &d) ==
            ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &f.leases[1],
            ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT, 0u, 0u, 0u) == 0);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &prepared,
            ZR_HOT_PATCH_GENERATION_INVALID_STATE,
            prepared.generation, prepared.generation, 1u) == 0);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Resolve(
            &f.manager, &f.leases[1], &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(view.state == ZR_HOT_PATCH_VERSION_PREPARED &&
                  view.contentHash == 303u && view.leaseCount == 1u);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Release(
            &f.manager, &f.leases[1], &d) == ZR_HOT_PATCH_GENERATION_OK);
    discard_snapshot(&f, &before);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &prepared, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(atomic_load(&f.manager.active) == before.active &&
                  atomic_load(&f.manager.nextGeneration) == before.next &&
                  f.manager.count == before.count);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Resolve(
            &f.manager, &f.leases[0], &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(view.state == ZR_HOT_PATCH_VERSION_RETIRED &&
                  view.contentHash == 101u && view.leaseCount == 1u);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_AcquireActive(
            &f.manager, &f.leases[1], &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Resolve(
            &f.manager, &f.leases[1], &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(view.state == ZR_HOT_PATCH_VERSION_ACTIVE && view.contentHash == 202u);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_CollectRetired(
            &f.manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(collected == 0u);
cleanup:
    failures += discard_fixture_cleanup(&other);
    failures += discard_fixture_cleanup(&f);
    return failures;
}

static int test_discard_rollback_and_generation_exhaustion(void) {
    ZrDiscardFixture f;
    SZrHotPatchGenerationHandle active, candidate, again;
    SZrHotPatchGenerationDiagnostic d;
    ZrDiscardSnapshot before;
    TZrUInt64 discardedGeneration;
    int failures = 0;
    if (discard_fixture_init(&f, 2u) != 0) return 1;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &active, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Publish(
            &f.manager, &active, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Rollback(
            &f.manager, active.generation, &candidate, &d) == ZR_HOT_PATCH_GENERATION_OK);
    discardedGeneration = candidate.generation;
    discard_snapshot(&f, &before);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &candidate, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(f.records[0].generation == active.generation &&
                  f.records[0].contentHash == 101u &&
                  f.records[0].state == ZR_HOT_PATCH_VERSION_ACTIVE &&
                  atomic_load(&f.manager.active) == active.record &&
                  f.manager.count == before.count &&
                  atomic_load(&f.manager.nextGeneration) == before.next);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Rollback(
            &f.manager, active.generation, &again, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(again.generation > discardedGeneration);
    /* Exclusive fixture seeding does not reset the production epoch allocator. */
    atomic_store_explicit(&f.manager.nextGeneration, UINT64_MAX, memory_order_relaxed);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &again, &d) == ZR_HOT_PATCH_GENERATION_OK);
    DISCARD_CHECK(atomic_load(&f.manager.nextGeneration) == UINT64_MAX &&
                  discard_record_is_clear(&f.records[1]));
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &candidate, &d) ==
            ZR_HOT_PATCH_GENERATION_OVERFLOW);
    DISCARD_CHECK(discard_record_is_clear(&f.records[1]) &&
                  atomic_load(&f.manager.active) == active.record);
cleanup:
    failures += discard_fixture_cleanup(&f);
    return failures;
}

static int test_discard_full_width_busy_and_active_guard(void) {
    ZrDiscardFixture f;
    SZrHotPatchGenerationHandle candidate;
    SZrHotPatchGenerationDiagnostic d;
    int failures = 0;
    if (discard_fixture_init(&f, 1u) != 0) return 1;
    DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
            &f.manager, &f.validated, 7u, &candidate, &d) == ZR_HOT_PATCH_GENERATION_OK);
    atomic_store_explicit(&f.records[0].leaseCount, UINT32_MAX, memory_order_relaxed);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &candidate,
            ZR_HOT_PATCH_GENERATION_INVALID_STATE,
            candidate.generation, candidate.generation, UINT32_MAX) == 0);
    if (UINT_FAST32_MAX > UINT32_MAX) {
        /* A wide atomic must not become a false zero after UInt32 truncation. */
        uint_fast32_t wideBusy = UINT32_MAX;
        ++wideBusy;
        atomic_store_explicit(&f.records[0].leaseCount,
                wideBusy, memory_order_relaxed);
        DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &candidate,
                ZR_HOT_PATCH_GENERATION_INVALID_STATE,
                candidate.generation, candidate.generation, UINT32_MAX) == 0);
    }
    atomic_store_explicit(&f.records[0].leaseCount, 0u, memory_order_relaxed);
    atomic_store_explicit(&f.manager.active, candidate.record, memory_order_release);
    DISCARD_CHECK(discard_expect_unchanged(&f, &f.manager, &candidate,
            ZR_HOT_PATCH_GENERATION_INVALID_STATE,
            candidate.generation, candidate.generation, 0u) == 0);
    atomic_store_explicit(&f.manager.active, ZR_NULL, memory_order_release);
    DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
            &f.manager, &candidate, &d) == ZR_HOT_PATCH_GENERATION_OK);
cleanup:
    /* Restore all exclusively seeded fictional state even after a failed check. */
    atomic_store_explicit(&f.records[0].leaseCount, 0u, memory_order_relaxed);
    atomic_store_explicit(&f.manager.active, ZR_NULL, memory_order_release);
    failures += discard_fixture_cleanup(&f);
    return failures;
}

typedef struct ZrDiscardWorker {
    SZrHotPatchGenerationManager *manager;
    SZrHotPatchGenerationHandle handle;
    TZrUInt64 generation;
    TZrBool acquire;
    EZrHotPatchGenerationStatus status;
} ZrDiscardWorker;

static void discard_worker_run(ZrDiscardWorker *worker) {
    if (worker->acquire) {
        worker->status = ZrCore_HotPatch_Generation_Acquire(
                worker->manager, worker->generation, &worker->handle, ZR_NULL);
    } else {
        worker->status = ZrCore_HotPatch_Generation_DiscardPrepared(
                worker->manager, &worker->handle, ZR_NULL);
    }
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI discard_worker_entry(LPVOID argument) {
    discard_worker_run((ZrDiscardWorker *)argument);
    return 0;
}
static TZrBool discard_worker_start(ZrGenerationTestThread *thread,
                                    ZrDiscardWorker *worker) {
    *thread = CreateThread(ZR_NULL, 0u, discard_worker_entry, worker, 0u, ZR_NULL);
    return (TZrBool)(*thread != ZR_NULL);
}
static TZrBool discard_worker_join(ZrGenerationTestThread thread) {
    DWORD status = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (TZrBool)(status == WAIT_OBJECT_0);
}
#else
static void *discard_worker_entry(void *argument) {
    discard_worker_run((ZrDiscardWorker *)argument);
    return ZR_NULL;
}
static TZrBool discard_worker_start(ZrGenerationTestThread *thread,
                                    ZrDiscardWorker *worker) {
    return (TZrBool)(pthread_create(thread, ZR_NULL, discard_worker_entry, worker) == 0);
}
static TZrBool discard_worker_join(ZrGenerationTestThread thread) {
    return (TZrBool)(pthread_join(thread, ZR_NULL) == 0);
}
#endif

static int test_discard_competing_operations(TZrBool raceAcquire) {
    for (TZrUInt32 i = 0u; i < ZR_DISCARD_TEST_REPETITIONS; ++i) {
        ZrDiscardFixture f;
        ZrDiscardWorker worker;
        ZrGenerationTestThread thread;
        SZrHotPatchGenerationHandle candidate, mainCopy;
        SZrHotPatchGenerationDiagnostic d;
        EZrHotPatchGenerationStatus mainStatus;
        TZrUInt64 generation;
        int failures = 0;
        if (discard_fixture_init(&f, 1u) != 0) return 1;
        DISCARD_CHECK(ZrCore_HotPatch_Generation_Prepare(
                &f.manager, &f.validated, 7u, &candidate, &d) ==
                ZR_HOT_PATCH_GENERATION_OK);
        generation = candidate.generation;
        mainCopy = candidate;
        memset(&worker, 0, sizeof(worker));
        worker.manager = &f.manager;
        worker.generation = generation;
        worker.acquire = raceAcquire;
        worker.handle = candidate;
        /* No manual-held lock or timing assertion: either serialized order is valid.
         * Driver bounds the entire process; every successfully started worker is
         * joined before any assertion, lease cleanup or fixture destruction. */
        if (!discard_worker_start(&thread, &worker)) {
            fputs("discard worker creation failed\n", stderr);
            failures = 1;
            goto cleanup;
        }
        if (raceAcquire) {
            mainStatus = ZrCore_HotPatch_Generation_DiscardPrepared(
                    &f.manager, &mainCopy, &d);
        } else {
            mainStatus = ZrCore_HotPatch_Generation_Publish(
                    &f.manager, &mainCopy, &d);
        }
        if (!discard_worker_join(thread)) {
            /* A join error cannot establish that stack-backed worker data is safe
             * to reclaim. End the process without reclaiming that shared fixture. */
            fputs("discard worker join failed\n", stderr);
            exit(1);
        }
        if (raceAcquire) {
            f.leases[0] = worker.handle;
            if (worker.status == ZR_HOT_PATCH_GENERATION_OK) {
                DISCARD_CHECK(mainStatus == ZR_HOT_PATCH_GENERATION_INVALID_STATE &&
                              mainCopy.record == candidate.record &&
                              mainCopy.generation == generation && !mainCopy.leased);
                DISCARD_CHECK(ZrCore_HotPatch_Generation_Release(
                        &f.manager, &f.leases[0], &d) == ZR_HOT_PATCH_GENERATION_OK);
                DISCARD_CHECK(ZrCore_HotPatch_Generation_DiscardPrepared(
                        &f.manager, &mainCopy, &d) == ZR_HOT_PATCH_GENERATION_OK);
            } else {
                DISCARD_CHECK(worker.status == ZR_HOT_PATCH_GENERATION_STALE_LINK &&
                              mainStatus == ZR_HOT_PATCH_GENERATION_OK &&
                              !worker.handle.leased && worker.handle.record == ZR_NULL);
            }
            DISCARD_CHECK(discard_record_is_clear(&f.records[0]) &&
                          atomic_load(&f.manager.active) == ZR_NULL);
        } else if (mainStatus == ZR_HOT_PATCH_GENERATION_OK) {
            DISCARD_CHECK(worker.status == ZR_HOT_PATCH_GENERATION_NOT_PREPARED &&
                          atomic_load(&f.manager.active) == candidate.record &&
                          f.records[0].state == ZR_HOT_PATCH_VERSION_ACTIVE &&
                          f.records[0].generation == generation);
        } else {
            DISCARD_CHECK(mainStatus == ZR_HOT_PATCH_GENERATION_NOT_PREPARED &&
                          worker.status == ZR_HOT_PATCH_GENERATION_OK &&
                          discard_record_is_clear(&f.records[0]) &&
                          atomic_load(&f.manager.active) == ZR_NULL);
        }
        DISCARD_CHECK(f.manager.count == 1u &&
                      atomic_load(&f.manager.nextGeneration) == generation);
cleanup:
        failures += discard_fixture_cleanup(&f);
        if (failures != 0) return failures;
    }
    return 0;
}

#undef DISCARD_CHECK
