/*
 * SSA release-gate manifest contract.
 *
 * This is intentionally a value-only test contract.  A runner (or a CI
 * adapter) can fill the same structures with observations from the current
 * checkout, then call ZrTests_Ssa_ValidateAcceptance().  The validator never
 * invents missing measurements: an incomplete row keeps the gate open and a
 * malformed row is reported as invalid.
 */
#include "zr_vm_common/zr_common_conf.h"

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#define ZR_SSA_ACCEPTANCE_MAGIC UINT32_C(0x53534152) /* "SSAR" */
#define ZR_SSA_ACCEPTANCE_SCHEMA_VERSION UINT32_C(1)
#define ZR_SSA_ACCEPTANCE_MILESTONE_COUNT UINT32_C(8)
#define ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS UINT32_C(47)
#define ZR_SSA_ACCEPTANCE_CASES_PER_REQUIREMENT UINT32_C(3)
#define ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT UINT32_C(8)

typedef enum EZrSsaAcceptanceStatus {
    ZR_SSA_ACCEPTANCE_STATUS_PLANNED = 0,
    ZR_SSA_ACCEPTANCE_STATUS_IMPLEMENTING = 1,
    ZR_SSA_ACCEPTANCE_STATUS_FOCUSED_PASSED = 2,
    ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED = 3,
    ZR_SSA_ACCEPTANCE_STATUS_BLOCKED = 4
} EZrSsaAcceptanceStatus;

typedef enum EZrSsaAcceptanceGate {
    ZR_SSA_ACCEPTANCE_GATE_OPEN = 0,
    ZR_SSA_ACCEPTANCE_GATE_ACCEPTED = 1,
    ZR_SSA_ACCEPTANCE_GATE_INVALID = 2
} EZrSsaAcceptanceGate;

typedef enum EZrSsaAcceptanceFailure {
    ZR_SSA_ACCEPTANCE_FAILURE_NONE = 0,
    ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST = 1,
    ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_METADATA = 2,
    ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_CASE = 3,
    ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_UNBOUND = 4,
    ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_INVALID = 5,
    ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_OPEN = 6,
    ZR_SSA_ACCEPTANCE_FAILURE_COVERAGE_INCOMPLETE = 7,
    ZR_SSA_ACCEPTANCE_FAILURE_MILESTONE_OPEN = 8,
    ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_UNAVAILABLE = 9,
    ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_INVALID = 10,
    ZR_SSA_ACCEPTANCE_FAILURE_LEGACY_CONSUMER = 11
} EZrSsaAcceptanceFailure;

typedef enum EZrSsaAcceptanceCoverageKind {
    ZR_SSA_ACCEPTANCE_COVERAGE_SEMANTIC = 0,
    ZR_SSA_ACCEPTANCE_COVERAGE_BACKEND = 1,
    ZR_SSA_ACCEPTANCE_COVERAGE_PLATFORM = 2,
    ZR_SSA_ACCEPTANCE_COVERAGE_SANITIZER = 3,
    ZR_SSA_ACCEPTANCE_COVERAGE_ARTIFACT = 4,
    ZR_SSA_ACCEPTANCE_COVERAGE_PERFORMANCE = 5,
    ZR_SSA_ACCEPTANCE_COVERAGE_LEGACY = 6,
    ZR_SSA_ACCEPTANCE_COVERAGE_DOCUMENTATION = 7
} EZrSsaAcceptanceCoverageKind;

typedef struct SZrSsaAcceptanceEvidence {
    const char *revision;
    const char *dirtyDigest;
    const char *environment;
    TZrBool executed;
    TZrBool passed;
    TZrBool unavailable;
    TZrUInt32 expectedVariants;
    TZrUInt32 executedVariants;
    TZrUInt32 failedVariants;
} SZrSsaAcceptanceEvidence;

typedef struct SZrSsaAcceptanceRequirement {
    const char *id;
    const char *domain;
    const char *owner;
    const char *positiveCase;
    const char *boundaryCase;
    const char *negativeCase;
    TZrUInt32 milestone;
    EZrSsaAcceptanceStatus status;
    SZrSsaAcceptanceEvidence evidence;
} SZrSsaAcceptanceRequirement;

typedef struct SZrSsaAcceptanceCoverage {
    EZrSsaAcceptanceCoverageKind kind;
    const char *name;
    const char *revision;
    const char *dirtyDigest;
    const char *environment;
    TZrUInt32 expectedVariants;
    TZrUInt32 executedVariants;
    TZrUInt32 passedVariants;
    TZrUInt32 failedVariants;
    TZrUInt32 unavailableVariants;
} SZrSsaAcceptanceCoverage;

typedef struct SZrSsaAcceptanceMilestone {
    TZrUInt32 milestone;
    EZrSsaAcceptanceStatus status;
    TZrBool prerequisitesAccepted;
    TZrBool allInScopeRequirementsPassed;
    TZrBool backendCoverageComplete;
    TZrBool platformCoverageComplete;
    TZrBool sanitizerCoverageComplete;
    const char *openReason;
} SZrSsaAcceptanceMilestone;

typedef struct SZrSsaAcceptancePerformanceSample {
    const char *workload;
    const char *revision;
    const char *dirtyDigest;
    const char *environment;
    TZrBool valid;
    TZrBool checksumMatch;
    TZrBool environmentMatch;
    TZrUInt32 sampleCount;
    TZrUInt32 coefficientVariationPermille;
    TZrFloat64 baselineMilliseconds;
    TZrFloat64 candidateMilliseconds;
} SZrSsaAcceptancePerformanceSample;

typedef struct SZrSsaAcceptanceLegacyInventory {
    TZrBool inventoryComplete;
    TZrBool removalClaimed;
    TZrUInt32 productionConsumers;
} SZrSsaAcceptanceLegacyInventory;

typedef struct SZrSsaAcceptanceManifest {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    const char *revision;
    const char *dirtyDigest;
    const char *environment;
    const SZrSsaAcceptanceRequirement *requirements;
    TZrUInt32 requirementCount;
    const SZrSsaAcceptanceCoverage *coverage;
    TZrUInt32 coverageCount;
    const SZrSsaAcceptanceMilestone *milestones;
    TZrUInt32 milestoneCount;
    const SZrSsaAcceptancePerformanceSample *performance;
    TZrUInt32 performanceCount;
    TZrBool performanceClaimed;
    SZrSsaAcceptanceLegacyInventory legacy;
} SZrSsaAcceptanceManifest;

typedef struct SZrSsaAcceptanceResult {
    EZrSsaAcceptanceGate gate;
    EZrSsaAcceptanceFailure failure;
    TZrUInt32 failureIndex;
    TZrUInt32 openRequirementCount;
    TZrUInt32 incompleteCoverageCount;
    TZrBool performanceClaimValid;
    TZrBool legacyClear;
} SZrSsaAcceptanceResult;

/* The public test-contract entry described by 11.03. */
TZrBool ZrTests_Ssa_ValidateAcceptance(
        const SZrSsaAcceptanceManifest *manifest,
        SZrSsaAcceptanceResult *result);

typedef struct SZrSsaRequirementDefinition {
    const char *id;
    const char *domain;
    const char *owner;
    const char *positiveCase;
    const char *boundaryCase;
    const char *negativeCase;
    TZrUInt32 milestone;
} SZrSsaRequirementDefinition;

/*
 * Keep this table in lock-step with docs/plans/ssa.  It is deliberately
 * explicit instead of deriving IDs from directory enumeration, so a renamed
 * or accidentally omitted leaf makes the focused gate fail review.
 */
#define ZR_SSA_REQUIREMENT(id_, domain_, owner_, positive_, boundary_, negative_, milestone_) \
    { id_, domain_, owner_, positive_, boundary_, negative_, milestone_ }

static const SZrSsaRequirementDefinition g_requirementDefinitions[
        ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS] = {
    ZR_SSA_REQUIREMENT("00.01", "measurement", "00-measurement-contracts/01-baseline-metrics.md",
                       "baseline_sample_with_checksum", "missing_pmu_is_unavailable",
                       "checksum_mismatch_rejected", 0u),
    ZR_SSA_REQUIREMENT("00.02", "measurement", "00-measurement-contracts/02-contract-freeze.md",
                       "versioned_contract_accepted", "unknown_field_rejected",
                       "abi_version_mismatch_rejected", 0u),
    ZR_SSA_REQUIREMENT("00.03", "measurement", "00-measurement-contracts/03-differential-harness.md",
                       "matching_semantic_events", "zero_ctest_selection_fails",
                       "drop_order_mismatch_rejected", 0u),

    ZR_SSA_REQUIREMENT("01.01", "execir", "01-execir-ssa/01-core-model.md",
                       "typed_execir_module", "empty_block_is_explicit", "malformed_opcode_rejected", 1u),
    ZR_SSA_REQUIREMENT("01.02", "execir", "01-execir-ssa/02-ssa-construction.md",
                       "canonical_ssa_with_phi", "unreachable_block_retained_for_diagnostic",
                       "duplicate_value_definition_rejected", 1u),
    ZR_SSA_REQUIREMENT("01.03", "execir", "01-execir-ssa/03-effects-verifier.md",
                       "ordered_effect_tokens", "empty_effect_range_is_checked",
                       "effect_reordering_rejected", 1u),
    ZR_SSA_REQUIREMENT("01.04", "execir", "01-execir-ssa/04-state-maps.md",
                       "logical_state_materialized", "partial_state_map_is_visible",
                       "state_slot_alias_rejected", 1u),
    ZR_SSA_REQUIREMENT("01.05", "execir", "01-execir-ssa/05-oracle-projections.md",
                       "oracle_execbc_aot_projection", "unsupported_projection_is_reported",
                       "projection_event_mismatch_rejected", 1u),

    ZR_SSA_REQUIREMENT("02.01", "optimization", "02-automatic-optimization/01-pass-manager-scalar.md",
                       "ordered_scalar_passes", "pass_budget_zero_is_noop",
                       "verifier_failure_rolls_back", 6u),
    ZR_SSA_REQUIREMENT("02.02", "optimization", "02-automatic-optimization/02-gvn-range.md",
                       "range_guard_elision", "unknown_range_keeps_check",
                       "stale_generation_rejected", 6u),
    ZR_SSA_REQUIREMENT("02.03", "optimization", "02-automatic-optimization/03-escape-ownership.md",
                       "nonescaping_allocation_plan", "unknown_escape_is_conservative",
                       "borrowed_value_escape_rejected", 6u),
    ZR_SSA_REQUIREMENT("02.04", "optimization", "02-automatic-optimization/04-interprocedural-inlining.md",
                       "guarded_inline_summary", "recursive_call_not_inlined",
                       "signature_or_effect_mismatch_rejected", 6u),
    ZR_SSA_REQUIREMENT("02.05", "optimization", "02-automatic-optimization/05-loops-specialization.md",
                       "bounded_loop_specialization", "unknown_trip_count_keeps_generic_loop",
                       "budget_or_guard_failure_falls_back", 6u),

    ZR_SSA_REQUIREMENT("03.01", "binding", "03-interpreter-binding/01-dispatch-boundaries.md",
                       "central_execution_boundary", "cold_boundary_is_explicit",
                       "invalid_boundary_context_rejected", 3u),
    ZR_SSA_REQUIREMENT("03.02", "binding", "03-interpreter-binding/02-static-binding-facts.md",
                       "token_signature_layout_fact", "missing_fact_stays_dynamic",
                       "ambiguous_binding_rejected", 3u),
    ZR_SSA_REQUIREMENT("03.03", "binding", "03-interpreter-binding/03-guarded-caches.md",
                       "generation_guarded_cache", "cache_miss_enters_cold_lane",
                       "stale_generation_not_reused", 3u),
    ZR_SSA_REQUIREMENT("03.04", "binding", "03-interpreter-binding/04-generated-fusion.md",
                       "generated_fusion_pattern", "partial_pattern_keeps_scalar_path",
                       "unknown_generated_opcode_rejected", 3u),

    ZR_SSA_REQUIREMENT("04.01", "frame-native", "04-frame-native/01-frame-layout.md",
                       "packed_frame_layout", "alignment_padding_is_accounted",
                       "overlapping_slot_rejected", 2u),
    ZR_SSA_REQUIREMENT("04.02", "frame-native", "04-frame-native/02-call-return-tail.md",
                       "return_and_tail_transfer", "zero_return_values_are_explicit",
                       "invalid_tail_target_rejected", 2u),
    ZR_SSA_REQUIREMENT("04.03", "frame-native", "04-frame-native/03-native-abi.md",
                       "typed_native_abi", "copy_marshal_on_layout_mismatch",
                       "callback_or_signature_mismatch_rejected", 2u),
    ZR_SSA_REQUIREMENT("04.04", "frame-native", "04-frame-native/04-roots-observation.md",
                       "precise_frame_roots", "empty_root_set_is_recorded",
                       "unmapped_live_root_rejected", 2u),

    ZR_SSA_REQUIREMENT("05.01", "layout", "05-data-layout/01-objects-layout-maps.md",
                       "object_layout_map", "private_unknown_shape_stays_generic",
                       "public_layout_change_rejected", 2u),
    ZR_SSA_REQUIREMENT("05.02", "layout", "05-data-layout/02-arrays-slices.md",
                       "checked_contiguous_view", "empty_slice_is_valid",
                       "overflow_or_bounds_violation_rejected", 2u),
    ZR_SSA_REQUIREMENT("05.03", "layout", "05-data-layout/03-maps-strings.md",
                       "map_string_storage_contract", "missing_hash_is_conservative",
                       "invalid_storage_kind_rejected", 2u),
    ZR_SSA_REQUIREMENT("05.04", "layout", "05-data-layout/04-aggregate-soa.md",
                       "aggregate_sroa_soa_plan", "unknown_alias_keeps_aos",
                       "public_or_ffi_layout_rewrite_rejected", 2u),

    ZR_SSA_REQUIREMENT("06.01", "gc-domain", "06-gc-domain/01-young-allocation.md",
                       "young_tlab_allocation", "tlab_exhaustion_refills_or_safepoints",
                       "invalid_remembered_set_entry_rejected", 2u),
    ZR_SSA_REQUIREMENT("06.02", "gc-domain", "06-gc-domain/02-major-budget.md",
                       "budgeted_major_slice", "budget_hit_preserves_cursor",
                       "overflow_or_unbounded_pause_rejected", 2u),
    ZR_SSA_REQUIREMENT("06.03", "gc-domain", "06-gc-domain/03-domain-sharing.md",
                       "send_sync_shared_value", "immutable_value_crosses_worker",
                       "borrowed_or_affine_handle_rejected", 2u),
    ZR_SSA_REQUIREMENT("06.04", "gc-domain", "06-gc-domain/04-cross-domain-clone.md",
                       "transactional_cross_domain_clone", "cycle_and_alias_preserved",
                       "quota_or_generation_mismatch_aborts", 2u),
    ZR_SSA_REQUIREMENT("06.05", "gc-domain", "06-gc-domain/05-async-frame-budget.md",
                       "async_frame_checkpoint", "cancelled_checkpoint_unwinds",
                       "borrowed_value_across_await_rejected", 2u),

    ZR_SSA_REQUIREMENT("07.01", "aot", "07-aot-backends/01-aotir-contract.md",
                       "shared_aotir_contract", "unsupported_operation_is_explicit",
                       "pointer_or_contract_drift_rejected", 4u),
    ZR_SSA_REQUIREMENT("07.02", "aot", "07-aot-backends/02-c-llvm-lowering.md",
                       "c_llvm_lowering_parity", "unsupported_lowering_keeps_fallback",
                       "event_or_exception_mismatch_rejected", 4u),
    ZR_SSA_REQUIREMENT("07.03", "aot", "07-aot-backends/03-generics-lto-pgo.md",
                       "generic_release_policy", "stale_profile_is_ignored",
                       "capability_or_abi_mismatch_rejected", 4u),
    ZR_SSA_REQUIREMENT("07.04", "aot", "07-aot-backends/04-aot-runner-coverage.md",
                       "actual_backend_coverage", "zero_denominator_is_unavailable",
                       "fallback_must_not_be_labeled_aot", 4u),

    ZR_SSA_REQUIREMENT("08.01", "artifact", "08-artifact-hotpatch/01-schema-relocation.md",
                       "pointer_free_artifact", "unknown_optional_section_is_skipped",
                       "raw_process_address_rejected", 5u),
    ZR_SSA_REQUIREMENT("08.02", "artifact", "08-artifact-hotpatch/02-capability-validation.md",
                       "capability_manifest_intersection", "empty_capability_set_is_valid",
                       "capability_escalation_rejected", 5u),
    ZR_SSA_REQUIREMENT("08.03", "artifact", "08-artifact-hotpatch/03-generation-publication.md",
                       "atomic_generation_publish", "old_frame_keeps_old_generation",
                       "stale_binding_returns_link_error", 5u),
    ZR_SSA_REQUIREMENT("08.04", "artifact", "08-artifact-hotpatch/04-rollback-restricted.md",
                       "restricted_rollback", "repeated_rollback_is_idempotent",
                       "new_import_or_layout_change_rejected", 5u),

    ZR_SSA_REQUIREMENT("09.01", "simd", "09-language-simd/01-inferred-protocols.md",
                       "inferred_optimization_protocol", "unknown_fact_is_conservative",
                       "invalid_protocol_summary_rejected", 7u),
    ZR_SSA_REQUIREMENT("09.02", "simd", "09-language-simd/02-numeric-vector-ir.md",
                       "strict_numeric_vector_ir", "fast_math_requires_project_permission",
                       "semantic_numeric_drift_rejected", 7u),
    ZR_SSA_REQUIREMENT("09.03", "simd", "09-language-simd/03-batch-vectorization.md",
                       "ordered_batch_vector", "scalar_fallback_is_visible",
                       "unordered_reduction_without_permission_rejected", 7u),

    ZR_SSA_REQUIREMENT("10.01", "platform", "10-jit-platforms/01-backend-service.md",
                       "async_backend_service", "pending_compile_is_not_ready",
                       "active_lease_shutdown_rejected", 7u),
    ZR_SSA_REQUIREMENT("10.02", "platform", "10-jit-platforms/02-host-baseline-jit.md",
                       "host_baseline_jit_lifecycle", "unsupported_architecture_falls_back",
                       "jit_code_persistence_rejected", 7u),
    ZR_SSA_REQUIREMENT("10.03", "platform", "10-jit-platforms/03-platform-matrix.md",
                       "platform_capability_matrix", "cross_compile_is_not_execution",
                       "mobile_or_wasm_jit_rejected", 7u),

    ZR_SSA_REQUIREMENT("11.01", "tooling", "11-tooling-acceptance/01-build-profiles.md",
                       "reproducible_profile_and_cache", "cache_miss_rebuilds",
                       "conflicting_profile_flags_rejected", 0u),
    ZR_SSA_REQUIREMENT("11.02", "tooling", "11-tooling-acceptance/02-optimization-remarks.md",
                       "source_located_optimization_remark", "missing_profile_is_estimated",
                       "stale_document_remark_discarded", 6u),
    ZR_SSA_REQUIREMENT("11.03", "tooling", "11-tooling-acceptance/03-release-acceptance.md",
                       "current_revision_release_manifest", "focused_only_keeps_gate_open",
                       "legacy_consumer_or_missing_backend_blocks", 7u)
};

#undef ZR_SSA_REQUIREMENT

static TZrBool zr_ssa_acceptance_nonempty(const char *text) {
    return (text != ZR_NULL && text[0] != '\0') ? ZR_TRUE : ZR_FALSE;
}

static TZrBool zr_ssa_acceptance_equal(const char *left, const char *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return (left == right) ? ZR_TRUE : ZR_FALSE;
    }
    return strcmp(left, right) == 0 ? ZR_TRUE : ZR_FALSE;
}

static void zr_ssa_acceptance_result_init(SZrSsaAcceptanceResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->gate = ZR_SSA_ACCEPTANCE_GATE_INVALID;
    result->failure = ZR_SSA_ACCEPTANCE_FAILURE_NONE;
    result->failureIndex = UINT32_MAX;
}

static void zr_ssa_acceptance_fail(
        SZrSsaAcceptanceResult *result,
        EZrSsaAcceptanceFailure failure,
        TZrUInt32 index,
        TZrBool malformed) {
    if (result == ZR_NULL || result->failure != ZR_SSA_ACCEPTANCE_FAILURE_NONE) {
        return;
    }
    result->failure = failure;
    result->failureIndex = index;
    result->gate = malformed ? ZR_SSA_ACCEPTANCE_GATE_INVALID
                             : ZR_SSA_ACCEPTANCE_GATE_OPEN;
}

static TZrBool zr_ssa_acceptance_evidence_shape_is_valid(
        const SZrSsaAcceptanceEvidence *evidence) {
    if (evidence == ZR_NULL || evidence->expectedVariants == 0u ||
        evidence->executedVariants > evidence->expectedVariants ||
        evidence->failedVariants > evidence->executedVariants) {
        return ZR_FALSE;
    }
    if (evidence->unavailable != ZR_FALSE &&
        (evidence->executed != ZR_FALSE || evidence->passed != ZR_FALSE ||
         evidence->executedVariants != 0u || evidence->failedVariants != 0u)) {
        return ZR_FALSE;
    }
    if (evidence->passed != ZR_FALSE &&
        (evidence->executed == ZR_FALSE || evidence->unavailable != ZR_FALSE ||
         evidence->failedVariants != 0u ||
         evidence->executedVariants != evidence->expectedVariants)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_ssa_acceptance_requirement_is_accepted(
        const SZrSsaAcceptanceRequirement *requirement) {
    return requirement != ZR_NULL &&
           requirement->status == ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED &&
           requirement->evidence.passed != ZR_FALSE &&
           requirement->evidence.failedVariants == 0u &&
           requirement->evidence.executedVariants ==
                   requirement->evidence.expectedVariants;
}

static TZrBool zr_ssa_acceptance_coverage_is_complete(
        const SZrSsaAcceptanceCoverage *coverage,
        const SZrSsaAcceptanceManifest *manifest) {
    if (coverage == ZR_NULL || coverage->expectedVariants == 0u ||
        manifest == ZR_NULL ||
        !zr_ssa_acceptance_equal(coverage->revision, manifest->revision) ||
        !zr_ssa_acceptance_equal(coverage->dirtyDigest, manifest->dirtyDigest) ||
        !zr_ssa_acceptance_equal(coverage->environment, manifest->environment) ||
        !zr_ssa_acceptance_nonempty(coverage->revision) ||
        !zr_ssa_acceptance_nonempty(coverage->dirtyDigest) ||
        !zr_ssa_acceptance_nonempty(coverage->environment) ||
        coverage->executedVariants > coverage->expectedVariants ||
        coverage->passedVariants > coverage->executedVariants ||
        coverage->failedVariants > coverage->executedVariants ||
        coverage->unavailableVariants > coverage->executedVariants) {
        return ZR_FALSE;
    }
    if (coverage->passedVariants + coverage->failedVariants +
                coverage->unavailableVariants != coverage->executedVariants) {
        return ZR_FALSE;
    }
    return coverage->executedVariants == coverage->expectedVariants &&
           coverage->passedVariants == coverage->expectedVariants &&
           coverage->failedVariants == 0u &&
           coverage->unavailableVariants == 0u;
}

static TZrBool zr_ssa_acceptance_performance_sample_is_valid(
        const SZrSsaAcceptancePerformanceSample *sample,
        const SZrSsaAcceptanceManifest *manifest) {
    if (sample == ZR_NULL || manifest == ZR_NULL ||
        !zr_ssa_acceptance_nonempty(sample->workload) ||
        !zr_ssa_acceptance_equal(sample->revision, manifest->revision) ||
        !zr_ssa_acceptance_equal(sample->dirtyDigest, manifest->dirtyDigest) ||
        !zr_ssa_acceptance_equal(sample->environment, manifest->environment) ||
        !zr_ssa_acceptance_nonempty(sample->revision) ||
        !zr_ssa_acceptance_nonempty(sample->dirtyDigest) ||
        !zr_ssa_acceptance_nonempty(sample->environment) ||
        sample->valid == ZR_FALSE || sample->checksumMatch == ZR_FALSE ||
        sample->environmentMatch == ZR_FALSE || sample->sampleCount == 0u ||
        !(sample->baselineMilliseconds > 0.0) ||
        !(sample->candidateMilliseconds > 0.0) ||
        sample->baselineMilliseconds >= DBL_MAX ||
        sample->candidateMilliseconds >= DBL_MAX ||
        sample->coefficientVariationPermille > 1000u) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_ssa_acceptance_performance_claim_is_valid(
        const SZrSsaAcceptanceManifest *manifest) {
    TZrUInt32 index;
    TZrBool anySample = ZR_FALSE;

    if (manifest == ZR_NULL || manifest->performanceClaimed == ZR_FALSE ||
        manifest->performance == ZR_NULL || manifest->performanceCount == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < manifest->performanceCount; ++index) {
        const SZrSsaAcceptancePerformanceSample *sample =
                &manifest->performance[index];
        if (!zr_ssa_acceptance_performance_sample_is_valid(sample, manifest)) {
            return ZR_FALSE;
        }
        /* A 3% claim is a threshold gate, not a promise based on one noisy run. */
        if (sample->candidateMilliseconds / sample->baselineMilliseconds > 0.97) {
            return ZR_FALSE;
        }
        anySample = ZR_TRUE;
    }
    return anySample;
}

TZrBool ZrTests_Ssa_ValidateAcceptance(
        const SZrSsaAcceptanceManifest *manifest,
        SZrSsaAcceptanceResult *result) {
    TZrUInt32 index;
    TZrBool malformed = ZR_FALSE;
    TZrBool allRequirementsAccepted = ZR_TRUE;
    TZrBool allMilestonesAccepted = ZR_TRUE;
    TZrBool seenCoverage[ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT];
    TZrBool seenMilestone[ZR_SSA_ACCEPTANCE_MILESTONE_COUNT];

    zr_ssa_acceptance_result_init(result);
    if (manifest == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    result->gate = ZR_SSA_ACCEPTANCE_GATE_OPEN;
    memset(seenCoverage, 0, sizeof(seenCoverage));
    memset(seenMilestone, 0, sizeof(seenMilestone));

    if (manifest->magic != ZR_SSA_ACCEPTANCE_MAGIC ||
        manifest->schemaVersion != ZR_SSA_ACCEPTANCE_SCHEMA_VERSION ||
        !zr_ssa_acceptance_nonempty(manifest->revision) ||
        !zr_ssa_acceptance_nonempty(manifest->dirtyDigest) ||
        !zr_ssa_acceptance_nonempty(manifest->environment) ||
        manifest->requirements == ZR_NULL ||
        manifest->requirementCount != ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS ||
        manifest->coverage == ZR_NULL ||
        manifest->coverageCount != ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT ||
        manifest->milestones == ZR_NULL ||
        manifest->milestoneCount != ZR_SSA_ACCEPTANCE_MILESTONE_COUNT) {
        zr_ssa_acceptance_fail(result, ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST,
                               UINT32_MAX, ZR_TRUE);
        return ZR_FALSE;
    }

    for (index = 0u; index < manifest->requirementCount; ++index) {
        const SZrSsaAcceptanceRequirement *requirement =
                &manifest->requirements[index];
        TZrUInt32 other;

        if (!zr_ssa_acceptance_nonempty(requirement->id) ||
            !zr_ssa_acceptance_nonempty(requirement->domain) ||
            !zr_ssa_acceptance_nonempty(requirement->owner)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_METADATA,
                                   index, ZR_TRUE);
        }
        if (!zr_ssa_acceptance_nonempty(requirement->positiveCase) ||
            !zr_ssa_acceptance_nonempty(requirement->boundaryCase) ||
            !zr_ssa_acceptance_nonempty(requirement->negativeCase)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_CASE,
                                   index, ZR_TRUE);
        }
        if (requirement->milestone >= ZR_SSA_ACCEPTANCE_MILESTONE_COUNT) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_METADATA,
                                   index, ZR_TRUE);
        }
        for (other = 0u; other < index; ++other) {
            if (zr_ssa_acceptance_equal(requirement->id,
                                        manifest->requirements[other].id)) {
                malformed = ZR_TRUE;
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_METADATA,
                                       index, ZR_TRUE);
                break;
            }
        }

        if (!zr_ssa_acceptance_evidence_shape_is_valid(&requirement->evidence)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_INVALID,
                                   index, ZR_TRUE);
        } else if (!zr_ssa_acceptance_equal(requirement->evidence.revision,
                                            manifest->revision) ||
                   !zr_ssa_acceptance_equal(requirement->evidence.dirtyDigest,
                                            manifest->dirtyDigest) ||
                   !zr_ssa_acceptance_equal(requirement->evidence.environment,
                                            manifest->environment) ||
                   !zr_ssa_acceptance_nonempty(requirement->evidence.revision) ||
                   !zr_ssa_acceptance_nonempty(requirement->evidence.dirtyDigest) ||
                   !zr_ssa_acceptance_nonempty(requirement->evidence.environment)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_UNBOUND,
                                   index, ZR_TRUE);
        }

        if (!zr_ssa_acceptance_requirement_is_accepted(requirement)) {
            allRequirementsAccepted = ZR_FALSE;
            ++result->openRequirementCount;
            if (requirement->status == ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED &&
                !malformed) {
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_INVALID,
                                       index, ZR_FALSE);
            } else if (requirement->status != ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED &&
                       result->failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE) {
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_OPEN,
                                       index, ZR_FALSE);
            }
        }
    }

    for (index = 0u; index < manifest->coverageCount; ++index) {
        const SZrSsaAcceptanceCoverage *coverage = &manifest->coverage[index];
        if (coverage->kind >= ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT ||
            seenCoverage[coverage->kind] != ZR_FALSE ||
            !zr_ssa_acceptance_nonempty(coverage->name)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST,
                                   index, ZR_TRUE);
            continue;
        }
        seenCoverage[coverage->kind] = ZR_TRUE;
        if (!zr_ssa_acceptance_coverage_is_complete(coverage, manifest)) {
            ++result->incompleteCoverageCount;
            if (result->failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE) {
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_COVERAGE_INCOMPLETE,
                                       index, ZR_FALSE);
            }
        }
    }
    for (index = 0u; index < ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT; ++index) {
        if (seenCoverage[index] == ZR_FALSE) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST,
                                   index, ZR_TRUE);
        }
    }

    for (index = 0u; index < manifest->milestoneCount; ++index) {
        const SZrSsaAcceptanceMilestone *milestone = &manifest->milestones[index];
        TZrUInt32 requirementIndex;
        TZrBool computedAll = ZR_TRUE;

        if (milestone->milestone >= ZR_SSA_ACCEPTANCE_MILESTONE_COUNT ||
            seenMilestone[milestone->milestone] != ZR_FALSE) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST,
                                   index, ZR_TRUE);
            continue;
        }
        seenMilestone[milestone->milestone] = ZR_TRUE;
        for (requirementIndex = 0u;
             requirementIndex < manifest->requirementCount;
             ++requirementIndex) {
            const SZrSsaAcceptanceRequirement *requirement =
                    &manifest->requirements[requirementIndex];
            if (requirement->milestone == milestone->milestone &&
                !zr_ssa_acceptance_requirement_is_accepted(requirement)) {
                computedAll = ZR_FALSE;
                break;
            }
        }
        if ((milestone->allInScopeRequirementsPassed != ZR_FALSE) !=
            (computedAll != ZR_FALSE)) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_MILESTONE_OPEN,
                                   index, ZR_TRUE);
        }
        if (milestone->status == ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED &&
            (computedAll == ZR_FALSE || milestone->prerequisitesAccepted == ZR_FALSE ||
             milestone->backendCoverageComplete == ZR_FALSE ||
             milestone->platformCoverageComplete == ZR_FALSE ||
             milestone->sanitizerCoverageComplete == ZR_FALSE)) {
            allMilestonesAccepted = ZR_FALSE;
            if (result->failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE) {
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_MILESTONE_OPEN,
                                       index, ZR_FALSE);
            }
        } else if (milestone->status != ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED) {
            allMilestonesAccepted = ZR_FALSE;
            if (result->failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE) {
                zr_ssa_acceptance_fail(result,
                                       ZR_SSA_ACCEPTANCE_FAILURE_MILESTONE_OPEN,
                                       index, ZR_FALSE);
            }
        }
    }
    for (index = 0u; index < ZR_SSA_ACCEPTANCE_MILESTONE_COUNT; ++index) {
        if (seenMilestone[index] == ZR_FALSE) {
            malformed = ZR_TRUE;
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST,
                                   index, ZR_TRUE);
        }
    }

    result->performanceClaimValid =
            manifest->performanceClaimed == ZR_FALSE
                    ? ZR_TRUE
                    : zr_ssa_acceptance_performance_claim_is_valid(manifest);
    if (manifest->performanceClaimed != ZR_FALSE &&
        result->performanceClaimValid == ZR_FALSE) {
        if (manifest->performance == ZR_NULL || manifest->performanceCount == 0u) {
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_UNAVAILABLE,
                                   UINT32_MAX, ZR_FALSE);
        } else {
            zr_ssa_acceptance_fail(result,
                                   ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_INVALID,
                                   UINT32_MAX, ZR_FALSE);
        }
    }

    result->legacyClear = manifest->legacy.inventoryComplete != ZR_FALSE &&
                          manifest->legacy.productionConsumers == 0u;
    if (result->legacyClear == ZR_FALSE ||
        (manifest->legacy.removalClaimed != ZR_FALSE &&
         manifest->legacy.productionConsumers != 0u)) {
        zr_ssa_acceptance_fail(result,
                               ZR_SSA_ACCEPTANCE_FAILURE_LEGACY_CONSUMER,
                               manifest->legacy.productionConsumers, ZR_FALSE);
    }

    if (malformed) {
        result->gate = ZR_SSA_ACCEPTANCE_GATE_INVALID;
    }
    if (result->failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE &&
        allRequirementsAccepted != ZR_FALSE &&
        allMilestonesAccepted != ZR_FALSE &&
        result->incompleteCoverageCount == 0u &&
        result->performanceClaimValid != ZR_FALSE &&
        result->legacyClear != ZR_FALSE) {
        result->gate = ZR_SSA_ACCEPTANCE_GATE_ACCEPTED;
    } else if (result->gate != ZR_SSA_ACCEPTANCE_GATE_INVALID) {
        result->gate = ZR_SSA_ACCEPTANCE_GATE_OPEN;
    }
    return result->gate == ZR_SSA_ACCEPTANCE_GATE_ACCEPTED ? ZR_TRUE : ZR_FALSE;
}

typedef struct SZrSsaAcceptanceFixture {
    SZrSsaAcceptanceRequirement requirements[ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS];
    SZrSsaAcceptanceCoverage coverage[ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT];
    SZrSsaAcceptanceMilestone milestones[ZR_SSA_ACCEPTANCE_MILESTONE_COUNT];
    SZrSsaAcceptancePerformanceSample performance[1];
    SZrSsaAcceptanceManifest manifest;
} SZrSsaAcceptanceFixture;

static const char g_fixtureRevision[] = "fixture-revision-20260914";
static const char g_fixtureDirtyDigest[] = "fixture-dirty-digest-ssa";
static const char g_fixtureEnvironment[] = "fixture-wsl-gcc-debug";

static void zr_ssa_acceptance_fixture_init(SZrSsaAcceptanceFixture *fixture) {
    TZrUInt32 index;
    static const EZrSsaAcceptanceCoverageKind kinds[
            ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT] = {
        ZR_SSA_ACCEPTANCE_COVERAGE_SEMANTIC,
        ZR_SSA_ACCEPTANCE_COVERAGE_BACKEND,
        ZR_SSA_ACCEPTANCE_COVERAGE_PLATFORM,
        ZR_SSA_ACCEPTANCE_COVERAGE_SANITIZER,
        ZR_SSA_ACCEPTANCE_COVERAGE_ARTIFACT,
        ZR_SSA_ACCEPTANCE_COVERAGE_PERFORMANCE,
        ZR_SSA_ACCEPTANCE_COVERAGE_LEGACY,
        ZR_SSA_ACCEPTANCE_COVERAGE_DOCUMENTATION
    };
    static const char *const names[
            ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT] = {
        "semantic-cases", "backends", "platforms", "sanitizers",
        "artifact-hotpatch", "performance-workloads", "legacy-consumers",
        "documentation-api-schema"
    };

    assert(fixture != ZR_NULL);
    memset(fixture, 0, sizeof(*fixture));
    for (index = 0u; index < ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS; ++index) {
        fixture->requirements[index].id = g_requirementDefinitions[index].id;
        fixture->requirements[index].domain = g_requirementDefinitions[index].domain;
        fixture->requirements[index].owner = g_requirementDefinitions[index].owner;
        fixture->requirements[index].positiveCase =
                g_requirementDefinitions[index].positiveCase;
        fixture->requirements[index].boundaryCase =
                g_requirementDefinitions[index].boundaryCase;
        fixture->requirements[index].negativeCase =
                g_requirementDefinitions[index].negativeCase;
        fixture->requirements[index].milestone =
                g_requirementDefinitions[index].milestone;
        fixture->requirements[index].status = ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED;
        fixture->requirements[index].evidence.revision = g_fixtureRevision;
        fixture->requirements[index].evidence.dirtyDigest = g_fixtureDirtyDigest;
        fixture->requirements[index].evidence.environment = g_fixtureEnvironment;
        fixture->requirements[index].evidence.executed = ZR_TRUE;
        fixture->requirements[index].evidence.passed = ZR_TRUE;
        fixture->requirements[index].evidence.unavailable = ZR_FALSE;
        fixture->requirements[index].evidence.expectedVariants =
                ZR_SSA_ACCEPTANCE_CASES_PER_REQUIREMENT;
        fixture->requirements[index].evidence.executedVariants =
                ZR_SSA_ACCEPTANCE_CASES_PER_REQUIREMENT;
    }
    for (index = 0u; index < ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT; ++index) {
        fixture->coverage[index].kind = kinds[index];
        fixture->coverage[index].name = names[index];
        fixture->coverage[index].revision = g_fixtureRevision;
        fixture->coverage[index].dirtyDigest = g_fixtureDirtyDigest;
        fixture->coverage[index].environment = g_fixtureEnvironment;
        fixture->coverage[index].expectedVariants =
                index == ZR_SSA_ACCEPTANCE_COVERAGE_SEMANTIC ? 141u : 4u;
        fixture->coverage[index].executedVariants =
                fixture->coverage[index].expectedVariants;
        fixture->coverage[index].passedVariants =
                fixture->coverage[index].expectedVariants;
    }
    fixture->performance[0].workload = "numeric_loops";
    fixture->performance[0].revision = g_fixtureRevision;
    fixture->performance[0].dirtyDigest = g_fixtureDirtyDigest;
    fixture->performance[0].environment = g_fixtureEnvironment;
    fixture->performance[0].valid = ZR_TRUE;
    fixture->performance[0].checksumMatch = ZR_TRUE;
    fixture->performance[0].environmentMatch = ZR_TRUE;
    fixture->performance[0].sampleCount = 5u;
    fixture->performance[0].coefficientVariationPermille = 20u;
    fixture->performance[0].baselineMilliseconds = 100.0;
    fixture->performance[0].candidateMilliseconds = 96.0;

    for (index = 0u; index < ZR_SSA_ACCEPTANCE_MILESTONE_COUNT; ++index) {
        fixture->milestones[index].milestone = index;
        fixture->milestones[index].status = ZR_SSA_ACCEPTANCE_STATUS_ACCEPTED;
        fixture->milestones[index].prerequisitesAccepted = ZR_TRUE;
        fixture->milestones[index].allInScopeRequirementsPassed = ZR_TRUE;
        fixture->milestones[index].backendCoverageComplete = ZR_TRUE;
        fixture->milestones[index].platformCoverageComplete = ZR_TRUE;
        fixture->milestones[index].sanitizerCoverageComplete = ZR_TRUE;
        fixture->milestones[index].openReason = "";
    }

    fixture->manifest.magic = ZR_SSA_ACCEPTANCE_MAGIC;
    fixture->manifest.schemaVersion = ZR_SSA_ACCEPTANCE_SCHEMA_VERSION;
    fixture->manifest.revision = g_fixtureRevision;
    fixture->manifest.dirtyDigest = g_fixtureDirtyDigest;
    fixture->manifest.environment = g_fixtureEnvironment;
    fixture->manifest.requirements = fixture->requirements;
    fixture->manifest.requirementCount = ZR_SSA_ACCEPTANCE_REQUIRED_REQUIREMENTS;
    fixture->manifest.coverage = fixture->coverage;
    fixture->manifest.coverageCount = ZR_SSA_ACCEPTANCE_COVERAGE_DIMENSION_COUNT;
    fixture->manifest.milestones = fixture->milestones;
    fixture->manifest.milestoneCount = ZR_SSA_ACCEPTANCE_MILESTONE_COUNT;
    fixture->manifest.performance = fixture->performance;
    fixture->manifest.performanceCount = 1u;
    fixture->manifest.performanceClaimed = ZR_FALSE;
    fixture->manifest.legacy.inventoryComplete = ZR_TRUE;
    fixture->manifest.legacy.removalClaimed = ZR_TRUE;
    fixture->manifest.legacy.productionConsumers = 0u;
}

static void test_complete_manifest_is_accepted(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_TRUE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_ACCEPTED);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_NONE);
    assert(result.openRequirementCount == 0u);
    assert(result.incompleteCoverageCount == 0u);
    assert(result.legacyClear == ZR_TRUE);
}

static void test_missing_case_is_a_structural_failure(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.requirements[14].positiveCase = ""; /* static/virtual/interface/accessor row */
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_INVALID);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_CASE);
    assert(result.failureIndex == 14u);
}

static void test_focused_only_status_keeps_milestone_open(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.requirements[0].status = ZR_SSA_ACCEPTANCE_STATUS_FOCUSED_PASSED;
    fixture.milestones[0].status = ZR_SSA_ACCEPTANCE_STATUS_FOCUSED_PASSED;
    fixture.milestones[0].allInScopeRequirementsPassed = ZR_FALSE;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_REQUIREMENT_OPEN);
    assert(result.openRequirementCount == 1u);
}

static void test_sanitizer_failure_cannot_be_hidden_by_smoke(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_SANITIZER].executedVariants = 5u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_SANITIZER].passedVariants = 4u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_SANITIZER].failedVariants = 1u;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_COVERAGE_INCOMPLETE);
    assert(result.incompleteCoverageCount == 1u);
}

static void test_native_coverage_and_platform_gaps_remain_open(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_BACKEND].executedVariants = 3u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_BACKEND].passedVariants = 3u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_PLATFORM].unavailableVariants = 1u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_PLATFORM].executedVariants = 4u;
    fixture.coverage[ZR_SSA_ACCEPTANCE_COVERAGE_PLATFORM].passedVariants = 3u;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_COVERAGE_INCOMPLETE);
    assert(result.incompleteCoverageCount == 2u);
}

static void test_performance_claim_rejects_missing_or_incomparable_samples(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.manifest.performanceClaimed = ZR_TRUE;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_TRUE);
    assert(result.performanceClaimValid == ZR_TRUE);

    fixture.performance[0].candidateMilliseconds = 99.0;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_INVALID);
    assert(result.performanceClaimValid == ZR_FALSE);

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.manifest.performanceClaimed = ZR_TRUE;
    fixture.performance[0].environmentMatch = ZR_FALSE;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_INVALID);
    assert(result.performanceClaimValid == ZR_FALSE);
}

static void test_legacy_decoder_consumer_blocks_removal_claim(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.manifest.legacy.productionConsumers = 1u;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_OPEN);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_LEGACY_CONSUMER);
    assert(result.legacyClear == ZR_FALSE);
}

static void test_revision_binding_and_no_fake_performance_data(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult result;

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.requirements[0].evidence.revision = "old-revision";
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.gate == ZR_SSA_ACCEPTANCE_GATE_INVALID);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_EVIDENCE_UNBOUND);

    zr_ssa_acceptance_fixture_init(&fixture);
    fixture.manifest.performanceClaimed = ZR_TRUE;
    fixture.manifest.performanceCount = 0u;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &result) == ZR_FALSE);
    assert(result.failure == ZR_SSA_ACCEPTANCE_FAILURE_PERFORMANCE_UNAVAILABLE);
}

static void test_null_partial_and_repeated_validation_are_safe(void) {
    SZrSsaAcceptanceFixture fixture;
    SZrSsaAcceptanceResult first;
    SZrSsaAcceptanceResult second;

    assert(ZrTests_Ssa_ValidateAcceptance(ZR_NULL, &first) == ZR_FALSE);
    assert(first.gate == ZR_SSA_ACCEPTANCE_GATE_INVALID);
    assert(ZrTests_Ssa_ValidateAcceptance(ZR_NULL, ZR_NULL) == ZR_FALSE);

    zr_ssa_acceptance_fixture_init(&fixture);
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &first) == ZR_TRUE);
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &second) == ZR_TRUE);
    assert(memcmp(&first, &second, sizeof(first)) == 0);

    fixture.manifest.requirements = ZR_NULL;
    assert(ZrTests_Ssa_ValidateAcceptance(&fixture.manifest, &second) == ZR_FALSE);
    assert(second.gate == ZR_SSA_ACCEPTANCE_GATE_INVALID);
    assert(second.failure == ZR_SSA_ACCEPTANCE_FAILURE_BAD_MANIFEST);
}

int main(void) {
    test_complete_manifest_is_accepted();
    test_missing_case_is_a_structural_failure();
    test_focused_only_status_keeps_milestone_open();
    test_sanitizer_failure_cannot_be_hidden_by_smoke();
    test_native_coverage_and_platform_gaps_remain_open();
    test_performance_claim_rejects_missing_or_incomparable_samples();
    test_legacy_decoder_consumer_blocks_removal_claim();
    test_revision_binding_and_no_fake_performance_data();
    test_null_partial_and_repeated_validation_are_safe();
    puts("ssa release acceptance PASS");
    return 0;
}
