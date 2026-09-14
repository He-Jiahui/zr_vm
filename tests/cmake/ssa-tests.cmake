# Central registration point for the SSA plan's focused tests.
# Keep these targets independent from the large legacy test list so an SSA
# milestone can be built and validated without changing existing suites.

if (NOT TARGET zr_vm_ssa_baseline_metrics_test)
    add_executable(
            zr_vm_ssa_baseline_metrics_test
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/test_ssa_baseline_metrics.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_ssa_metrics.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c
    )
    target_include_directories(zr_vm_ssa_baseline_metrics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/performance
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_baseline_metrics_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    if (NOT WIN32)
        target_link_libraries(zr_vm_ssa_baseline_metrics_test PRIVATE m)
    endif ()
    add_test(NAME ssa_baseline_metrics COMMAND zr_vm_ssa_baseline_metrics_test)
    set_tests_properties(ssa_baseline_metrics PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_contract_freeze_test)
    add_executable(
            zr_vm_ssa_contract_freeze_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_contract_freeze.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
    )
    target_include_directories(zr_vm_ssa_contract_freeze_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_contract_freeze_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_contract_freeze COMMAND zr_vm_ssa_contract_freeze_test)
    set_tests_properties(ssa_contract_freeze PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_differential_harness_test)
    add_executable(
            zr_vm_ssa_differential_harness_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_differential_harness.c
            ${CMAKE_SOURCE_DIR}/tests/harness/ssa_differential_support.c
    )
    target_include_directories(zr_vm_ssa_differential_harness_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_differential_harness_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_differential_harness COMMAND zr_vm_ssa_differential_harness_test)
    set_tests_properties(ssa_differential_harness PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_core_model_test)
    add_executable(
            zr_vm_ssa_core_model_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_core_model.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
    )
    target_include_directories(zr_vm_ssa_core_model_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_core_model_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_core_model COMMAND zr_vm_ssa_core_model_test)
    set_tests_properties(ssa_core_model PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_effects_verifier_test)
    add_executable(zr_vm_ssa_effects_verifier_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_effects_verifier.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)
    target_include_directories(zr_vm_ssa_effects_verifier_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_effects_verifier_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_effects_verifier COMMAND zr_vm_ssa_effects_verifier_test)
    set_tests_properties(ssa_effects_verifier PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_construction_test)
    add_executable(zr_vm_ssa_construction_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_construction.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_construction_test)
    target_include_directories(zr_vm_ssa_construction_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    target_compile_definitions(zr_vm_ssa_construction_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_construction_test "zr_unity")
    zr_vm_link_parser_core(zr_vm_ssa_construction_test)
    add_test(NAME ssa_construction COMMAND zr_vm_ssa_construction_test)
    set_tests_properties(ssa_construction PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_state_maps_test)
    add_executable(zr_vm_ssa_state_maps_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_state_maps_test)
    target_include_directories(zr_vm_ssa_state_maps_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_state_maps_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_state_maps_test "zr_unity")
    add_test(NAME ssa_state_maps COMMAND zr_vm_ssa_state_maps_test)
    set_tests_properties(ssa_state_maps PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_oracle_projections_test)
    add_executable(zr_vm_ssa_oracle_projections_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_oracle_projections.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c)
    target_include_directories(zr_vm_ssa_oracle_projections_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_oracle_projections_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_oracle_projections COMMAND zr_vm_ssa_oracle_projections_test)
    set_tests_properties(ssa_oracle_projections PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_pass_manager_scalar_test)
    add_executable(zr_vm_ssa_pass_manager_scalar_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_pass_manager_scalar.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c)
    target_include_directories(zr_vm_ssa_pass_manager_scalar_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_pass_manager_scalar_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_pass_manager_scalar COMMAND zr_vm_ssa_pass_manager_scalar_test)
    set_tests_properties(ssa_pass_manager_scalar PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_gvn_range_test)
    add_executable(zr_vm_ssa_gvn_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_gvn_range.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c)
    target_include_directories(zr_vm_ssa_gvn_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_gvn_range_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_gvn_range COMMAND zr_vm_ssa_gvn_range_test)
    set_tests_properties(ssa_gvn_range PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_escape_ownership_test)
    add_executable(zr_vm_ssa_escape_ownership_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_escape_ownership.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_hash.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_summary.c)
    target_include_directories(zr_vm_ssa_escape_ownership_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_escape_ownership_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_escape_ownership COMMAND zr_vm_ssa_escape_ownership_test)
    set_tests_properties(ssa_escape_ownership PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_interprocedural_inlining_test)
    add_executable(zr_vm_ssa_interprocedural_inlining_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_interprocedural_inlining.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c)
    target_include_directories(zr_vm_ssa_interprocedural_inlining_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_interprocedural_inlining_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_interprocedural_inlining COMMAND zr_vm_ssa_interprocedural_inlining_test)
    set_tests_properties(ssa_interprocedural_inlining PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_dispatch_boundaries_test)
    add_executable(zr_vm_ssa_dispatch_boundaries_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_dispatch_boundaries.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_cold.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_dispatch_boundaries_test)
    target_compile_definitions(zr_vm_ssa_dispatch_boundaries_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    target_include_directories(zr_vm_ssa_dispatch_boundaries_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_link_third_party_for_target(zr_vm_ssa_dispatch_boundaries_test "zr_unity")
    zr_vm_link_core(zr_vm_ssa_dispatch_boundaries_test)
    add_test(NAME ssa_dispatch_boundaries COMMAND zr_vm_ssa_dispatch_boundaries_test)
    set_tests_properties(ssa_dispatch_boundaries PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_static_binding_facts_test)
    add_executable(zr_vm_ssa_static_binding_facts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_static_binding_facts.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_static_binding_facts_test)
    target_compile_definitions(zr_vm_ssa_static_binding_facts_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    target_include_directories(zr_vm_ssa_static_binding_facts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_link_third_party_for_target(zr_vm_ssa_static_binding_facts_test "zr_unity")
    zr_vm_link_core(zr_vm_ssa_static_binding_facts_test)
    add_test(NAME ssa_static_binding_facts COMMAND zr_vm_ssa_static_binding_facts_test)
    set_tests_properties(ssa_static_binding_facts PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_guarded_caches_test)
    add_executable(zr_vm_ssa_guarded_caches_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_guarded_caches.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_guarded_caches_test)
    target_compile_definitions(zr_vm_ssa_guarded_caches_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    target_include_directories(zr_vm_ssa_guarded_caches_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_link_third_party_for_target(zr_vm_ssa_guarded_caches_test "zr_unity")
    zr_vm_link_core(zr_vm_ssa_guarded_caches_test)
    add_test(NAME ssa_guarded_caches COMMAND zr_vm_ssa_guarded_caches_test)
    set_tests_properties(ssa_guarded_caches PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_frame_layout_test)
    add_executable(zr_vm_ssa_frame_layout_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_frame_layout.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c)
    target_include_directories(zr_vm_ssa_frame_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_frame_layout_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_frame_layout COMMAND zr_vm_ssa_frame_layout_test)
    set_tests_properties(ssa_frame_layout PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_native_abi_test)
    add_executable(zr_vm_ssa_native_abi_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_ssa_native_abi.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/native_call_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/native_call_lease.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/native_call_marshalling.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/native_call_callback.c)
    target_include_directories(zr_vm_ssa_native_abi_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_native_abi_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_core(zr_vm_ssa_native_abi_test)
    add_test(NAME ssa_native_abi COMMAND zr_vm_ssa_native_abi_test)
    set_tests_properties(ssa_native_abi PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_aotir_contract_test)
    add_executable(zr_vm_ssa_aotir_contract_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_aotir_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/aot_ir.c)
    target_include_directories(zr_vm_ssa_aotir_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_aotir_contract_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_aotir_contract COMMAND zr_vm_ssa_aotir_contract_test)
    set_tests_properties(ssa_aotir_contract PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_schema_relocation_test)
    add_executable(zr_vm_ssa_schema_relocation_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_ssa_schema_relocation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c)
    target_include_directories(zr_vm_ssa_schema_relocation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_schema_relocation_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_schema_relocation COMMAND zr_vm_ssa_schema_relocation_test)
    set_tests_properties(ssa_schema_relocation PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_capability_validation_test)
    add_executable(zr_vm_ssa_capability_validation_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_ssa_capability_validation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c)
    target_include_directories(zr_vm_ssa_capability_validation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_capability_validation_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_capability_validation COMMAND zr_vm_ssa_capability_validation_test)
    set_tests_properties(ssa_capability_validation PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_maps_strings_test)
    add_executable(zr_vm_ssa_maps_strings_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_maps_strings.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library/container_storage_contract.c)
    target_include_directories(zr_vm_ssa_maps_strings_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_maps_strings_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_maps_strings COMMAND zr_vm_ssa_maps_strings_test)
    set_tests_properties(ssa_maps_strings PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_objects_layout_maps_test)
    add_executable(zr_vm_ssa_objects_layout_maps_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_objects_layout_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/object/object_layout_map.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_layout_visibility.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_objects_layout_maps_test)
    target_compile_definitions(zr_vm_ssa_objects_layout_maps_test PRIVATE
            UNITY_INCLUDE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
    target_include_directories(zr_vm_ssa_objects_layout_maps_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_link_third_party_for_target(zr_vm_ssa_objects_layout_maps_test "zr_unity")
    add_test(NAME ssa_objects_layout_maps COMMAND zr_vm_ssa_objects_layout_maps_test)
    set_tests_properties(ssa_objects_layout_maps PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_arrays_slices_test)
    add_executable(zr_vm_ssa_arrays_slices_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_arrays_slices.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/object/contiguous_view.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c)
    target_include_directories(zr_vm_ssa_arrays_slices_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_arrays_slices_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_arrays_slices COMMAND zr_vm_ssa_arrays_slices_test)
    set_tests_properties(ssa_arrays_slices PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_generation_publication_test)
    add_executable(zr_vm_ssa_generation_publication_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_generation_publication.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c)
    target_include_directories(zr_vm_ssa_generation_publication_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_generation_publication_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_generation_publication COMMAND zr_vm_ssa_generation_publication_test)
    set_tests_properties(ssa_generation_publication PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_rollback_restricted_test)
    add_executable(zr_vm_ssa_rollback_restricted_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_ssa_rollback_restricted.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c)
    target_include_directories(zr_vm_ssa_rollback_restricted_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_rollback_restricted_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_rollback_restricted COMMAND zr_vm_ssa_rollback_restricted_test)
    set_tests_properties(ssa_rollback_restricted PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_major_budget_test)
    add_executable(zr_vm_ssa_major_budget_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_major_budget.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c)
    target_include_directories(zr_vm_ssa_major_budget_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_major_budget_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_major_budget COMMAND zr_vm_ssa_major_budget_test)
    set_tests_properties(ssa_major_budget PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_domain_sharing_test)
    add_executable(zr_vm_ssa_domain_sharing_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_domain_sharing.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_send_sync.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_domain_sharing_test)
    target_compile_definitions(zr_vm_ssa_domain_sharing_test PRIVATE
            UNITY_INCLUDE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
    target_include_directories(zr_vm_ssa_domain_sharing_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_link_third_party_for_target(zr_vm_ssa_domain_sharing_test "zr_unity")
    zr_vm_link_core(zr_vm_ssa_domain_sharing_test)
    add_test(NAME ssa_domain_sharing COMMAND zr_vm_ssa_domain_sharing_test)
    set_tests_properties(ssa_domain_sharing PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_batch_vectorization_test)
    add_executable(zr_vm_ssa_batch_vectorization_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_ssa_batch_vectorization.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/batch_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library/batch_protocol.c)
    target_include_directories(zr_vm_ssa_batch_vectorization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_batch_vectorization_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_batch_vectorization COMMAND zr_vm_ssa_batch_vectorization_test)
    set_tests_properties(ssa_batch_vectorization PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_numeric_vector_ir_test)
    add_executable(zr_vm_ssa_numeric_vector_ir_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_numeric_vector_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c)
    target_include_directories(zr_vm_ssa_numeric_vector_ir_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_numeric_vector_ir_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_numeric_vector_ir COMMAND zr_vm_ssa_numeric_vector_ir_test)
    set_tests_properties(ssa_numeric_vector_ir PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_generics_lto_pgo_test)
    add_executable(zr_vm_ssa_generics_lto_pgo_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_generics_lto_pgo.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c)
    target_include_directories(zr_vm_ssa_generics_lto_pgo_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_generics_lto_pgo_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_generics_lto_pgo COMMAND zr_vm_ssa_generics_lto_pgo_test)
    set_tests_properties(ssa_generics_lto_pgo PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_host_baseline_jit_test)
    add_executable(zr_vm_ssa_host_baseline_jit_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_host_baseline_jit.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c)
    target_include_directories(zr_vm_ssa_host_baseline_jit_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_host_baseline_jit_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_host_baseline_jit COMMAND zr_vm_ssa_host_baseline_jit_test)
    set_tests_properties(ssa_host_baseline_jit PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_generated_fusion_test)
    add_executable(zr_vm_ssa_generated_fusion_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_generated_fusion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/call_binding.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_lifecycle.c)
    target_include_directories(zr_vm_ssa_generated_fusion_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_generated_fusion_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_generated_fusion COMMAND zr_vm_ssa_generated_fusion_test)
    set_tests_properties(ssa_generated_fusion PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_aggregate_soa_test)
    add_executable(zr_vm_ssa_aggregate_soa_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_aggregate_soa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c)
    target_include_directories(zr_vm_ssa_aggregate_soa_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_aggregate_soa_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_aggregate_soa COMMAND zr_vm_ssa_aggregate_soa_test)
    set_tests_properties(ssa_aggregate_soa PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_call_return_tail_test)
    add_executable(zr_vm_ssa_call_return_tail_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_call_return_tail.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_call_return_tail_test)
    target_compile_definitions(zr_vm_ssa_call_return_tail_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    target_include_directories(zr_vm_ssa_call_return_tail_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_link_third_party_for_target(zr_vm_ssa_call_return_tail_test "zr_unity")
    zr_vm_link_core(zr_vm_ssa_call_return_tail_test)
    add_test(NAME ssa_call_return_tail COMMAND zr_vm_ssa_call_return_tail_test)
    set_tests_properties(ssa_call_return_tail PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_roots_observation_test)
    add_executable(zr_vm_ssa_roots_observation_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_roots_observation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c)
    target_include_directories(zr_vm_ssa_roots_observation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_roots_observation_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_core(zr_vm_ssa_roots_observation_test)
    add_test(NAME ssa_roots_observation COMMAND zr_vm_ssa_roots_observation_test)
    set_tests_properties(ssa_roots_observation PROPERTIES LABELS "ssa")
endif ()
