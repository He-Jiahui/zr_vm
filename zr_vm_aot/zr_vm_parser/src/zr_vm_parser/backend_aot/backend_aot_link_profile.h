#ifndef ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H
#define ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H

#include "zr_vm_parser/aot_generic_policy.h"

typedef enum EZrBackendAotLinkProfileStatus {
    ZR_BACKEND_AOT_LINK_PROFILE_OK = 0,
    ZR_BACKEND_AOT_LINK_PROFILE_INVALID_ARGUMENT,
    ZR_BACKEND_AOT_LINK_PROFILE_MISMATCH,
    ZR_BACKEND_AOT_LINK_PROFILE_UNSUPPORTED,
    ZR_BACKEND_AOT_LINK_PROFILE_FALLBACK
} EZrBackendAotLinkProfileStatus;

typedef struct SZrBackendAotLinkProfileResult {
    EZrAotReleaseProfileKind effectiveKind;
    TZrBool ltoEnabled;
    TZrBool thinLtoEnabled;
    TZrBool pgoEnabled;
    TZrBool fallbackToDev;
} SZrBackendAotLinkProfileResult;

ZR_PARSER_API EZrBackendAotLinkProfileStatus
backend_aot_link_profile_resolve(
        const SZrAotReleaseProfile *profile,
        const SZrAotReleaseToolchain *toolchain,
        TZrBool allowFallback,
        SZrBackendAotLinkProfileResult *outResult,
        SZrAotReleasePolicyDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_link_profile_mark_roots(
        const SZrAotReleaseRoot *roots,
        TZrUInt32 rootCount,
        TZrUInt32 functionCount,
        TZrBool *retained,
        SZrAotReleasePolicyDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_LINK_PROFILE_H */
