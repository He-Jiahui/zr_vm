#include "zr_vm_core/hotpatch_capability.h"

#include <string.h>

static EZrHotPatchCapabilityStatus capability_fail(
        SZrHotPatchDiagnostic *diagnostic,
        EZrHotPatchCapabilityStatus status,
        TZrUInt32 token,
        TZrUInt32 sourceOffset,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->token = token;
        diagnostic->sourceOffset = sourceOffset;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

EZrHotPatchCapabilityStatus ZrCore_HotPatch_ComputeCapabilityClosure(
        const SZrHotPatchCapabilityRequirement *requirements,
        TZrUInt32 requirementCount,
        TZrUInt64 manifestRequiredCapabilities,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic) {
    TZrUInt64 required = manifestRequiredCapabilities;
    TZrUInt32 index;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (outRequiredCapabilities != ZR_NULL) {
        *outRequiredCapabilities = 0u;
    }
    if (requirementCount > ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS) {
        return capability_fail(diagnostic, ZR_HOT_PATCH_LIMIT, 0u,
                               0u, ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS,
                               requirementCount);
    }
    if (requirementCount != 0u && requirements == ZR_NULL) {
        return capability_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT, 0u,
                               0u, hostAllowedCapabilities,
                               manifestRequiredCapabilities);
    }
    if ((manifestRequiredCapabilities & ~hostAllowedCapabilities) != 0u) {
        return capability_fail(diagnostic,
                               ZR_HOT_PATCH_CAPABILITY_ESCALATION,
                               0u, 0u, hostAllowedCapabilities,
                               manifestRequiredCapabilities);
    }
    for (index = 0u; index < requirementCount; ++index) {
        const SZrHotPatchCapabilityRequirement *requirement =
                &requirements[index];

        if (requirement->token == 0u || requirement->requiredBits == 0u ||
            requirement->reserved != 0u) {
            return capability_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT,
                                   requirement->token,
                                   requirement->sourceOffset, 0u,
                                   requirement->requiredBits);
        }
        if ((requirement->requiredBits & ~hostAllowedCapabilities) != 0u) {
            return capability_fail(diagnostic,
                                   ZR_HOT_PATCH_CAPABILITY_ESCALATION,
                                   requirement->token,
                                   requirement->sourceOffset,
                                   hostAllowedCapabilities,
                                   requirement->requiredBits);
        }
        required |= requirement->requiredBits;
    }
    if ((required & ~hostAllowedCapabilities) != 0u) {
        return capability_fail(diagnostic,
                               ZR_HOT_PATCH_CAPABILITY_ESCALATION,
                               0u, 0u, hostAllowedCapabilities, required);
    }
    if (outRequiredCapabilities != ZR_NULL) {
        *outRequiredCapabilities = required;
    }
    return capability_fail(diagnostic, ZR_HOT_PATCH_OK, 0u, 0u, 0u, 0u);
}

EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateCapabilityClosure(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic) {
    if (manifest == ZR_NULL ||
        (manifest->flags & ~ZR_HOT_PATCH_FLAG_KNOWN_MASK) != 0u) {
        if (outRequiredCapabilities != ZR_NULL) {
            *outRequiredCapabilities = 0u;
        }
        return capability_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT, 0u,
                               0u, ZR_HOT_PATCH_FLAG_KNOWN_MASK,
                               manifest != ZR_NULL ? manifest->flags : 0u);
    }
    return ZrCore_HotPatch_ComputeCapabilityClosure(
            manifest->requirements, manifest->requirementCount,
            manifest->requiredCapabilities, hostAllowedCapabilities,
            outRequiredCapabilities, diagnostic);
}

EZrHotPatchCapabilityStatus ZrCore_HotPatch_ComputeRequiredCapabilities(
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 hostAllowedCapabilities,
        TZrUInt64 *outRequiredCapabilities,
        SZrHotPatchDiagnostic *diagnostic) {
    return ZrCore_HotPatch_ValidateCapabilityClosure(
            manifest, hostAllowedCapabilities, outRequiredCapabilities,
            diagnostic);
}
