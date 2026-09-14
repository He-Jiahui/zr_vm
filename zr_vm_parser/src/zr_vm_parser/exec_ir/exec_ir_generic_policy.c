#include "zr_vm_parser/aot_generic_policy.h"

#include <string.h>

static void clear_generic_diagnostic(SZrAotGenericPolicyDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void clear_release_diagnostic(SZrAotReleasePolicyDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

TZrBool ZrParser_AotGenericPolicy_CanDictionaryShare(
        const SZrAotGenericPolicyRequest *left,
        const SZrAotGenericPolicyRequest *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    return (left->key.signatureHash == right->key.signatureHash &&
            left->key.layoutHash == right->key.layoutHash &&
            left->key.ownershipHash == right->key.ownershipHash &&
            left->key.effectHash == right->key.effectHash &&
            left->key.targetAbiHash == right->key.targetAbiHash) ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrParser_AotGenericPolicy_Decide(
        const SZrAotGenericPolicyRequest *request,
        const SZrAotGenericPolicyBudget *budget,
        SZrAotGenericPolicyResult *result,
        SZrAotGenericPolicyDiagnostic *diagnostic) {
    TZrUInt64 projected;
    clear_generic_diagnostic(diagnostic);
    if (result != ZR_NULL) {
        (void)memset(result, 0, sizeof(*result));
    }
    if (request == ZR_NULL || budget == ZR_NULL || result == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_AOT_GENERIC_POLICY_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (request->key.signatureHash == 0u || request->key.layoutHash == 0u ||
        request->key.ownershipHash == 0u || request->key.effectHash == 0u ||
        request->key.targetAbiHash == 0u) {
        diagnostic->code = ZR_AOT_GENERIC_POLICY_INVALID_KEY;
        return ZR_FALSE;
    }
    projected = (TZrUInt64)budget->usedModuleCodeBytes + request->estimatedCodeBytes;
    if (projected > 0xFFFFFFFFULL) {
        diagnostic->code = ZR_AOT_GENERIC_POLICY_BUDGET_OVERFLOW;
        return ZR_FALSE;
    }
    result->projectedModuleCodeBytes = (TZrUInt32)projected;
    result->decision = ZR_AOT_GENERIC_POLICY_DICTIONARY_SHARE;
    if (request->recursiveDepth > budget->maxRecursiveDepth) {
        result->remark = ZR_AOT_GENERIC_REMARK_RECURSION_DEPTH;
    } else if (request->profileHotness < budget->hotnessThreshold) {
        result->remark = ZR_AOT_GENERIC_REMARK_COLD_PROFILE;
    } else if (projected > budget->maxModuleCodeBytes) {
        result->remark = ZR_AOT_GENERIC_REMARK_CODE_BUDGET;
    } else {
        result->decision = ZR_AOT_GENERIC_POLICY_MONOMORPHIZE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_AotReleasePolicy_MarkRoots(
        const SZrAotReleaseRoot *roots,
        TZrUInt32 rootCount,
        TZrUInt32 functionCount,
        TZrBool *retained,
        SZrAotReleasePolicyDiagnostic *diagnostic) {
    TZrUInt32 index;
    clear_release_diagnostic(diagnostic);
    if (retained != ZR_NULL && functionCount != 0u) {
        (void)memset(retained, 0, (TZrSize)functionCount * sizeof(*retained));
    }
    if ((rootCount != 0u && roots == ZR_NULL) || retained == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    for (index = 0u; index < rootCount; ++index) {
        if (roots[index].functionIndex >= functionCount) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_AOT_RELEASE_POLICY_ROOT_OUT_OF_RANGE;
                diagnostic->index = index;
                diagnostic->expected = functionCount;
                diagnostic->actual = roots[index].functionIndex;
            }
            (void)memset(retained, 0, (TZrSize)functionCount * sizeof(*retained));
            return ZR_FALSE;
        }
        retained[roots[index].functionIndex] = ZR_TRUE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_AotReleasePolicy_ValidateProfile(
        const SZrAotReleaseProfile *profile,
        const SZrAotReleaseToolchain *toolchain,
        SZrAotReleaseEffectiveProfile *effective,
        SZrAotReleasePolicyDiagnostic *diagnostic) {
    clear_release_diagnostic(diagnostic);
    if (effective != ZR_NULL) {
        (void)memset(effective, 0, sizeof(*effective));
    }
    if (profile == ZR_NULL || toolchain == ZR_NULL || effective == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    effective->kind = profile->kind;
    if (profile->kind == ZR_AOT_RELEASE_PROFILE_DEV) {
        return ZR_TRUE;
    }
    if (profile->kind == ZR_AOT_RELEASE_PROFILE_RELEASE_LTO) {
        if (!toolchain->supportsLto) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_LTO_UNSUPPORTED;
            return ZR_FALSE;
        }
        effective->ltoEnabled = ZR_TRUE;
        return ZR_TRUE;
    }
    if (profile->kind == ZR_AOT_RELEASE_PROFILE_RELEASE_THIN_LTO) {
        if (!toolchain->supportsThinLto) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_LTO_UNSUPPORTED;
            return ZR_FALSE;
        }
        effective->ltoEnabled = ZR_TRUE;
        effective->thinLtoEnabled = ZR_TRUE;
        return ZR_TRUE;
    }
    if (profile->kind == ZR_AOT_RELEASE_PROFILE_RELEASE_PGO) {
        if (!toolchain->supportsPgo) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_PGO_UNSUPPORTED;
            return ZR_FALSE;
        }
        if (profile->targetFingerprint != profile->profileTargetFingerprint ||
            profile->irFingerprint != profile->profileIrFingerprint ||
            profile->compilerFingerprint != profile->profileCompilerFingerprint) {
            diagnostic->code = ZR_AOT_RELEASE_POLICY_PROFILE_MISMATCH;
            diagnostic->expected = profile->compilerFingerprint;
            diagnostic->actual = profile->profileCompilerFingerprint;
            return ZR_FALSE;
        }
        effective->pgoEnabled = ZR_TRUE;
        return ZR_TRUE;
    }
    diagnostic->code = ZR_AOT_RELEASE_POLICY_INVALID_ARGUMENT;
    return ZR_FALSE;
}
