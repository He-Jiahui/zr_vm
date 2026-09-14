#ifndef ZR_VM_PARSER_AOT_GENERIC_POLICY_H
#define ZR_VM_PARSER_AOT_GENERIC_POLICY_H

#include "zr_vm_parser/conf.h"

typedef struct SZrAotGenericKey {
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 ownershipHash;
    TZrUInt64 effectHash;
    TZrUInt64 targetAbiHash;
} SZrAotGenericKey;

typedef struct SZrAotGenericPolicyRequest {
    SZrAotGenericKey key;
    TZrUInt32 estimatedCodeBytes;
    TZrUInt32 profileHotness;
    TZrUInt32 recursiveDepth;
} SZrAotGenericPolicyRequest;

typedef struct SZrAotGenericPolicyBudget {
    TZrUInt32 maxModuleCodeBytes;
    TZrUInt32 usedModuleCodeBytes;
    TZrUInt32 maxRecursiveDepth;
    TZrUInt32 hotnessThreshold;
} SZrAotGenericPolicyBudget;

typedef enum EZrAotGenericPolicyDecision {
    ZR_AOT_GENERIC_POLICY_DICTIONARY_SHARE = 0,
    ZR_AOT_GENERIC_POLICY_MONOMORPHIZE = 1
} EZrAotGenericPolicyDecision;

typedef enum EZrAotGenericRemark {
    ZR_AOT_GENERIC_REMARK_NONE = 0,
    ZR_AOT_GENERIC_REMARK_CODE_BUDGET,
    ZR_AOT_GENERIC_REMARK_RECURSION_DEPTH,
    ZR_AOT_GENERIC_REMARK_COLD_PROFILE,
    ZR_AOT_GENERIC_REMARK_INCOMPATIBLE_KEY
} EZrAotGenericRemark;

typedef struct SZrAotGenericPolicyResult {
    EZrAotGenericPolicyDecision decision;
    EZrAotGenericRemark remark;
    TZrUInt32 projectedModuleCodeBytes;
} SZrAotGenericPolicyResult;

typedef enum EZrAotGenericPolicyDiagnosticCode {
    ZR_AOT_GENERIC_POLICY_DIAGNOSTIC_NONE = 0,
    ZR_AOT_GENERIC_POLICY_INVALID_ARGUMENT,
    ZR_AOT_GENERIC_POLICY_INVALID_KEY,
    ZR_AOT_GENERIC_POLICY_BUDGET_OVERFLOW
} EZrAotGenericPolicyDiagnosticCode;

typedef struct SZrAotGenericPolicyDiagnostic {
    EZrAotGenericPolicyDiagnosticCode code;
    TZrUInt32 expected;
    TZrUInt32 actual;
} SZrAotGenericPolicyDiagnostic;

typedef enum EZrAotReleaseRootKind {
    ZR_AOT_RELEASE_ROOT_ENTRY = 1,
    ZR_AOT_RELEASE_ROOT_EXPORT,
    ZR_AOT_RELEASE_ROOT_REFLECTION,
    ZR_AOT_RELEASE_ROOT_NATIVE_CALLBACK,
    ZR_AOT_RELEASE_ROOT_CAPABILITY_ENTRY,
    ZR_AOT_RELEASE_ROOT_SERIALIZED_TYPE,
    ZR_AOT_RELEASE_ROOT_PATCH_FUTURE_USE
} EZrAotReleaseRootKind;

typedef struct SZrAotReleaseRoot {
    TZrUInt32 functionIndex;
    EZrAotReleaseRootKind kind;
} SZrAotReleaseRoot;

typedef enum EZrAotReleasePolicyDiagnosticCode {
    ZR_AOT_RELEASE_POLICY_DIAGNOSTIC_NONE = 0,
    ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT,
    ZR_AOT_RELEASE_POLICY_ROOT_OUT_OF_RANGE,
    ZR_AOT_RELEASE_POLICY_PROFILE_MISMATCH,
    ZR_AOT_RELEASE_POLICY_LTO_UNSUPPORTED,
    ZR_AOT_RELEASE_POLICY_PGO_UNSUPPORTED
} EZrAotReleasePolicyDiagnosticCode;

typedef struct SZrAotReleasePolicyDiagnostic {
    EZrAotReleasePolicyDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrAotReleasePolicyDiagnostic;

typedef enum EZrAotReleaseProfileKind {
    ZR_AOT_RELEASE_PROFILE_DEV = 0,
    ZR_AOT_RELEASE_PROFILE_RELEASE_LTO,
    ZR_AOT_RELEASE_PROFILE_RELEASE_THIN_LTO,
    ZR_AOT_RELEASE_PROFILE_RELEASE_PGO
} EZrAotReleaseProfileKind;

typedef struct SZrAotReleaseProfile {
    EZrAotReleaseProfileKind kind;
    TZrUInt64 targetFingerprint;
    TZrUInt64 irFingerprint;
    TZrUInt64 compilerFingerprint;
    TZrUInt64 profileTargetFingerprint;
    TZrUInt64 profileIrFingerprint;
    TZrUInt64 profileCompilerFingerprint;
} SZrAotReleaseProfile;

typedef struct SZrAotReleaseToolchain {
    TZrBool supportsLto;
    TZrBool supportsThinLto;
    TZrBool supportsPgo;
} SZrAotReleaseToolchain;

typedef struct SZrAotReleaseEffectiveProfile {
    EZrAotReleaseProfileKind kind;
    TZrBool ltoEnabled;
    TZrBool thinLtoEnabled;
    TZrBool pgoEnabled;
} SZrAotReleaseEffectiveProfile;

ZR_PARSER_API TZrBool ZrParser_AotGenericPolicy_CanDictionaryShare(
        const SZrAotGenericPolicyRequest *left,
        const SZrAotGenericPolicyRequest *right);
ZR_PARSER_API TZrBool ZrParser_AotGenericPolicy_Decide(
        const SZrAotGenericPolicyRequest *request,
        const SZrAotGenericPolicyBudget *budget,
        SZrAotGenericPolicyResult *result,
        SZrAotGenericPolicyDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_AotReleasePolicy_MarkRoots(
        const SZrAotReleaseRoot *roots,
        TZrUInt32 rootCount,
        TZrUInt32 functionCount,
        TZrBool *retained,
        SZrAotReleasePolicyDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_AotReleasePolicy_ValidateProfile(
        const SZrAotReleaseProfile *profile,
        const SZrAotReleaseToolchain *toolchain,
        SZrAotReleaseEffectiveProfile *effective,
        SZrAotReleasePolicyDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_AOT_GENERIC_POLICY_H */
