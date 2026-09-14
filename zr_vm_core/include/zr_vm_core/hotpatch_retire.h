#ifndef ZR_VM_CORE_HOTPATCH_RETIRE_H
#define ZR_VM_CORE_HOTPATCH_RETIRE_H

#include "zr_vm_core/hotpatch_generation.h"

/* Reclaim is intentionally a separate seam: a host can schedule it at a
 * safe point while the generation manager still enforces lease==0. */
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_RetireCollect(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
#define ZrCore_HotPatch_Retire ZrCore_HotPatch_RetireCollect

#endif
