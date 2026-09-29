#include "zr_vm_core/hotpatch_generation.h"
#include "zr_vm_core/hotpatch_publish.h"
#include "zr_vm_core/hotpatch_retire.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

    if (test_cross_manager_handles_are_rejected() != 0) return 1;

    assert(ZrCore_HotPatch_GenerationManager_Init(&manager, records, 3u, &d) == ZR_HOT_PATCH_GENERATION_OK);
    make_validated(&v1, &a1, &m1, 101u, 7u);
    make_validated(&v2, &a2, &m2, 202u, 7u);
    assert(ZrCore_HotPatch_Generation_Prepare(&manager, &v1, 7u, &p1, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_PublishPrepared(&manager, &p1, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_AcquireActive(&manager, &oldFrame, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_Prepare(&manager, &v2, 7u, &p2, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Publish(&manager, &p2, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_AcquireActive(&manager, &newCall, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(oldFrame.generation != newCall.generation);
    assert(ZrCore_HotPatch_Generation_Resolve(&manager, &oldFrame, &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(view.contentHash == 101u && view.state == ZR_HOT_PATCH_VERSION_RETIRED);
    assert(ZrCore_HotPatch_Generation_Resolve(&manager, &newCall, &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(view.contentHash == 202u && view.state == ZR_HOT_PATCH_VERSION_ACTIVE);
    assert(ZrCore_HotPatch_Generation_Release(&manager, &newCall, &d) == ZR_HOT_PATCH_GENERATION_OK);
    collected = 0u;
    assert(ZrCore_HotPatch_RetireCollect(&manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(collected == 0u);
    assert(ZrCore_HotPatch_Generation_Release(&manager, &oldFrame, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_CollectRetired(&manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(collected == 1u);
    assert(ZrCore_HotPatch_Generation_Acquire(&manager, 1u, &newCall, &d) == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return 0;
}
