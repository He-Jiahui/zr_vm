#ifndef ZR_VM_CORE_HOTPATCH_CAPABILITY_H
#define ZR_VM_CORE_HOTPATCH_CAPABILITY_H

#include "zr_vm_core/capability_manifest.h"

/* The manifest validator owns identity/signature checks.  This seam isolates
 * the transitive capability closure so producers cannot accidentally replace
 * it with a simple aggregate OR at a call site. */
#define ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS ((TZrUInt32)4096u)

ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateCapabilityClosure(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ComputeCapabilityClosure(
        const SZrHotPatchCapabilityRequirement *requirements,
        TZrUInt32 requirementCount,
        TZrUInt64 manifestRequiredCapabilities,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

/* Manifest-shaped spelling for hosts that do not need the lower-level row
 * traversal entry point. */
ZR_CORE_API EZrHotPatchCapabilityStatus
ZrCore_HotPatch_ComputeRequiredCapabilities(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic);

#define ZrCore_HotPatch_ValidateCapabilities \
    ZrCore_HotPatch_ValidateCapabilityClosure

#endif
