#include "zr_vm_core/hotpatch_rollback.h"

#include <string.h>

static EZrHotPatchApplyStatus apply_fail(SZrHotPatchApplyDiagnostic *d, EZrHotPatchApplyStatus s,
                                          TZrUInt64 id, TZrUInt64 expected, TZrUInt64 actual, TZrUInt64 generation) {
    if (d) { d->status = s; d->patchId = id; d->expectedHash = expected; d->actualHash = actual; d->generation = generation; }
    return s;
}

EZrHotPatchApplyStatus ZrCore_HotPatch_ApplyValidated(
        SZrHotPatchGenerationManager *manager, SZrHotPatchRegistry *registry,
        const SZrValidatedHotPatch *validated, TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle, SZrHotPatchApplyDiagnostic *diagnostic) {
    if (outHandle) memset(outHandle, 0, sizeof(*outHandle));
    if (!manager || !registry || !validated || !validated->manifest || !validated->artifact ||
        !validated->signatureVerified || !validated->immutableContent || !registry->entries || !registry->capacity || !outHandle)
        return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT, 0u, 0u, 0u, 0u);
    TZrUInt64 id = validated->manifest->patchId, hash = validated->contentHash;
    SZrHotPatchRegistryEntry *freeEntry = ZR_NULL;
    for (TZrUInt32 i = 0u; i < registry->capacity; ++i) {
        SZrHotPatchRegistryEntry *e = &registry->entries[i];
        if (!e->patchId && !freeEntry) freeEntry = e;
        if (!e->patchId || e->patchId != id) continue;
        if (e->contentHash != hash) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ID_COLLISION, id, e->contentHash, hash, e->generation);
        if (e->state == ZR_HOT_PATCH_VERSION_ACTIVE || e->state == ZR_HOT_PATCH_VERSION_RETIRED)
            return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ALREADY_APPLIED, id, hash, hash, e->generation);
    }
    if (!freeEntry) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_CAPACITY, id, hash, hash, 0u);
    SZrHotPatchGenerationHandle prepared;
    SZrHotPatchGenerationDiagnostic gd;
    EZrHotPatchGenerationStatus gs = ZrCore_HotPatch_Generation_Prepare(manager, validated, moduleHash, &prepared, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_CAPACITY, id, hash, hash, 0u);
    gs = ZrCore_HotPatch_Generation_Publish(manager, &prepared, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_PUBLISH_FAILED, id, hash, hash, prepared.generation);
    freeEntry->patchId = id; freeEntry->contentHash = hash; freeEntry->generation = prepared.generation; freeEntry->state = ZR_HOT_PATCH_VERSION_ACTIVE;
    if (registry->count < registry->capacity) ++registry->count;
    *outHandle = prepared;
    return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_OK, id, hash, hash, prepared.generation);
}

EZrHotPatchApplyStatus ZrCore_HotPatch_Rollback(
        SZrHotPatchGenerationManager *manager, TZrUInt64 targetGeneration,
        SZrHotPatchGenerationHandle *outHandle, SZrHotPatchApplyDiagnostic *diagnostic) {
    if (outHandle) memset(outHandle, 0, sizeof(*outHandle));
    if (!manager || !outHandle || !targetGeneration) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT, 0u, 0u, 0u, 0u);
    SZrHotPatchGenerationDiagnostic gd;
    EZrHotPatchGenerationStatus gs = ZrCore_HotPatch_Generation_Rollback(manager, targetGeneration, outHandle, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND, 0u, targetGeneration, gd.actualGeneration, 0u);
    gs = ZrCore_HotPatch_Generation_Publish(manager, outHandle, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ROLLBACK_FAILED, 0u, targetGeneration, gd.actualGeneration, outHandle->generation);
    return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_OK, 0u, targetGeneration, targetGeneration, outHandle->generation);
}

const TZrChar *ZrCore_HotPatch_ApplyStatusName(EZrHotPatchApplyStatus s) {
    switch (s) { case ZR_HOT_PATCH_APPLY_OK: return "ok"; case ZR_HOT_PATCH_APPLY_ALREADY_APPLIED: return "already-applied"; case ZR_HOT_PATCH_APPLY_ID_COLLISION: return "id-collision"; case ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND: return "rollback-not-found"; default: return "apply-failed"; }
}
