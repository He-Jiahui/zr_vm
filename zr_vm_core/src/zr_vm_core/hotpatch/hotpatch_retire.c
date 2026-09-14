#include "zr_vm_core/hotpatch_retire.h"

EZrHotPatchGenerationStatus ZrCore_HotPatch_RetireCollect(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_Generation_CollectRetired(manager, outCollected,
                                                     diagnostic);
}

EZrHotPatchGenerationStatus ZrCore_HotPatch_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_RetireCollect(manager, outCollected, diagnostic);
}
