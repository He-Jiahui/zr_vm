#include <assert.h>
#include <string.h>

#include "zr_vm_common/ssa_platform_contract.h"

static SZrSsaPlatformCapability platform_capability(
        EZrSsaPlatformTarget target,
        EZrSsaPlatformArchitecture architecture) {
    SZrSsaPlatformCapability capability;

    ZrCommon_SsaPlatform_CapabilityInit(&capability);
    capability.target = target;
    capability.architecture = architecture;
    capability.abi.pointerWidthBits = 64u;
    capability.abi.endianness = ZR_SSA_PLATFORM_ENDIAN_LITTLE;
    capability.abi.maxAlignment = 16u;
    capability.abi.int8WidthBits = 8u;
    capability.abi.int16WidthBits = 16u;
    capability.abi.int32WidthBits = 32u;
    capability.abi.int64WidthBits = 64u;
    capability.abi.float32WidthBits = 32u;
    capability.abi.float64WidthBits = 64u;
    capability.abi.structReturnKind = ZR_SSA_PLATFORM_STRUCT_RETURN_REGISTER;
    capability.abi.nativeCallbackAbiHash = 71u;
    capability.abiHash = 101u;
    capability.numericContractHash = 102u;
    capability.layoutContractHash = 103u;
    capability.featureFlags = ZR_SSA_PLATFORM_FEATURE_EXECBC |
                              ZR_SSA_PLATFORM_FEATURE_AOT_C |
                              ZR_SSA_PLATFORM_FEATURE_AOT_LLVM |
                              ZR_SSA_PLATFORM_FEATURE_UNWIND |
                              ZR_SSA_PLATFORM_FEATURE_DEBUG |
                              ZR_SSA_PLATFORM_FEATURE_NATIVE_CALLBACKS;
    strcpy(capability.targetTriple, "x86_64-unknown-linux-gnu");
    return capability;
}

static SZrSsaPlatformObservation passing_observation(
        const SZrSsaPlatformCapability *capability,
        EZrSsaPlatformBackend backend) {
    SZrSsaPlatformObservation observation;

    ZrCommon_SsaPlatform_ObservationInit(&observation);
    observation.target = capability->target;
    observation.architecture = capability->architecture;
    observation.backend = backend;
    observation.runner = ZR_SSA_PLATFORM_RUNNER_REAL_DEVICE;
    observation.compiled = ZR_TRUE;
    observation.executed = ZR_TRUE;
    observation.semanticPassed = ZR_TRUE;
    observation.outcome = ZR_SSA_PLATFORM_OUTCOME_PASSED;
    observation.observedAbi = capability->abi;
    observation.observedAbiHash = capability->abiHash;
    observation.observedNumericContractHash = capability->numericContractHash;
    observation.observedLayoutContractHash = capability->layoutContractHash;
    strcpy(observation.targetTriple, capability->targetTriple);
    strcpy(observation.compiler, "fixture-cc");
    strcpy(observation.deviceOrRuntime, "fixture-runtime");
    return observation;
}

static SZrSsaPlatformArtifactContract artifact_contract(
        const SZrSsaPlatformCapability *capability) {
    SZrSsaPlatformArtifactContract artifact;

    ZrCommon_SsaPlatform_ArtifactContractInit(&artifact);
    artifact.target = capability->target;
    artifact.architecture = capability->architecture;
    artifact.abi = capability->abi;
    artifact.abiHash = capability->abiHash;
    artifact.numericContractHash = capability->numericContractHash;
    artifact.layoutContractHash = capability->layoutContractHash;
    strcpy(artifact.targetTriple, capability->targetTriple);
    return artifact;
}

static void test_desktop_execbc_evidence_requires_all_runtime_stages(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    SZrSsaPlatformDiagnostic diagnostic;

    assert(ZrCommon_SsaPlatform_ValidateCapability(&capability, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OK);
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OK);
    assert(ZrTests_Ssa_CheckPlatform(&capability, &observation));

    observation.executed = ZR_FALSE;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED);
    assert(diagnostic.status == ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED);
    assert(!ZrTests_Ssa_CheckPlatform(&capability, &observation));
}

static void test_artifact_rejects_pointer_and_numeric_contract_mismatch(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformArtifactContract artifact = artifact_contract(&capability);
    SZrSsaPlatformDiagnostic diagnostic;

    artifact.abi.pointerWidthBits = 32u;
    assert(ZrCommon_SsaPlatform_ValidateArtifact(
                   &capability, &artifact, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH);
    assert(diagnostic.expected == 64u);
    assert(diagnostic.actual == 32u);

    artifact = artifact_contract(&capability);
    artifact.numericContractHash++;
    assert(ZrCommon_SsaPlatform_ValidateArtifact(
                   &capability, &artifact, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_ARTIFACT_NUMERIC_CONTRACT_MISMATCH);
}

static void test_wasm_thread_and_pmu_gaps_are_unsupported_not_passed(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_WASM,
            ZR_SSA_PLATFORM_ARCH_WASM32);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "wasm32-unknown-unknown");
    capability.abi.pointerWidthBits = 32u;
    observation.requiredFeatures = ZR_SSA_PLATFORM_FEATURE_THREADS |
                                   ZR_SSA_PLATFORM_FEATURE_CONCURRENT_GC |
                                   ZR_SSA_PLATFORM_FEATURE_PMU;
    observation.outcome = ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED;
    observation.semanticPassed = ZR_FALSE;
    observation.unsupportedFeatures = observation.requiredFeatures;
    observation.observedAbi.pointerWidthBits = 32u;
    strcpy(observation.targetTriple, capability.targetTriple);

    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED);
    assert(diagnostic.unsupportedFeatures == observation.requiredFeatures);
    assert(!ZrTests_Ssa_CheckPlatform(&capability, &observation));
}

static void test_mobile_and_wasm_machine_code_jit_is_forbidden(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_IOS,
            ZR_SSA_PLATFORM_ARCH_AARCH64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_HOST_JIT);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "arm64-apple-ios");
    strcpy(observation.targetTriple, capability.targetTriple);
    observation.machineCodeJitExecuted = ZR_TRUE;

    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
    assert(diagnostic.target == ZR_SSA_PLATFORM_TARGET_IOS);
}

static void test_cross_compile_and_real_device_execution_are_distinct(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_ANDROID,
            ZR_SSA_PLATFORM_ARCH_AARCH64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_AOT_C);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "aarch64-linux-android");
    strcpy(observation.targetTriple, capability.targetTriple);
    observation.runner = ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE;
    observation.executed = ZR_FALSE;
    observation.semanticPassed = ZR_FALSE;
    observation.outcome = ZR_SSA_PLATFORM_OUTCOME_UNAVAILABLE;

    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_RUNTIME_UNAVAILABLE);
    assert(diagnostic.runner == ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE);
    assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
}

static void test_restricted_ios_patch_cannot_add_native_imports(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_IOS,
            ZR_SSA_PLATFORM_ARCH_AARCH64);
    SZrSsaPlatformArtifactContract artifact = artifact_contract(&capability);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "arm64-apple-ios");
    strcpy(artifact.targetTriple, capability.targetTriple);
    artifact.restrictedPatch = ZR_TRUE;
    artifact.newNativeImportCount = 1u;

    assert(ZrCommon_SsaPlatform_ValidateArtifact(
                   &capability, &artifact, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_RESTRICTED_PATCH_NATIVE_IMPORT);
}

static void test_capability_rejects_forbidden_mobile_jit_and_wasm32_width(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_ANDROID,
            ZR_SSA_PLATFORM_ARCH_AARCH64);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "aarch64-linux-android");
    capability.featureFlags |= ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT;
    assert(ZrCommon_SsaPlatform_ValidateCapability(&capability, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);

    capability = platform_capability(ZR_SSA_PLATFORM_TARGET_WASM,
                                     ZR_SSA_PLATFORM_ARCH_WASM32);
    strcpy(capability.targetTriple, "wasm32-unknown-unknown");
    assert(ZrCommon_SsaPlatform_ValidateCapability(&capability, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
    assert(diagnostic.abiField == ZR_SSA_PLATFORM_ABI_FIELD_POINTER_WIDTH);
}

static void test_observation_rejects_target_and_callback_abi_drift(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_WINDOWS,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_AOT_LLVM);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "x86_64-pc-windows-msvc");
    strcpy(observation.targetTriple, "x86_64-unknown-linux-gnu");
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH);

    strcpy(observation.targetTriple, capability.targetTriple);
    observation.observedAbi.nativeCallbackAbiHash++;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH);
    assert(diagnostic.abiField == ZR_SSA_PLATFORM_ABI_FIELD_NATIVE_CALLBACK);
}

static void test_host_jit_is_checked_on_both_supported_architectures(void) {
    const EZrSsaPlatformArchitecture architectures[2] = {
        ZR_SSA_PLATFORM_ARCH_X86_64,
        ZR_SSA_PLATFORM_ARCH_AARCH64
    };
    const char *triples[2] = {
        "x86_64-unknown-linux-gnu",
        "aarch64-unknown-linux-gnu"
    };
    TZrUInt32 index;

    for (index = 0u; index < 2u; ++index) {
        SZrSsaPlatformCapability capability = platform_capability(
                ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX, architectures[index]);
        SZrSsaPlatformObservation observation;
        SZrSsaPlatformDiagnostic diagnostic;

        strcpy(capability.targetTriple, triples[index]);
        capability.featureFlags |= ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT;
        observation = passing_observation(
                &capability, ZR_SSA_PLATFORM_BACKEND_HOST_JIT);
        observation.machineCodeJitExecuted = ZR_TRUE;
        assert(ZrCommon_SsaPlatform_Check(
                       &capability, &observation, &diagnostic) ==
               ZR_SSA_PLATFORM_STATUS_OK);
        assert(ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
    }
}

static void test_unavailable_backend_is_not_silently_passed(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_AOT_LLVM);
    SZrSsaPlatformDiagnostic diagnostic;

    capability.featureFlags &= ~ZR_SSA_PLATFORM_FEATURE_AOT_LLVM;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED);
    assert((diagnostic.unsupportedFeatures &
            ZR_SSA_PLATFORM_FEATURE_AOT_LLVM) != 0u);
}

static void test_diagnostic_keeps_source_and_instruction_context(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    SZrSsaPlatformDiagnostic diagnostic;

    observation.sourceId = 41u;
    observation.instructionId = 42u;
    observation.observedLayoutContractHash++;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_LAYOUT_CONTRACT_MISMATCH);
    assert(diagnostic.sourceId == 41u);
    assert(diagnostic.instructionId == 42u);
    assert(diagnostic.expected == capability.layoutContractHash);
}

static void test_dispatch_implementation_can_differ_when_semantic_witnesses_match(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_WINDOWS,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_AOT_C);
    SZrSsaPlatformDiagnostic diagnostic;

    strcpy(capability.targetTriple, "x86_64-pc-windows-msvc");
    capability.dispatchFlags = ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH |
                               ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO;
    capability.semanticResultHash = 501u;
    capability.exceptionContractHash = 502u;
    capability.sourceMapHash = 503u;
    strcpy(observation.targetTriple, capability.targetTriple);
    observation.dispatchKind = ZR_SSA_PLATFORM_DISPATCH_SWITCH;
    observation.dispatchFlags = ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH;
    observation.semanticResultHash = 501u;
    observation.exceptionContractHash = 502u;
    observation.sourceMapHash = 503u;
    assert(ZrCommon_SsaPlatform_Check(
                   &capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OK);

    observation.dispatchKind = ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO;
    observation.dispatchFlags = ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO;
    observation.exceptionContractHash++;
    assert(ZrCommon_SsaPlatform_Check(
                   &capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH);
    assert(diagnostic.witnessField == ZR_SSA_PLATFORM_WITNESS_FIELD_EXCEPTION);
}

static void test_host_abi_probe_is_valid_and_hash_is_sensitive(void) {
    SZrSsaPlatformAbi abi;
    SZrSsaPlatformAbi changed;
    SZrSsaPlatformDiagnostic diagnostic;
    TZrUInt64 hash;

    ZrCommon_SsaPlatform_DetectHostAbi(&abi);
    assert(ZrCommon_SsaPlatform_ValidateAbi(&abi, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OK);
    hash = ZrCommon_SsaPlatform_ComputeAbiHash(&abi);
    assert(hash != 0u);
    changed = abi;
    changed.nativeCallbackAbiHash++;
    assert(ZrCommon_SsaPlatform_ComputeAbiHash(&changed) != hash);
}

static void test_invalid_observed_abi_is_rejected_before_hash_comparison(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    SZrSsaPlatformDiagnostic diagnostic;

    observation.observedAbi.maxAlignment = 3u;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
    assert(diagnostic.abiField == ZR_SSA_PLATFORM_ABI_FIELD_MAX_ALIGNMENT);
}

static void test_null_and_repeated_initialization_are_safe(void) {
    SZrSsaPlatformCapability capability;
    SZrSsaPlatformObservation observation;

    ZrCommon_SsaPlatform_DiagnosticInit(ZR_NULL);
    ZrCommon_SsaPlatform_CapabilityInit(ZR_NULL);
    ZrCommon_SsaPlatform_ObservationInit(ZR_NULL);
    ZrCommon_SsaPlatform_ArtifactContractInit(ZR_NULL);
    assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(ZR_NULL));
    ZrCommon_SsaPlatform_CapabilityInit(&capability);
    ZrCommon_SsaPlatform_CapabilityInit(&capability);
    ZrCommon_SsaPlatform_ObservationInit(&observation);
    ZrCommon_SsaPlatform_ObservationInit(&observation);
    assert(capability.magic == ZR_SSA_PLATFORM_CONTRACT_MAGIC);
    assert(observation.magic == ZR_SSA_PLATFORM_CONTRACT_MAGIC);
}

int main(void) {
    test_desktop_execbc_evidence_requires_all_runtime_stages();
    test_artifact_rejects_pointer_and_numeric_contract_mismatch();
    test_wasm_thread_and_pmu_gaps_are_unsupported_not_passed();
    test_mobile_and_wasm_machine_code_jit_is_forbidden();
    test_cross_compile_and_real_device_execution_are_distinct();
    test_restricted_ios_patch_cannot_add_native_imports();
    test_capability_rejects_forbidden_mobile_jit_and_wasm32_width();
    test_observation_rejects_target_and_callback_abi_drift();
    test_host_jit_is_checked_on_both_supported_architectures();
    test_unavailable_backend_is_not_silently_passed();
    test_diagnostic_keeps_source_and_instruction_context();
    test_dispatch_implementation_can_differ_when_semantic_witnesses_match();
    test_host_abi_probe_is_valid_and_hash_is_sensitive();
    test_invalid_observed_abi_is_rejected_before_hash_comparison();
    test_null_and_repeated_initialization_are_safe();
    return 0;
}
