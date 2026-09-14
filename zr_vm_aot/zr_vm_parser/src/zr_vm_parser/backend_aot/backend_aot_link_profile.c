#include "backend_aot_link_profile.h"

#include <string.h>

static void backend_aot_link_profile_clear_diagnostic(
        SZrAotReleasePolicyDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool backend_aot_link_profile_bool_valid(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool backend_aot_link_profile_kind_valid(
        EZrAotReleaseProfileKind kind) {
    return (TZrBool)(kind >= ZR_AOT_RELEASE_PROFILE_DEV &&
                     kind <= ZR_AOT_RELEASE_PROFILE_RELEASE_PGO);
}

static EZrBackendAotLinkProfileStatus backend_aot_link_profile_status_for_code(
        EZrAotReleasePolicyDiagnosticCode code) {
    switch (code) {
        case ZR_AOT_RELEASE_POLICY_PROFILE_MISMATCH:
            return ZR_BACKEND_AOT_LINK_PROFILE_MISMATCH;
        case ZR_AOT_RELEASE_POLICY_LTO_UNSUPPORTED:
        case ZR_AOT_RELEASE_POLICY_PGO_UNSUPPORTED:
            return ZR_BACKEND_AOT_LINK_PROFILE_UNSUPPORTED;
        case ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT:
        case ZR_AOT_RELEASE_POLICY_ROOT_OUT_OF_RANGE:
        default:
            return ZR_BACKEND_AOT_LINK_PROFILE_INVALID_ARGUMENT;
    }
}

static void backend_aot_link_profile_copy_effective(
        const SZrAotReleaseEffectiveProfile *effective,
        SZrBackendAotLinkProfileResult *result) {
    result->effectiveKind = effective->kind;
    result->ltoEnabled = effective->ltoEnabled;
    result->thinLtoEnabled = effective->thinLtoEnabled;
    result->pgoEnabled = effective->pgoEnabled;
    result->fallbackToDev = ZR_FALSE;
}

EZrBackendAotLinkProfileStatus backend_aot_link_profile_resolve(
        const SZrAotReleaseProfile *profile,
        const SZrAotReleaseToolchain *toolchain,
        TZrBool allowFallback,
        SZrBackendAotLinkProfileResult *outResult,
        SZrAotReleasePolicyDiagnostic *diagnostic) {
    SZrAotReleaseEffectiveProfile effective;
    SZrAotReleasePolicyDiagnostic localDiagnostic;
    SZrAotReleasePolicyDiagnostic *policyDiagnostic;
    EZrAotReleasePolicyDiagnosticCode policyCode;
    EZrBackendAotLinkProfileStatus status;

    backend_aot_link_profile_clear_diagnostic(diagnostic);
    if (outResult != ZR_NULL) {
        (void)memset(outResult, 0, sizeof(*outResult));
    }
    (void)memset(&localDiagnostic, 0, sizeof(localDiagnostic));
    policyDiagnostic = diagnostic != ZR_NULL ? diagnostic : &localDiagnostic;
    if (profile == ZR_NULL || toolchain == ZR_NULL || outResult == ZR_NULL ||
        !backend_aot_link_profile_bool_valid(allowFallback) ||
        !backend_aot_link_profile_kind_valid(profile->kind) ||
        !backend_aot_link_profile_bool_valid(toolchain->supportsLto) ||
        !backend_aot_link_profile_bool_valid(toolchain->supportsThinLto) ||
        !backend_aot_link_profile_bool_valid(toolchain->supportsPgo)) {
        policyDiagnostic->code = ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT;
        return ZR_BACKEND_AOT_LINK_PROFILE_INVALID_ARGUMENT;
    }
    (void)memset(&effective, 0, sizeof(effective));
    if (ZrParser_AotReleasePolicy_ValidateProfile(profile, toolchain,
                                                   &effective,
                                                   policyDiagnostic)) {
        backend_aot_link_profile_copy_effective(&effective, outResult);
        return ZR_BACKEND_AOT_LINK_PROFILE_OK;
    }

    policyCode = policyDiagnostic->code;
    status = backend_aot_link_profile_status_for_code(policyCode);
    if (allowFallback &&
        (status == ZR_BACKEND_AOT_LINK_PROFILE_MISMATCH ||
         status == ZR_BACKEND_AOT_LINK_PROFILE_UNSUPPORTED)) {
        outResult->effectiveKind = ZR_AOT_RELEASE_PROFILE_DEV;
        outResult->ltoEnabled = ZR_FALSE;
        outResult->thinLtoEnabled = ZR_FALSE;
        outResult->pgoEnabled = ZR_FALSE;
        outResult->fallbackToDev = ZR_TRUE;
        return ZR_BACKEND_AOT_LINK_PROFILE_FALLBACK;
    }
    return status;
}

TZrBool backend_aot_link_profile_mark_roots(
        const SZrAotReleaseRoot *roots,
        TZrUInt32 rootCount,
        TZrUInt32 functionCount,
        TZrBool *retained,
        SZrAotReleasePolicyDiagnostic *diagnostic) {
    backend_aot_link_profile_clear_diagnostic(diagnostic);
    /* Keep failure atomic from the caller's perspective.  The shared policy
     * also clears this range, but only after its own argument checks; clear
     * first so an invalid root kind cannot leave stale reachability bits. */
    if (retained != ZR_NULL && functionCount != 0u) {
        (void)memset(retained, 0, (TZrSize)functionCount * sizeof(*retained));
    }
    for (TZrUInt32 index = 0u; index < rootCount; ++index) {
        if (roots == ZR_NULL ||
            roots[index].kind < ZR_AOT_RELEASE_ROOT_ENTRY ||
            roots[index].kind > ZR_AOT_RELEASE_ROOT_PATCH_FUTURE_USE) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT;
                diagnostic->index = index;
            }
            return ZR_FALSE;
        }
    }
    return ZrParser_AotReleasePolicy_MarkRoots(roots, rootCount,
                                                functionCount, retained,
                                                diagnostic);
}
