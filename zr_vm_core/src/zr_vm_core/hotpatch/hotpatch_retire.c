#include "zr_vm_core/hotpatch_retire.h"

/* host 可在安全点主动回收退役代际；manager 仍负责 lease 为零的判定。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_RetireCollect(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_Generation_CollectRetired(manager, outCollected,
                                                     diagnostic);
}

/* 与 RetireCollect 共享回收协议的兼容名称。 */
EZrHotPatchGenerationStatus ZrCore_HotPatch_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_RetireCollect(manager, outCollected, diagnostic);
}
