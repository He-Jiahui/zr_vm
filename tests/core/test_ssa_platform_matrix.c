/* Acceptance assertions must execute even when production is built NDEBUG. */
#ifdef NDEBUG
#undef NDEBUG
#endif
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

static void test_cross_compile_cannot_claim_execution_or_a_passed_outcome(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX, ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation;
    SZrSsaPlatformDiagnostic diagnostic;
    TZrUInt32 stages;

    for (stages = 0u; stages < 4u; ++stages) {
        observation = passing_observation(&capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
        observation.runner = ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE;
        observation.executed = (stages & 1u) != 0u;
        observation.semanticPassed = (stages & 2u) != 0u;
        observation.sourceId = 61u;
        observation.instructionId = 62u;
        assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
               ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        assert(diagnostic.status == ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        assert(diagnostic.runner == ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE);
        assert(diagnostic.sourceId == 61u && diagnostic.instructionId == 62u);
        assert(!ZrTests_Ssa_CheckPlatform(&capability, &observation));
        assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
        if (stages != 0u) {
            observation.outcome = ZR_SSA_PLATFORM_OUTCOME_FAILED;
            assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
                   ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
            assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
        }
    }
}

static void test_execution_runners_remain_eligible_for_runtime_acceptance(void) {
    const EZrSsaPlatformRunner runners[] = {
        ZR_SSA_PLATFORM_RUNNER_HOST, ZR_SSA_PLATFORM_RUNNER_EMULATOR,
        ZR_SSA_PLATFORM_RUNNER_REAL_DEVICE, ZR_SSA_PLATFORM_RUNNER_BROWSER_RUNTIME,
        ZR_SSA_PLATFORM_RUNNER_WASM_RUNTIME
    };
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX, ZR_SSA_PLATFORM_ARCH_X86_64);
    TZrSize index;

    for (index = 0u; index < sizeof(runners) / sizeof(runners[0]); ++index) {
        SZrSsaPlatformObservation observation = passing_observation(
                &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
        SZrSsaPlatformDiagnostic diagnostic;
        observation.runner = runners[index];
        assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
               ZR_SSA_PLATFORM_STATUS_OK);
        assert(ZrTests_Ssa_CheckPlatform(&capability, &observation));
        assert(ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
    }
}

static void test_forbidden_jit_observations_fail_both_acceptance_entry_points(void) {
    const EZrSsaPlatformTarget targets[] = {
        ZR_SSA_PLATFORM_TARGET_ANDROID, ZR_SSA_PLATFORM_TARGET_IOS,
        ZR_SSA_PLATFORM_TARGET_WASM
    };
    TZrSize index;
    TZrUInt32 trigger;

    for (index = 0u; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        SZrSsaPlatformCapability capability = platform_capability(targets[index],
                targets[index] == ZR_SSA_PLATFORM_TARGET_WASM ?
                ZR_SSA_PLATFORM_ARCH_WASM64 : ZR_SSA_PLATFORM_ARCH_AARCH64);
        strcpy(capability.targetTriple, targets[index] == ZR_SSA_PLATFORM_TARGET_WASM ?
                "wasm64-unknown-unknown" : "aarch64-fixture-mobile");
        for (trigger = 0u; trigger < 3u; ++trigger) {
            SZrSsaPlatformObservation observation = passing_observation(
                    &capability, trigger == 0u ? ZR_SSA_PLATFORM_BACKEND_HOST_JIT :
                    ZR_SSA_PLATFORM_BACKEND_EXECBC);
            SZrSsaPlatformDiagnostic diagnostic;
            observation.machineCodeJitExecuted = trigger == 1u;
            observation.requiredFeatures = trigger == 2u ?
                    ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT : 0u;
            assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
                   ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
            assert(diagnostic.status == ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
            assert(diagnostic.target == targets[index]);
            assert(diagnostic.backend == observation.backend);
            assert(!ZrTests_Ssa_CheckPlatform(&capability, &observation));
            assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
        }
    }
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

static void test_passed_observation_cannot_advertise_unsupported_features(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    SZrSsaPlatformDiagnostic diagnostic;

    capability.featureFlags |= ZR_SSA_PLATFORM_FEATURE_THREADS;
    observation.requiredFeatures = ZR_SSA_PLATFORM_FEATURE_THREADS;
    observation.unsupportedFeatures = ZR_SSA_PLATFORM_FEATURE_THREADS;
    assert(ZrCommon_SsaPlatform_Check(&capability, &observation, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
    assert(diagnostic.unsupportedFeatures == observation.unsupportedFeatures);
    assert(!ZrTests_Ssa_CheckPlatform(&capability, &observation));
    assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
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

static void test_runtime_acceptance_fails_closed_for_malformed_observations(void) {
    SZrSsaPlatformCapability capability = platform_capability(
            ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
            ZR_SSA_PLATFORM_ARCH_X86_64);
    SZrSsaPlatformObservation observation = passing_observation(
            &capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);

    assert(ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
    observation.magic = 0u;
    assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
    observation = passing_observation(&capability, ZR_SSA_PLATFORM_BACKEND_EXECBC);
    observation.dispatchKind = ZR_SSA_PLATFORM_DISPATCH_SWITCH;
    observation.dispatchFlags = 0u;
    assert(!ZrCommon_SsaPlatform_IsRuntimeAcceptance(&observation));
}

int main(void) {
    test_desktop_execbc_evidence_requires_all_runtime_stages();
    test_artifact_rejects_pointer_and_numeric_contract_mismatch();
    test_wasm_thread_and_pmu_gaps_are_unsupported_not_passed();
    test_mobile_and_wasm_machine_code_jit_is_forbidden();
    test_cross_compile_and_real_device_execution_are_distinct();
    test_cross_compile_cannot_claim_execution_or_a_passed_outcome();
    test_execution_runners_remain_eligible_for_runtime_acceptance();
    test_forbidden_jit_observations_fail_both_acceptance_entry_points();
    test_restricted_ios_patch_cannot_add_native_imports();
    test_capability_rejects_forbidden_mobile_jit_and_wasm32_width();
    test_observation_rejects_target_and_callback_abi_drift();
    test_host_jit_is_checked_on_both_supported_architectures();
    test_unavailable_backend_is_not_silently_passed();
    test_passed_observation_cannot_advertise_unsupported_features();
    test_diagnostic_keeps_source_and_instruction_context();
    test_dispatch_implementation_can_differ_when_semantic_witnesses_match();
    test_host_abi_probe_is_valid_and_hash_is_sensitive();
    test_invalid_observed_abi_is_rejected_before_hash_comparison();
    test_null_and_repeated_initialization_are_safe();
    test_runtime_acceptance_fails_closed_for_malformed_observations();
    return 0;
}

/**
 * @fn SZrSsaPlatformCapability platform_capability( EZrSsaPlatformTarget target, EZrSsaPlatformArchitecture architecture)
 *
 * ## Summary
 * 为各平台值契约场景构造固定的合成能力声明，减少重复填字段而不冒充目标探针。
 *
 * ## Parameters
 * @param[in] target 原样写入目标字段的枚举；助手不校验值域或与三元组的一致性。
 * @param[in] architecture 原样写入架构字段的枚举；调用方按测试场景另行调整ABI或三元组。
 *
 * ## Returns
 * @return 按值返回初始化后的独立能力记录；没有失败返回、分配或合法性认证，任意传入枚举仍会被原样写入。
 *
 * ## Constraints and lifecycle
 * - 仅使用自动存储期的值记录和定长内嵌数组，不取得堆、线程、GC根、设备或回调资源；返回记录不借用局部对象地址。
 * - 先CapabilityInit，再填64位指针、little端序、16字节对齐、固定整数/浮点宽度、REGISTER返回类别与人工回调哈希71。ABI/数值/布局外层哈希为101/102/103，均非设备探测结果。
 * - 固定声明ExecBC、AOT C、AOT LLVM、UNWIND、DEBUG、NATIVE_CALLBACKS六位，不包含线程/并发GC/PMU/JIT。默认两个dispatch许可来自初始化；三个可选语义见证保持零。
 * - 固定targetTriple是x86_64-unknown-linux-gnu。该助手不会按target/architecture更换字符串，也不会为WASM32改指针宽度；需要的case在消费前自行改写。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:10
 * - tests/core/test_ssa_platform_matrix.c:15
 * - tests/core/test_ssa_platform_matrix.c:18
 * - tests/core/test_ssa_platform_matrix.c:28
 * - tests/core/test_ssa_platform_matrix.c:29
 * - tests/core/test_ssa_platform_matrix.c:32
 * - tests/core/test_ssa_platform_matrix.c:38
 * - tests/core/test_ssa_platform_matrix.c:82
 * - tests/core/test_ssa_platform_matrix.c:132
 * - tests/core/test_ssa_platform_matrix.c:139
 * - tests/core/test_ssa_platform_matrix.c:299
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:577
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:589
 */

/**
 * @fn SZrSsaPlatformObservation passing_observation( const SZrSsaPlatformCapability *capability, EZrSsaPlatformBackend backend)
 *
 * ## Summary
 * 从已构造的声明复制身份与三个契约哈希，并人工设置通过阶段，作为检查器正负例的起点。
 *
 * ## Parameters
 * @param[in] capability 非空、稳定可读的声明；targetTriple须在96字节内NUL结束以满足strcpy源/目标范围，当前调用由固定fixture或有界字面量提供。
 * @param[in] backend 写入观察后端字段的枚举，助手不校验或检测该后端。
 *
 * ## Returns
 * @return 按值返回独立观察值；没有失败通道。REAL_DEVICE/PASSED与阶段真均是人工标签，不表示真实设备执行。
 *
 * ## Constraints and lifecycle
 * - 仅使用自动存储期的值记录和定长内嵌数组，不取得堆、线程、GC根、设备或回调资源；返回记录不借用局部对象地址。
 * - ObservationInit后复制target/architecture、内嵌ABI及ABI/数值/布局外层哈希；将compiled/executed/semanticPassed置真，runner置REAL_DEVICE，outcome置PASSED。
 * - 不复制声明的semanticResultHash/exceptionContractHash/sourceMapHash，三维见证仍为零；需要非零见证的case显式填写。requiredFeatures/unsupportedFeatures、dispatchKind/flags及JIT标记保留初始化值。
 * - targetTriple逐字复制到等容量内嵌数组；compiler/deviceOrRuntime填固定fixture文本。不保存capability地址；输入为空或字符串不结束不由本助手安全拒绝。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:42
 * - tests/core/test_ssa_platform_matrix.c:47
 * - tests/core/test_ssa_platform_matrix.c:48
 * - tests/core/test_ssa_platform_matrix.c:51
 * - tests/core/test_ssa_platform_matrix.c:56
 * - tests/core/test_ssa_platform_matrix.c:60
 * - tests/core/test_ssa_platform_matrix.c:61
 * - tests/core/test_ssa_platform_matrix.c:62
 * - tests/core/test_ssa_platform_matrix.c:85
 * - tests/core/test_ssa_platform_matrix.c:194
 * - tests/core/test_ssa_platform_matrix.c:227
 * - tests/core/test_ssa_platform_matrix.c:253
 * - tests/core/test_ssa_platform_matrix.c:421
 * - tests/core/test_ssa_platform_matrix.c:424
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:682
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:693
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:950
 */

/**
 * @fn SZrSsaPlatformArtifactContract artifact_contract( const SZrSsaPlatformCapability *capability)
 *
 * ## Summary
 * 从合成声明建立装载前产物值，为ABI、契约哈希及iOS补丁策略负例提供独立可修改记录。
 *
 * ## Parameters
 * @param[in] capability 非空、稳定可读且targetTriple在固定容量内NUL结束的声明；当前fixture满足strcpy所需范围。
 *
 * ## Returns
 * @return 按值返回独立产物记录；没有错误码、分配或真实装载行为。
 *
 * ## Constraints and lifecycle
 * - 仅使用自动存储期的值记录和定长内嵌数组，不取得堆、线程、GC根、设备或回调资源；返回记录不借用局部对象地址。
 * - ArtifactContractInit后复制目标/架构、内嵌ABI、三个外层契约哈希及三元组；requiredFeatures、machineCodeJit、restrictedPatch和newNativeImportCount仍为零。
 * - 助手不验证声明、哈希真实性或目标ABI；返回值可能不适合任意target，调用方按场景补齐字段后才交ValidateArtifact。
 * - 不借用输入字符串或保留capability指针；NULL输入或未结束字符串不由本助手检查。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:66
 * - tests/core/test_ssa_platform_matrix.c:70
 * - tests/core/test_ssa_platform_matrix.c:71
 * - tests/core/test_ssa_platform_matrix.c:73
 * - tests/core/test_ssa_platform_matrix.c:74
 * - tests/core/test_ssa_platform_matrix.c:77
 * - tests/core/test_ssa_platform_matrix.c:106
 * - tests/core/test_ssa_platform_matrix.c:116
 * - tests/core/test_ssa_platform_matrix.c:275
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:992
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1002
 */

/**
 * @fn void test_desktop_execbc_evidence_requires_all_runtime_stages(void)
 *
 * ## Summary
 * 桌面合成观察先通过声明/Check/布尔包装；仅将executed清假后断言RUNTIME_NOT_EXECUTED及包装为假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 首个负例门禁是Check的executed检查；不证明真实ExecBC执行。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 负例仅清executed字段；未独立将compiled或semanticPassed清假，不把名称中的all stages扩大为三种负例均覆盖。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:81
 * - tests/core/test_ssa_platform_matrix.c:504
 * - tests/core/test_ssa_platform_matrix.c:89
 * - tests/core/test_ssa_platform_matrix.c:91
 * - tests/core/test_ssa_platform_matrix.c:93
 * - tests/core/test_ssa_platform_matrix.c:96
 * - tests/core/test_ssa_platform_matrix.c:98
 * - tests/core/test_ssa_platform_matrix.c:99
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:709
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:889
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1142
 * - tests/core/test_ssa_platform_matrix.c:95
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:884
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:894
 */

/**
 * @fn void test_artifact_rejects_pointer_and_numeric_contract_mismatch(void)
 *
 * ## Summary
 * 产物ABI指针位宽不同得到ARTIFACT_ABI_MISMATCH及64/32细分；修复后仅数值契约哈希变化得到ARTIFACT_NUMERIC_CONTRACT_MISMATCH。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 两个顺序独立的合成负例；不加载产物，也不证明所有ABI维度或哈希碰撞性质。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:102
 * - tests/core/test_ssa_platform_matrix.c:505
 * - tests/core/test_ssa_platform_matrix.c:110
 * - tests/core/test_ssa_platform_matrix.c:113
 * - tests/core/test_ssa_platform_matrix.c:114
 * - tests/core/test_ssa_platform_matrix.c:118
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1055
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1070
 */

/**
 * @fn void test_wasm_thread_and_pmu_gaps_are_unsupported_not_passed(void)
 *
 * ## Summary
 * WASM所需THREADS/CONCURRENT_GC/PMU不在声明中，断言REQUIRED_FEATURE_UNSUPPORTED及unsupported位和布尔包装为假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 首拒绝是需求不在声明的Check832，尚未到OUTCOME_UNSUPPORTED860；不是WASM PMU声明策略650的覆盖证明。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 先在132及139把声明和观察ABI指针位宽均改32，再142调用Check；因此WASM位宽门禁通过，需求位不在声明是首个负例。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:123
 * - tests/core/test_ssa_platform_matrix.c:506
 * - tests/core/test_ssa_platform_matrix.c:142
 * - tests/core/test_ssa_platform_matrix.c:144
 * - tests/core/test_ssa_platform_matrix.c:145
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:832
 * - tests/core/test_ssa_platform_matrix.c:132
 * - tests/core/test_ssa_platform_matrix.c:139
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:626
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:860
 */

/**
 * @fn void test_mobile_and_wasm_machine_code_jit_is_forbidden(void)
 *
 * ## Summary
 * iOS/AArch64观察选择HOST_JIT并带执行标记时断言MACHINE_CODE_JIT_FORBIDDEN及诊断目标。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 共享来源策略在后端能力检查之前拒绝；函数名提及WASM不代表本case单独遍历WASM。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 该case实际只构造iOS/AArch64；Android/WASM的组合在另一个三目标循环case中，不能由本case名字重复授信。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:148
 * - tests/core/test_ssa_platform_matrix.c:507
 * - tests/core/test_ssa_platform_matrix.c:160
 * - tests/core/test_ssa_platform_matrix.c:162
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:200
 * - tests/core/test_ssa_platform_matrix.c:149
 * - tests/core/test_ssa_platform_matrix.c:150
 * - tests/core/test_ssa_platform_matrix.c:151
 * - tests/core/test_ssa_platform_matrix.c:156
 * - tests/core/test_ssa_platform_matrix.c:158
 */

/**
 * @fn void test_cross_compile_and_real_device_execution_are_distinct(void)
 *
 * ## Summary
 * Android交叉编译观察未声称executed/semantic且outcome UNAVAILABLE，断言RUNTIME_UNAVAILABLE、runner与有限谓词假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - Check到UNAVAILABLE分支；IsRuntimeAcceptance可在阶段短路处返回假，不能据该断言说其policy分支已执行。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - passing_observation先产生人工REAL_DEVICE标签，随后本case只改为CROSS_COMPILE并取消运行阶段；没有在本case再执行真实设备或断言真实设备正例。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:165
 * - tests/core/test_ssa_platform_matrix.c:508
 * - tests/core/test_ssa_platform_matrix.c:180
 * - tests/core/test_ssa_platform_matrix.c:182
 * - tests/core/test_ssa_platform_matrix.c:183
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:207
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:850
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:984
 * - tests/core/test_ssa_platform_matrix.c:169
 * - tests/core/test_ssa_platform_matrix.c:175
 * - tests/core/test_ssa_platform_matrix.c:176
 * - tests/core/test_ssa_platform_matrix.c:177
 * - tests/core/test_ssa_platform_matrix.c:178
 */

/**
 * @fn void test_cross_compile_cannot_claim_execution_or_a_passed_outcome(void)
 *
 * ## Summary
 * 四种executed/semantic组合搭配CROSS_COMPILE与PASSED均拒绝；非零阶段改FAILED仍拒绝，并检查status/runner/sourceId/instructionId及两个布尔入口。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - Check共享策略可达；IsRuntimeAcceptance仅更早stage/outcome条件全真时才调用policy，其余假断言不授后续分支信用。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - stages循环0..3，各位分别控制executed/semanticPassed；初始outcome保持PASSED，使所有四轮Check均在来源策略拒绝。仅stages非零时再改FAILED测试。有限谓词的&&会先短路，不把每个false都归为policy已执行。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:186
 * - tests/core/test_ssa_platform_matrix.c:509
 * - tests/core/test_ssa_platform_matrix.c:200
 * - tests/core/test_ssa_platform_matrix.c:202
 * - tests/core/test_ssa_platform_matrix.c:203
 * - tests/core/test_ssa_platform_matrix.c:204
 * - tests/core/test_ssa_platform_matrix.c:205
 * - tests/core/test_ssa_platform_matrix.c:206
 * - tests/core/test_ssa_platform_matrix.c:209
 * - tests/core/test_ssa_platform_matrix.c:211
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:207
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 * - tests/core/test_ssa_platform_matrix.c:193
 * - tests/core/test_ssa_platform_matrix.c:195
 * - tests/core/test_ssa_platform_matrix.c:196
 * - tests/core/test_ssa_platform_matrix.c:197
 * - tests/core/test_ssa_platform_matrix.c:207
 * - tests/core/test_ssa_platform_matrix.c:208
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:198
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:984
 */

/**
 * @fn void test_execution_runners_remain_eligible_for_runtime_acceptance(void)
 *
 * ## Summary
 * 五种非交叉编译runner枚举的同一Linux合成观察均满足Check/包装/有限谓词。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 这是五种来源标签的值契约正例，不是运行五个真实runner、设备或平台。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:216
 * - tests/core/test_ssa_platform_matrix.c:510
 * - tests/core/test_ssa_platform_matrix.c:231
 * - tests/core/test_ssa_platform_matrix.c:233
 * - tests/core/test_ssa_platform_matrix.c:234
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 */

/**
 * @fn void test_forbidden_jit_observations_fail_both_acceptance_entry_points(void)
 *
 * ## Summary
 * Android/iOS/WASM各与HOST_JIT、machineCodeJitExecuted、required JIT三触发组合，检查MACHINE_CODE_JIT_FORBIDDEN、诊断上下文及两个布尔入口假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 九个静态构造场景；无实际JIT执行或设备权限探测。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 外层3target配Android/iOS的AArch64与WASM64及对应字面量三元组，内层3trigger分别选HOST_JIT、设置执行标记、填JIT需求位；共享policy先于后端能力/需求子集门禁。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:238
 * - tests/core/test_ssa_platform_matrix.c:511
 * - tests/core/test_ssa_platform_matrix.c:260
 * - tests/core/test_ssa_platform_matrix.c:262
 * - tests/core/test_ssa_platform_matrix.c:263
 * - tests/core/test_ssa_platform_matrix.c:264
 * - tests/core/test_ssa_platform_matrix.c:265
 * - tests/core/test_ssa_platform_matrix.c:266
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:200
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 * - tests/core/test_ssa_platform_matrix.c:239
 * - tests/core/test_ssa_platform_matrix.c:246
 * - tests/core/test_ssa_platform_matrix.c:247
 * - tests/core/test_ssa_platform_matrix.c:250
 * - tests/core/test_ssa_platform_matrix.c:252
 * - tests/core/test_ssa_platform_matrix.c:254
 * - tests/core/test_ssa_platform_matrix.c:257
 * - tests/core/test_ssa_platform_matrix.c:258
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:198
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:822
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:832
 */

/**
 * @fn void test_restricted_ios_patch_cannot_add_native_imports(void)
 *
 * ## Summary
 * iOS受限补丁且newNativeImportCount=1时，产物检查拒绝RESTRICTED_PATCH_NATIVE_IMPORT。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 只检查元数据条件，不实施导入、部署或运行。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:271
 * - tests/core/test_ssa_platform_matrix.c:512
 * - tests/core/test_ssa_platform_matrix.c:283
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1103
 */

/**
 * @fn void test_capability_rejects_forbidden_mobile_jit_and_wasm32_width(void)
 *
 * ## Summary
 * Android声明JIT位被拒；随后WASM32配64位ABI被拒为ABI_INVALID并指明pointer width。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 是ValidateCapability两个有序负例；不外推IsRuntimeAcceptance包含WASM架构位宽绑定。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:288
 * - tests/core/test_ssa_platform_matrix.c:513
 * - tests/core/test_ssa_platform_matrix.c:296
 * - tests/core/test_ssa_platform_matrix.c:302
 * - tests/core/test_ssa_platform_matrix.c:304
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:642
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:626
 */

/**
 * @fn void test_observation_rejects_target_and_callback_abi_drift(void)
 *
 * ## Summary
 * 观察triple与声明不同先拒绝；修复triple后回调ABI哈希不同拒为ABI_MISMATCH并指明native callback字段。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 只比较人工字符串与ABI字段，不调用回调或解析triple的target语义。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:307
 * - tests/core/test_ssa_platform_matrix.c:514
 * - tests/core/test_ssa_platform_matrix.c:317
 * - tests/core/test_ssa_platform_matrix.c:322
 * - tests/core/test_ssa_platform_matrix.c:324
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:764
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:773
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:563
 */

/**
 * @fn void test_host_jit_is_checked_on_both_supported_architectures(void)
 *
 * ## Summary
 * Linux的x86_64和AArch64两种合成身份声明JIT并报告已执行，Check和有限谓词均接受。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 只证明记录值的两种枚举组合；没有在两种机器架构执行JIT。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 两种架构及triple来自自动数组，只在同一进程中变更记录；ABI仍使用相同人工64位值，不是跨架构执行或JIT生成。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:327
 * - tests/core/test_ssa_platform_matrix.c:515
 * - tests/core/test_ssa_platform_matrix.c:349
 * - tests/core/test_ssa_platform_matrix.c:352
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 * - tests/core/test_ssa_platform_matrix.c:328
 * - tests/core/test_ssa_platform_matrix.c:332
 * - tests/core/test_ssa_platform_matrix.c:338
 * - tests/core/test_ssa_platform_matrix.c:340
 * - tests/core/test_ssa_platform_matrix.c:344
 * - tests/core/test_ssa_platform_matrix.c:345
 * - tests/core/test_ssa_platform_matrix.c:346
 * - tests/core/test_ssa_platform_matrix.c:348
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:200
 */

/**
 * @fn void test_unavailable_backend_is_not_silently_passed(void)
 *
 * ## Summary
 * 清除声明AOT_LLVM位，观察仍选LLVM后端，断言BACKEND_UNSUPPORTED及unsupportedFeatures含该位。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 后端位门禁先拒绝，不是requiredFeatures832或实际LLVM执行测试。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:356
 * - tests/core/test_ssa_platform_matrix.c:516
 * - tests/core/test_ssa_platform_matrix.c:365
 * - tests/core/test_ssa_platform_matrix.c:367
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:823
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:824
 */

/**
 * @fn void test_passed_observation_cannot_advertise_unsupported_features(void)
 *
 * ## Summary
 * 已声明且required THREADS同时被PASSED观察列为unsupported，断言OBSERVATION_INVALID、诊断集合及布尔入口假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 因声明含该位，Check需求门禁已通过；具体首拒绝是PASSED携带不支持位875。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:371
 * - tests/core/test_ssa_platform_matrix.c:517
 * - tests/core/test_ssa_platform_matrix.c:382
 * - tests/core/test_ssa_platform_matrix.c:384
 * - tests/core/test_ssa_platform_matrix.c:385
 * - tests/core/test_ssa_platform_matrix.c:386
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:875
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:987
 */

/**
 * @fn void test_diagnostic_keeps_source_and_instruction_context(void)
 *
 * ## Summary
 * layout外层哈希变化得到LAYOUT_CONTRACT_MISMATCH，诊断保留source41/instruction42和expected布局哈希。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 证明早期上下文复制和布局差异字段；不证明所有诊断字段都会清空或每层校验都清整记录。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:389
 * - tests/core/test_ssa_platform_matrix.c:518
 * - tests/core/test_ssa_platform_matrix.c:400
 * - tests/core/test_ssa_platform_matrix.c:402
 * - tests/core/test_ssa_platform_matrix.c:403
 * - tests/core/test_ssa_platform_matrix.c:404
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:703
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:796
 */

/**
 * @fn void test_dispatch_implementation_can_differ_when_semantic_witnesses_match(void)
 *
 * ## Summary
 * switch路径的三个相同可选见证通过；computed-goto标签下仅异常见证加一，拒绝并指明EXCEPTION。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 没有执行两种实际分派；只覆盖异常见证负例，不宣称三个维度的独立负例全覆盖。 正例只断言switch；没有在三维见证相等时再断言computed-goto成功。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:407
 * - tests/core/test_ssa_platform_matrix.c:519
 * - tests/core/test_ssa_platform_matrix.c:427
 * - tests/core/test_ssa_platform_matrix.c:434
 * - tests/core/test_ssa_platform_matrix.c:437
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:900
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:912
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:925
 */

/**
 * @fn void test_host_abi_probe_is_valid_and_hash_is_sensitive(void)
 *
 * ## Summary
 * 宿主探针产生的ABI通过ValidateAbi，计算哈希非零；复制后回调哈希加一使本次摘要不同。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 探针仍是通用占位；单个差异正例不能证明无碰撞、实际回调ABI或所有平台。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - DetectHostAbi只探当前编译宿主基本字段，MIXED与回调哈希仍是通用占位；单次changed.nativeCallbackAbiHash++导致摘要变化的断言不是无碰撞或真实callback证明。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:440
 * - tests/core/test_ssa_platform_matrix.c:520
 * - tests/core/test_ssa_platform_matrix.c:446
 * - tests/core/test_ssa_platform_matrix.c:447
 * - tests/core/test_ssa_platform_matrix.c:449
 * - tests/core/test_ssa_platform_matrix.c:450
 * - tests/core/test_ssa_platform_matrix.c:453
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:348
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:374
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:384
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:395
 * - tests/core/test_ssa_platform_matrix.c:452
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:371
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:377
 */

/**
 * @fn void test_invalid_observed_abi_is_rejected_before_hash_comparison(void)
 *
 * ## Summary
 * 观察maxAlignment=3，在外层哈希比对前拒绝ABI_INVALID并指明MAX_ALIGNMENT。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 首个负例为观察ABI内部对齐校验，而不是observedAbiHash差异。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - 只把观察ABI对齐改3，声明ABI保持fixture有效；Check先ValidateAbi观察而在检查observedAbiHash前拒绝，不是一个外层哈希不相等测试。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:456
 * - tests/core/test_ssa_platform_matrix.c:521
 * - tests/core/test_ssa_platform_matrix.c:465
 * - tests/core/test_ssa_platform_matrix.c:467
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:769
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:420
 * - tests/core/test_ssa_platform_matrix.c:464
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:778
 */

/**
 * @fn void test_null_and_repeated_initialization_are_safe(void)
 *
 * ## Summary
 * Diagnostic/Capability/Observation/Artifact初始化可收NULL；NULL有限谓词假；Capability/Observation重复初始化后magic正确。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 未直接断言AbiInit(NULL)、DetectHostAbi(NULL)或ComputeAbiHash(NULL)；不能外推未读字段全部正确。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 * - NULL调用覆盖Diagnostic/Capability/Observation/Artifact初始化与有限谓词；重复调用仅对capability/observation作magic断言，不新增AbiInit(NULL)、DetectHostAbi(NULL)、ComputeAbiHash(NULL)或所有字段的信用。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:470
 * - tests/core/test_ssa_platform_matrix.c:522
 * - tests/core/test_ssa_platform_matrix.c:474
 * - tests/core/test_ssa_platform_matrix.c:475
 * - tests/core/test_ssa_platform_matrix.c:476
 * - tests/core/test_ssa_platform_matrix.c:477
 * - tests/core/test_ssa_platform_matrix.c:478
 * - tests/core/test_ssa_platform_matrix.c:479
 * - tests/core/test_ssa_platform_matrix.c:480
 * - tests/core/test_ssa_platform_matrix.c:481
 * - tests/core/test_ssa_platform_matrix.c:482
 * - tests/core/test_ssa_platform_matrix.c:483
 * - tests/core/test_ssa_platform_matrix.c:484
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:248
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:577
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:682
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:992
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:952
 */

/**
 * @fn void test_runtime_acceptance_fails_closed_for_malformed_observations(void)
 *
 * ## Summary
 * 有效合成观察先满足有限谓词，magic清零或SWITCH选择缺对应许可位时分别为假。
 *
 * ## Returns
 * void：本场景的断言均完成则正常返回main；断言失败abort当前测试程序，不返回失败码。
 *
 * ## Constraints and lifecycle
 * - 每个场景仅由普通C main调用；标准assert启用时失败会abort当前测试程序，后续断言与case不再保证执行。void case没有C返回值或失败码，不是Unity runner/longjmp。
 * - 记录均为栈上纯值；标准assert失败触发abort，后续case不保证执行；无Unity longjmp、局部堆池或资源归还承诺。
 * - 两个负例按magic/分派选择门禁拒绝；不证明所有畸形枚举或掩码分支。
 * - 构造的runner、后端、target与哈希是值记录；不据case名、循环标签或返回OK授予真实线程、PMU、JIT、设备、回调或多架构运行证明。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:487
 * - tests/core/test_ssa_platform_matrix.c:523
 * - tests/core/test_ssa_platform_matrix.c:494
 * - tests/core/test_ssa_platform_matrix.c:496
 * - tests/core/test_ssa_platform_matrix.c:500
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:952
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:974
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 作为矩阵契约测试可执行程序的C入口，按固定顺序运行20个静态断言场景。
 *
 * ## Returns
 * @return 所有case正常返回后返回整数0；标准assert失败会abort进程而不走本函数的return，不存在Unity失败计数或继续运行保证。
 *
 * ## Constraints and lifecycle
 * - 调用序列在504..523逐项显式列出；没有RUN_TEST、Unity setUp/tearDown、函数指针case表、动态插件或线程派发。
 * - 本文件先取消NDEBUG再包含assert.h，保持当前TU断言活跃；构建/CTest进程状态仍须实际运行观察。
 * - 各case使用栈上值记录，main不取得或归还堆/设备/GC根；失败时没有本地堆池清理承诺。
 * - 本文件无共享可变全局、锁或原子发布；当前main串行执行场景，若单独调用助手，调用方须保证被读输入存活稳定且无并发写入。
 *
 * ## References
 * - tests/core/test_ssa_platform_matrix.c:1
 * - tests/core/test_ssa_platform_matrix.c:2
 * - tests/core/test_ssa_platform_matrix.c:3
 * - tests/core/test_ssa_platform_matrix.c:5
 * - tests/core/test_ssa_platform_matrix.c:503
 * - tests/core/test_ssa_platform_matrix.c:504
 * - tests/core/test_ssa_platform_matrix.c:509
 * - tests/core/test_ssa_platform_matrix.c:511
 * - tests/core/test_ssa_platform_matrix.c:519
 * - tests/core/test_ssa_platform_matrix.c:520
 * - tests/core/test_ssa_platform_matrix.c:522
 * - tests/core/test_ssa_platform_matrix.c:523
 * - tests/core/test_ssa_platform_matrix.c:524
 * - tests/cmake/ssa-tests.cmake:971
 * - tests/cmake/ssa-tests.cmake:972
 * - tests/cmake/ssa-tests.cmake:973
 * - tests/cmake/ssa-tests.cmake:977
 */
