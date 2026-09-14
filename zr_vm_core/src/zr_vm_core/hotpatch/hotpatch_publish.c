#include "zr_vm_core/hotpatch_publish.h"

EZrHotPatchGenerationStatus ZrCore_HotPatch_PublishPrepared(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_Generation_Publish(manager, prepared, diagnostic);
}

EZrHotPatchGenerationStatus ZrCore_HotPatch_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic) {
    return ZrCore_HotPatch_PublishPrepared(manager, prepared, diagnostic);
}
