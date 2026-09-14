#include <assert.h>
#include <string.h>

#include "zr_vm_parser/aot_generic_policy.h"

static SZrAotGenericPolicyRequest request(TZrUInt64 layout,
                                          TZrUInt64 ownership,
                                          TZrUInt64 effects) {
    SZrAotGenericPolicyRequest value;
    (void)memset(&value, 0, sizeof(value));
    value.key.signatureHash = 100u;
    value.key.layoutHash = layout;
    value.key.ownershipHash = ownership;
    value.key.effectHash = effects;
    value.key.targetAbiHash = 17u;
    value.estimatedCodeBytes = 16u;
    value.profileHotness = 80u;
    return value;
}

static void generic_sharing_requires_full_contract(void) {
    SZrAotGenericPolicyRequest left = request(11u, 21u, 31u);
    SZrAotGenericPolicyRequest same = left;
    SZrAotGenericPolicyRequest differentOwnership = left;
    SZrAotGenericPolicyRequest differentLayout = left;
    differentOwnership.key.ownershipHash++;
    differentLayout.key.layoutHash++;
    assert(ZrParser_AotGenericPolicy_CanDictionaryShare(&left, &same));
    assert(!ZrParser_AotGenericPolicy_CanDictionaryShare(&left, &differentOwnership));
    assert(!ZrParser_AotGenericPolicy_CanDictionaryShare(&left, &differentLayout));
}

static void generic_policy_prefers_hot_specialization_within_budget(void) {
    SZrAotGenericPolicyBudget budget;
    SZrAotGenericPolicyRequest value = request(11u, 21u, 31u);
    SZrAotGenericPolicyResult result;
    (void)memset(&budget, 0, sizeof(budget));
    budget.maxModuleCodeBytes = 64u;
    budget.usedModuleCodeBytes = 32u;
    budget.maxRecursiveDepth = 4u;
    budget.hotnessThreshold = 50u;
    assert(ZrParser_AotGenericPolicy_Decide(&value, &budget, &result,
                                             (SZrAotGenericPolicyDiagnostic *)0));
    assert(result.decision == ZR_AOT_GENERIC_POLICY_MONOMORPHIZE);
    assert(result.projectedModuleCodeBytes == 48u);
}

static void generic_policy_shares_cold_budgeted_and_deep_cases(void) {
    SZrAotGenericPolicyBudget budget;
    SZrAotGenericPolicyRequest value = request(11u, 21u, 31u);
    SZrAotGenericPolicyResult result;
    (void)memset(&budget, 0, sizeof(budget));
    budget.maxModuleCodeBytes = 64u;
    budget.usedModuleCodeBytes = 56u;
    budget.maxRecursiveDepth = 1u;
    budget.hotnessThreshold = 50u;
    assert(ZrParser_AotGenericPolicy_Decide(&value, &budget, &result, 0));
    assert(result.decision == ZR_AOT_GENERIC_POLICY_DICTIONARY_SHARE);
    assert(result.remark == ZR_AOT_GENERIC_REMARK_CODE_BUDGET);
    value.recursiveDepth = 2u;
    assert(ZrParser_AotGenericPolicy_Decide(&value, &budget, &result, 0));
    assert(result.remark == ZR_AOT_GENERIC_REMARK_RECURSION_DEPTH);
}

static void roots_preserve_future_capabilities_and_callbacks(void) {
    const SZrAotReleaseRoot roots[] = {
        { 1u, ZR_AOT_RELEASE_ROOT_ENTRY },
        { 3u, ZR_AOT_RELEASE_ROOT_REFLECTION },
        { 5u, ZR_AOT_RELEASE_ROOT_NATIVE_CALLBACK },
        { 7u, ZR_AOT_RELEASE_ROOT_CAPABILITY_ENTRY },
        { 9u, ZR_AOT_RELEASE_ROOT_PATCH_FUTURE_USE }
    };
    TZrBool retained[10];
    SZrAotReleasePolicyDiagnostic diagnostic;
    (void)memset(retained, 0, sizeof(retained));
    assert(ZrParser_AotReleasePolicy_MarkRoots(roots, 5u, 10u, retained,
                                                &diagnostic));
    assert(retained[1u] && retained[3u] && retained[5u] && retained[7u] && retained[9u]);
    assert(!retained[0u]);
}

static void link_profile_rejects_stale_pgo_and_unsupported_thin_lto(void) {
    SZrAotReleaseProfile profile;
    SZrAotReleaseToolchain toolchain;
    SZrAotReleaseEffectiveProfile effective;
    SZrAotReleasePolicyDiagnostic diagnostic;
    (void)memset(&profile, 0, sizeof(profile));
    (void)memset(&toolchain, 0, sizeof(toolchain));
    profile.kind = ZR_AOT_RELEASE_PROFILE_RELEASE_PGO;
    profile.targetFingerprint = 1u;
    profile.irFingerprint = 2u;
    profile.compilerFingerprint = 3u;
    profile.profileTargetFingerprint = 1u;
    profile.profileIrFingerprint = 2u;
    profile.profileCompilerFingerprint = 99u;
    toolchain.supportsLto = ZR_TRUE;
    toolchain.supportsPgo = ZR_TRUE;
    assert(!ZrParser_AotReleasePolicy_ValidateProfile(&profile, &toolchain,
                                                        &effective, &diagnostic));
    assert(diagnostic.code == ZR_AOT_RELEASE_POLICY_PROFILE_MISMATCH);
    profile.kind = ZR_AOT_RELEASE_PROFILE_RELEASE_LTO;
    toolchain.supportsThinLto = ZR_FALSE;
    toolchain.supportsLto = ZR_FALSE;
    assert(!ZrParser_AotReleasePolicy_ValidateProfile(&profile, &toolchain,
                                                        &effective, &diagnostic));
    assert(diagnostic.code == ZR_AOT_RELEASE_POLICY_LTO_UNSUPPORTED);
}

int main(void) {
    generic_sharing_requires_full_contract();
    generic_policy_prefers_hot_specialization_within_budget();
    generic_policy_shares_cold_budgeted_and_deep_cases();
    roots_preserve_future_capabilities_and_callbacks();
    link_profile_rejects_stale_pgo_and_unsupported_thin_lto();
    return 0;
}
