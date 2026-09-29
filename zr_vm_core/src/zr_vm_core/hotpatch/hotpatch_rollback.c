#include "zr_vm_core/hotpatch_rollback.h"

#include <string.h>

/* registry/manager 协调层用同一诊断形状报告 patch ID、内容哈希和代际。 */
static EZrHotPatchApplyStatus apply_fail(SZrHotPatchApplyDiagnostic *d, EZrHotPatchApplyStatus s,
                                          TZrUInt64 id, TZrUInt64 expected, TZrUInt64 actual, TZrUInt64 generation) {
    if (d) { d->status = s; d->patchId = id; d->expectedHash = expected; d->actualHash = actual; d->generation = generation; }
    return s;
}

/* 已验证候选先按 ID/内容做幂等检查，再 Prepare + Publish；
 * TODO: registry 判重/登记无锁；核查 host 是否串行调用，否则并发同 ID 可重复发布。 */
EZrHotPatchApplyStatus ZrCore_HotPatch_ApplyValidated(
        SZrHotPatchGenerationManager *manager, SZrHotPatchRegistry *registry,
        const SZrValidatedHotPatch *validated, TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle, SZrHotPatchApplyDiagnostic *diagnostic) {
    if (outHandle) memset(outHandle, 0, sizeof(*outHandle));
    if (!manager || !registry || !validated || !validated->manifest || !validated->artifact ||
        !validated->patchId || !validated->publicContractHash ||
        !validated->signatureVerified || !validated->immutableContent || !registry->entries || !registry->capacity || !outHandle)
        return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT, 0u, 0u, 0u, 0u);
    TZrUInt64 id = validated->patchId, hash = validated->contentHash;
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
    /* BUG: moduleHash=0 使 Prepare 返回 INVALID_ARGUMENT，代际溢出返回
     * OVERFLOW；此处都报 CAPACITY，host 无法按实际原因处理失败。 */
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_CAPACITY, id, hash, hash, 0u);
    gs = ZrCore_HotPatch_Generation_Publish(manager, &prepared, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_PUBLISH_FAILED, id, hash, hash, prepared.generation);
    freeEntry->patchId = id; freeEntry->contentHash = hash; freeEntry->generation = prepared.generation; freeEntry->state = ZR_HOT_PATCH_VERSION_ACTIVE;
    if (registry->count < registry->capacity) ++registry->count;
    *outHandle = prepared;
    return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_OK, id, hash, hash, prepared.generation);
}

/* 回滚通过新代际重新发布旧身份，避免改变持有旧编号的 frame 视图。 */
EZrHotPatchApplyStatus ZrCore_HotPatch_Rollback(
        SZrHotPatchGenerationManager *manager, TZrUInt64 targetGeneration,
        SZrHotPatchGenerationHandle *outHandle, SZrHotPatchApplyDiagnostic *diagnostic) {
    if (outHandle) memset(outHandle, 0, sizeof(*outHandle));
    if (!manager || !outHandle || !targetGeneration) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT, 0u, 0u, 0u, 0u);
    SZrHotPatchGenerationDiagnostic gd;
    EZrHotPatchGenerationStatus gs = ZrCore_HotPatch_Generation_Rollback(manager, targetGeneration, outHandle, &gd);
    /* BUG: 目标存在但无空槽时 Generation_Rollback 返回 CAPACITY；这里
     * 统一报 NOT_FOUND，调用方会误判可回收后重试的情况。 */
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND, 0u, targetGeneration, gd.actualGeneration, 0u);
    gs = ZrCore_HotPatch_Generation_Publish(manager, outHandle, &gd);
    if (gs != ZR_HOT_PATCH_GENERATION_OK) return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_ROLLBACK_FAILED, 0u, targetGeneration, gd.actualGeneration, outHandle->generation);
    return apply_fail(diagnostic, ZR_HOT_PATCH_APPLY_OK, 0u, targetGeneration, targetGeneration, outHandle->generation);
}

/* host 日志的文字映射，不用字符串反推恢复策略。 */
const TZrChar *ZrCore_HotPatch_ApplyStatusName(EZrHotPatchApplyStatus s) {
    switch (s) { case ZR_HOT_PATCH_APPLY_OK: return "ok"; case ZR_HOT_PATCH_APPLY_ALREADY_APPLIED: return "already-applied"; case ZR_HOT_PATCH_APPLY_ID_COLLISION: return "id-collision"; case ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND: return "rollback-not-found"; default: return "apply-failed"; }
}
