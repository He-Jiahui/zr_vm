# Declarative release-gate denominator for the SSA plan.
#
# This file is data plus a side-effect-free validation function.  It does not
# run tests, infer support from a host, or turn an unavailable backend into a
# pass.  The parent CMake integration may include it and compare the executed
# CTest list with these expected variants.

set(ZR_SSA_COVERAGE_MANIFEST_VERSION 1)
set(ZR_SSA_COVERAGE_REQUIRED_REQUIREMENTS 47)
set(ZR_SSA_COVERAGE_CASES_PER_REQUIREMENT 3)

set(ZR_SSA_COVERAGE_REQUIRED_LEAF_IDS
    "00.01" "00.02" "00.03"
    "01.01" "01.02" "01.03" "01.04" "01.05"
    "02.01" "02.02" "02.03" "02.04" "02.05"
    "03.01" "03.02" "03.03" "03.04"
    "04.01" "04.02" "04.03" "04.04"
    "05.01" "05.02" "05.03" "05.04"
    "06.01" "06.02" "06.03" "06.04" "06.05"
    "07.01" "07.02" "07.03" "07.04"
    "08.01" "08.02" "08.03" "08.04"
    "09.01" "09.02" "09.03"
    "10.01" "10.02" "10.03"
    "11.01" "11.02" "11.03")

# One expected CTest name per leaf.  A missing name is an omission, not an
# unavailable result; CTest integration must fail the release gate explicitly.
set(ZR_SSA_COVERAGE_REQUIRED_CTEST_NAMES
    ssa_baseline_metrics ssa_contract_freeze ssa_differential_harness
    ssa_core_model ssa_construction ssa_effects_verifier ssa_state_maps
    ssa_oracle_projections
    ssa_pass_manager_scalar ssa_gvn_range ssa_escape_ownership
    ssa_interprocedural_inlining ssa_loops_specialization
    ssa_dispatch_boundaries ssa_static_binding_facts ssa_guarded_caches
    ssa_generated_fusion
    ssa_frame_layout ssa_call_return_tail ssa_native_abi ssa_roots_observation
    ssa_objects_layout_maps ssa_arrays_slices ssa_maps_strings ssa_aggregate_soa
    ssa_young_allocation ssa_major_budget ssa_domain_sharing
    ssa_cross_domain_clone ssa_async_frame_budget
    ssa_aotir_contract ssa_c_llvm_lowering ssa_generics_lto_pgo
    ssa_aot_runner_coverage
    ssa_schema_relocation ssa_capability_validation ssa_generation_publication
    ssa_rollback_restricted
    ssa_inferred_protocols ssa_numeric_vector_ir ssa_batch_vectorization
    ssa_backend_service ssa_host_baseline_jit ssa_platform_matrix
    ssa_build_profiles ssa_optimization_remarks ssa_release_acceptance)

# Semantic dimensions are intentionally separate from backend/platform rows.
# A backend may execute a fixture while still leaving a semantic case absent.
set(ZR_SSA_COVERAGE_REQUIRED_SEMANTIC_VARIANTS
    parser_call_forms
    execir_cfg_phi_effects
    runtime_gc_ownership_async_native
    artifact_hotpatch
    tooling_remarks_lsp
    differential_event_order)

set(ZR_SSA_COVERAGE_REQUIRED_BACKEND_VARIANTS
    execbc
    aot_c
    aot_llvm
    host_jit)

set(ZR_SSA_COVERAGE_REQUIRED_PLATFORM_VARIANTS
    wsl_gcc
    wsl_clang
    windows_msvc
    android_aot_or_execbc
    ios_aot_or_restricted_patch
    wasm_execbc_or_aot)

set(ZR_SSA_COVERAGE_REQUIRED_SANITIZER_VARIANTS
    asan
    ubsan
    lsan
    tsan
    valgrind
    helgrind)

set(ZR_SSA_COVERAGE_REQUIRED_ARTIFACT_VARIANTS
    schema_roundtrip
    no_process_addresses
    capability_intersection
    generation_and_rollback)

set(ZR_SSA_COVERAGE_REQUIRED_PERFORMANCE_VARIANTS
    numeric_loops
    dispatch_loops
    call_chain_polymorphic
    mixed_service_loop
    object_field_hot
    array_index_dense
    matrix_add_2d
    map_object_access
    string_build
    gc_fragment_baseline
    gc_fragment_stress
    native_member
    accessor
    async_task
    shared_domain_worker)

set(ZR_SSA_COVERAGE_REQUIRED_LEGACY_VARIANTS
    legacy_semir_projection
    legacy_aot_opcode_decoder)

set(ZR_SSA_COVERAGE_REQUIRED_DOCUMENTATION_VARIANTS
    module_api
    execir_artifact_schema
    cli_lsp_reference
    acceptance_records)

set(ZR_SSA_COVERAGE_PERFORMANCE_MIN_IMPROVEMENT_PERCENT 3)
set(ZR_SSA_COVERAGE_REQUIRED_MILESTONES M0 M1 M2 M3 M4 M5 M6 M7)

function(zr_vm_ssa_coverage_manifest_validate output_variable)
    set(_valid TRUE)

    list(LENGTH ZR_SSA_COVERAGE_REQUIRED_LEAF_IDS _leaf_count)
    if (NOT _leaf_count EQUAL ZR_SSA_COVERAGE_REQUIRED_REQUIREMENTS)
        set(_valid FALSE)
    endif ()

    list(LENGTH ZR_SSA_COVERAGE_REQUIRED_CTEST_NAMES _ctest_count)
    if (NOT _ctest_count EQUAL ZR_SSA_COVERAGE_REQUIRED_REQUIREMENTS)
        set(_valid FALSE)
    endif ()

    list(LENGTH ZR_SSA_COVERAGE_REQUIRED_MILESTONES _milestone_count)
    if (NOT _milestone_count EQUAL 8)
        set(_valid FALSE)
    endif ()

    foreach (_required_list
            ZR_SSA_COVERAGE_REQUIRED_SEMANTIC_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_BACKEND_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_PLATFORM_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_SANITIZER_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_ARTIFACT_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_PERFORMANCE_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_LEGACY_VARIANTS
            ZR_SSA_COVERAGE_REQUIRED_DOCUMENTATION_VARIANTS)
        if (NOT DEFINED ${_required_list} OR "${${_required_list}}" STREQUAL "")
            set(_valid FALSE)
        endif ()
    endforeach ()

    # Duplicate leaf IDs or CTest names would make a green count ambiguous.
    list(REMOVE_DUPLICATES ZR_SSA_COVERAGE_REQUIRED_LEAF_IDS)
    list(REMOVE_DUPLICATES ZR_SSA_COVERAGE_REQUIRED_CTEST_NAMES)
    list(LENGTH ZR_SSA_COVERAGE_REQUIRED_LEAF_IDS _unique_leaf_count)
    list(LENGTH ZR_SSA_COVERAGE_REQUIRED_CTEST_NAMES _unique_ctest_count)
    if (NOT _unique_leaf_count EQUAL ZR_SSA_COVERAGE_REQUIRED_REQUIREMENTS OR
        NOT _unique_ctest_count EQUAL ZR_SSA_COVERAGE_REQUIRED_REQUIREMENTS)
        set(_valid FALSE)
    endif ()

    set(${output_variable} "${_valid}" PARENT_SCOPE)
endfunction()

# Make `cmake -P tests/cmake/ssa-coverage-manifest.cmake` a cheap, deterministic
# declaration check while remaining inert when included by the main project.
if (CMAKE_SCRIPT_MODE_FILE)
    zr_vm_ssa_coverage_manifest_validate(_zr_ssa_manifest_valid)
    if (NOT _zr_ssa_manifest_valid)
        message(FATAL_ERROR "invalid SSA coverage manifest declaration")
    endif ()
endif ()
