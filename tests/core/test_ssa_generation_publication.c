#include "zr_vm_core/hotpatch_generation.h"

#include <assert.h>
#include <string.h>

static void make_validated(SZrValidatedHotPatch *v, SZrArtifactExecIrView *a,
                           SZrHotPatchCapabilityManifest *m,
                           TZrUInt64 content, TZrUInt64 module) {
    memset(v, 0, sizeof(*v)); memset(a, 0, sizeof(*a)); memset(m, 0, sizeof(*m));
    a->moduleHash = module; a->buffer = (const TZrByte *)"x"; a->bufferLength = 1u;
    m->publicContractHash = 55u;
    v->artifact = a; v->manifest = m; v->contentHash = content;
    v->targetProfile = 2u; v->signatureVerified = ZR_TRUE;
    v->immutableContent = ZR_TRUE;
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

    assert(ZrCore_HotPatch_GenerationManager_Init(&manager, records, 3u, &d) == ZR_HOT_PATCH_GENERATION_OK);
    make_validated(&v1, &a1, &m1, 101u, 7u);
    make_validated(&v2, &a2, &m2, 202u, 7u);
    assert(ZrCore_HotPatch_Generation_Prepare(&manager, &v1, 7u, &p1, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_Publish(&manager, &p1, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_AcquireActive(&manager, &oldFrame, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_Prepare(&manager, &v2, 7u, &p2, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_Publish(&manager, &p2, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_AcquireActive(&manager, &newCall, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(oldFrame.generation != newCall.generation);
    assert(ZrCore_HotPatch_Generation_Resolve(&manager, &oldFrame, &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(view.contentHash == 101u && view.state == ZR_HOT_PATCH_VERSION_RETIRED);
    assert(ZrCore_HotPatch_Generation_Resolve(&manager, &newCall, &view, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(view.contentHash == 202u && view.state == ZR_HOT_PATCH_VERSION_ACTIVE);
    assert(ZrCore_HotPatch_Generation_Release(&manager, &newCall, &d) == ZR_HOT_PATCH_GENERATION_OK);
    collected = 0u;
    assert(ZrCore_HotPatch_Generation_CollectRetired(&manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(collected == 0u);
    assert(ZrCore_HotPatch_Generation_Release(&manager, &oldFrame, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_Generation_CollectRetired(&manager, &collected, &d) == ZR_HOT_PATCH_GENERATION_OK);
    assert(collected == 1u);
    assert(ZrCore_HotPatch_Generation_Acquire(&manager, 1u, &newCall, &d) == ZR_HOT_PATCH_GENERATION_STALE_LINK);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return 0;
}
