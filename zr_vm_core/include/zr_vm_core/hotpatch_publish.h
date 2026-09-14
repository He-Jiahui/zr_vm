#ifndef ZR_VM_CORE_HOTPATCH_PUBLISH_H
#define ZR_VM_CORE_HOTPATCH_PUBLISH_H

#include "zr_vm_core/hotpatch_generation.h"

/* Compatibility facade for callers that keep publication separate from
 * version-record construction.  The generation manager remains the sole
 * owner of the lock and release-store protocol. */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_PublishPrepared(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);

#endif
