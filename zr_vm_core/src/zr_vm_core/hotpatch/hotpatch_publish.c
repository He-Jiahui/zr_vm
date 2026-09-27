#include "zr_vm_core/hotpatch_publish.h"

/* 兼容分阶段发布调用方；真正的代际切换由 manager 锁内完成。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_PublishPrepared(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_Generation_Publish(manager, prepared, diagnostic);
}

/* 同一发布协议的简名入口，准备句柄在成功后指向新 active 记录。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_PublishPrepared(manager, prepared, diagnostic);
}
