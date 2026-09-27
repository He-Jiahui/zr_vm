#include "zr_vm_core/hotpatch_profile.h"

/* 把首个不兼容 section 位置带回受限 host，避免只返回笼统失败。 */
static EZrHotPatchRestrictedStatus restricted_fail(SZrHotPatchRestrictedDiagnostic *d,
                                                    EZrHotPatchRestrictedStatus s, TZrUInt32 kind, TZrUInt32 index) {
    if (d) { d->status = s; d->sectionKind = kind; d->sectionIndex = index; }
    return s;
}

/* iOS/WASM 解释器入口只接纳可解释执行的 section；调用方应先获得
 * 已解析且在调用期间稳定的 artifact view。 */
EZrHotPatchRestrictedStatus ZrCore_HotPatch_ValidateRestrictedProfile(
        const SZrArtifactExecIrView *artifact, TZrUInt32 profile,
        SZrHotPatchRestrictedDiagnostic *diagnostic) {
    if (!artifact || !artifact->buffer || !artifact->bufferLength ||
        artifact->sectionCount > ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS)
        return restricted_fail(diagnostic, ZR_HOT_PATCH_RESTRICTED_INVALID_ARGUMENT, 0u, 0u);
    if (profile != ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER && profile != ZR_HOT_PATCH_PROFILE_WASM_INTERPRETER)
        return restricted_fail(diagnostic, ZR_HOT_PATCH_RESTRICTED_PROFILE_MISMATCH, 0u, 0u);
    for (TZrUInt32 i = 0u; i < artifact->sectionCount; ++i) {
        EZrArtifactExecIrSectionKind kind = artifact->sections[i].kind;
        if (kind != ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR && kind != ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC &&
            kind != ZR_ARTIFACT_EXEC_IR_SECTION_STATE_MAPS && kind != ZR_ARTIFACT_EXEC_IR_SECTION_BINDINGS)
            return restricted_fail(diagnostic, ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN, (TZrUInt32)kind, i);
    }
    return restricted_fail(diagnostic, ZR_HOT_PATCH_RESTRICTED_OK, 0u, 0u);
}

/* 日志与测试诊断使用稳定状态名，不参与准入判断。 */
const TZrChar *ZrCore_HotPatch_RestrictedStatusName(EZrHotPatchRestrictedStatus s) {
    switch (s) { case ZR_HOT_PATCH_RESTRICTED_OK: return "ok"; case ZR_HOT_PATCH_RESTRICTED_PROFILE_MISMATCH: return "profile-mismatch"; case ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN: return "section-forbidden"; default: return "invalid-restricted-profile"; }
}
