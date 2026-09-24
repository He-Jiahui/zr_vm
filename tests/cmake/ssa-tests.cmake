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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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

if (NOT TARGET zr_vm_ssa_dominator_cfg_test)
    add_executable(zr_vm_ssa_dominator_cfg_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_dominator_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)
    target_include_directories(zr_vm_ssa_dominator_cfg_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_dominator_cfg_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_dominator_cfg COMMAND zr_vm_ssa_dominator_cfg_test)
    set_tests_properties(ssa_dominator_cfg PROPERTIES LABELS "ssa")
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/ssa-builder-tests.cmake)

if (NOT TARGET zr_vm_ssa_source_cleanup_cfg_test)
    zr_vm_add_unity_test_target(
            zr_vm_ssa_source_cleanup_cfg_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_cleanup_cfg.c)
    target_include_directories(zr_vm_ssa_source_cleanup_cfg_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_cleanup_cfg_test)
    add_test(NAME ssa_source_cleanup_cfg
            COMMAND zr_vm_ssa_source_cleanup_cfg_test)
    set_tests_properties(ssa_source_cleanup_cfg PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_value_validation_test)
    add_executable(zr_vm_ssa_value_validation_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_value_validation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)
    target_include_directories(zr_vm_ssa_value_validation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_value_validation_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_value_validation COMMAND zr_vm_ssa_value_validation_test)
    set_tests_properties(ssa_value_validation PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_state_map_ownership_test)
    add_executable(zr_vm_ssa_state_map_ownership_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_state_map_ownership.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/tests/parser/ssa_owner_fault_allocator.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_state_map_ownership_test)
    target_include_directories(zr_vm_ssa_state_map_ownership_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_state_map_ownership_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_state_map_ownership_test "zr_unity")
    add_test(NAME ssa_state_map_ownership COMMAND zr_vm_ssa_state_map_ownership_test)
    set_tests_properties(ssa_state_map_ownership PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_state_map_liveness_test)
    add_executable(zr_vm_ssa_state_map_liveness_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_state_map_liveness.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_state_map_liveness_test)
    target_include_directories(zr_vm_ssa_state_map_liveness_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_state_map_liveness_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_state_map_liveness_test "zr_unity")
    add_test(NAME ssa_state_map_liveness COMMAND zr_vm_ssa_state_map_liveness_test)
    set_tests_properties(ssa_state_map_liveness PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_runtime_objects_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_runtime_objects_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_runtime_objects.c
            ${CMAKE_SOURCE_DIR}/tests/core/ssa_runtime_objects_concurrency.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    # The fault copy also calls private GC helpers. Keep it inside the DLL on
    # Windows, where those helpers intentionally have no exported ABI.
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_core_shared PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/core/ssa_runtime_objects_faults.c)
    else ()
        target_sources(zr_vm_ssa_runtime_objects_test PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/core/ssa_runtime_objects_faults.c)
    endif ()
    target_include_directories(zr_vm_ssa_runtime_objects_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_core(zr_vm_ssa_runtime_objects_test)
    target_link_libraries(zr_vm_ssa_runtime_objects_test PRIVATE Threads::Threads)
    add_test(NAME ssa_runtime_objects COMMAND zr_vm_ssa_runtime_objects_test)
    set_tests_properties(ssa_runtime_objects PROPERTIES LABELS "ssa" TIMEOUT 30)
endif ()

if (NOT TARGET zr_vm_ssa_deopt_aggregates_test)
    add_executable(zr_vm_ssa_deopt_aggregates_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_deopt_aggregates.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/tests/parser/ssa_deopt_aggregate_fault_allocator.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_deopt_aggregates_test)
    target_include_directories(zr_vm_ssa_deopt_aggregates_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_deopt_aggregates_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_deopt_aggregates_test "zr_unity")
    add_test(NAME ssa_deopt_aggregates COMMAND zr_vm_ssa_deopt_aggregates_test)
    set_tests_properties(ssa_deopt_aggregates PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_deopt_validation_test)
    add_executable(zr_vm_ssa_deopt_validation_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_deopt_validation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_deopt_validation_test)
    target_include_directories(zr_vm_ssa_deopt_validation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_deopt_validation_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_deopt_validation_test "zr_unity")
    add_test(NAME ssa_deopt_validation COMMAND zr_vm_ssa_deopt_validation_test)
    set_tests_properties(ssa_deopt_validation PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_state_maps_test)
    add_executable(zr_vm_ssa_state_maps_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
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

if (NOT TARGET zr_vm_ssa_oracle_resume_test)
    add_executable(zr_vm_ssa_oracle_resume_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_oracle_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${CMAKE_SOURCE_DIR}/tests/parser/ssa_oracle_resume_fault_allocator.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_oracle_resume_test)
    target_include_directories(zr_vm_ssa_oracle_resume_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_oracle_resume_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_oracle_resume_test "zr_unity")
    add_test(NAME ssa_oracle_resume COMMAND zr_vm_ssa_oracle_resume_test)
    set_tests_properties(ssa_oracle_resume PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_oracle_projections_test)
    add_executable(zr_vm_ssa_oracle_projections_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_oracle_projections.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
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

if (NOT TARGET zr_vm_ssa_oracle_parallel_edges_test)
    add_executable(zr_vm_ssa_oracle_parallel_edges_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_oracle_parallel_edges.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c)
    target_include_directories(zr_vm_ssa_oracle_parallel_edges_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_oracle_parallel_edges_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_oracle_parallel_edges COMMAND zr_vm_ssa_oracle_parallel_edges_test)
    set_tests_properties(ssa_oracle_parallel_edges PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_pass_manager_scalar_test)
    add_executable(zr_vm_ssa_pass_manager_scalar_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_pass_manager_scalar.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_hash.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_summary.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c)
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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

if (NOT TARGET zr_vm_ssa_loops_specialization_test)
    add_executable(zr_vm_ssa_loops_specialization_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_loops_specialization.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c)
    target_include_directories(zr_vm_ssa_loops_specialization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_loops_specialization_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_loops_specialization COMMAND zr_vm_ssa_loops_specialization_test)
    set_tests_properties(ssa_loops_specialization PROPERTIES LABELS "ssa")
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
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

# Core-facing frame descriptors are tested independently from the parser
# producer.  Keeping this contract test standalone catches accidental
# dependencies on parser diagnostics or pointer-bearing producer state.
if (NOT TARGET zr_vm_ssa_core_frame_layout_test)
    add_executable(zr_vm_ssa_core_frame_layout_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_frame_layout.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c)
    target_include_directories(zr_vm_ssa_core_frame_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_core_frame_layout_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_core_frame_layout COMMAND zr_vm_ssa_core_frame_layout_test)
    set_tests_properties(ssa_core_frame_layout PROPERTIES LABELS "ssa")
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

if (NOT TARGET zr_vm_ssa_c_llvm_lowering_test)
    add_executable(zr_vm_ssa_c_llvm_lowering_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_c_llvm_lowering.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/aot_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c)
    target_include_directories(zr_vm_ssa_c_llvm_lowering_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_c_llvm_lowering_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_c_llvm_lowering COMMAND zr_vm_ssa_c_llvm_lowering_test)
    set_tests_properties(ssa_c_llvm_lowering PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_build_profiles_test)
    add_executable(zr_vm_ssa_build_profiles_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_build_profiles.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_content_hash.c)
    target_include_directories(zr_vm_ssa_build_profiles_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
    target_compile_definitions(zr_vm_ssa_build_profiles_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_build_profiles COMMAND zr_vm_ssa_build_profiles_test)
    set_tests_properties(ssa_build_profiles PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_platform_matrix_test)
    add_executable(zr_vm_ssa_platform_matrix_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_platform_matrix.c
            ${CMAKE_SOURCE_DIR}/zr_vm_common/src/zr_vm_common/ssa_platform_contract.c)
    target_include_directories(zr_vm_ssa_platform_matrix_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_platform_matrix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_platform_matrix COMMAND zr_vm_ssa_platform_matrix_test)
    set_tests_properties(ssa_platform_matrix PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_backend_service_test)
    add_executable(zr_vm_ssa_backend_service_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_backend_service.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_backend.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c)
    target_include_directories(zr_vm_ssa_backend_service_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_backend_service_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_backend_service COMMAND zr_vm_ssa_backend_service_test)
    set_tests_properties(ssa_backend_service PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_schema_relocation_test)
    add_executable(zr_vm_ssa_schema_relocation_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_ssa_schema_relocation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/execbc_verify.c
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c)
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

# 05.03's parser admission pass is kept as a small standalone contract test.
# It intentionally does not add a second `ssa_*` denominator row: the
# canonical maps/strings row above covers the runtime storage contract while
# this target exercises the parser-side proof gate and generic fallback.
if (NOT TARGET zr_vm_container_specialization_test)
    add_executable(zr_vm_container_specialization_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_container_specialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/object/container_storage_contract.c)
    target_include_directories(zr_vm_container_specialization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_container_specialization_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME container_specialization COMMAND zr_vm_container_specialization_test)
    set_tests_properties(container_specialization PROPERTIES LABELS "ssa")
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
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_arrays_slices.c
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c)
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c)
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
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_budget.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_major.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_compact.c)
    target_include_directories(zr_vm_ssa_major_budget_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_major_budget_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_major_budget COMMAND zr_vm_ssa_major_budget_test)
    set_tests_properties(ssa_major_budget PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_major_budget_runtime_test)
    add_executable(zr_vm_ssa_major_budget_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_major_budget_runtime.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c)
    target_include_directories(zr_vm_ssa_major_budget_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_major_budget_runtime_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_major_budget_runtime COMMAND zr_vm_ssa_major_budget_runtime_test)
    set_tests_properties(ssa_major_budget_runtime PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_young_allocation_test)
    add_executable(zr_vm_ssa_young_allocation_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_young_allocation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_tlab.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_minor.c)
    target_include_directories(zr_vm_ssa_young_allocation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_young_allocation_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_young_allocation COMMAND zr_vm_ssa_young_allocation_test)
    set_tests_properties(ssa_young_allocation PROPERTIES LABELS "ssa")
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

if (NOT TARGET zr_vm_ssa_cross_domain_clone_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_cross_domain_clone_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_cross_domain_clone.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c)
    target_compile_definitions(zr_vm_ssa_cross_domain_clone_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_include_directories(zr_vm_ssa_cross_domain_clone_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_core(zr_vm_ssa_cross_domain_clone_test)
    add_test(NAME ssa_cross_domain_clone COMMAND zr_vm_ssa_cross_domain_clone_test)
    set_tests_properties(ssa_cross_domain_clone PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_async_frame_budget_test)
    add_executable(zr_vm_ssa_async_frame_budget_test
            ${CMAKE_SOURCE_DIR}/tests/task/test_ssa_async_frame_budget.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c)
    target_include_directories(zr_vm_ssa_async_frame_budget_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_async_frame_budget_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_async_frame_budget COMMAND zr_vm_ssa_async_frame_budget_test)
    set_tests_properties(ssa_async_frame_budget PROPERTIES LABELS "ssa")
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

# 09.03 planner coverage is a separate non-denominator fixture because the
# existing batch runtime test has its own executable entry point.  Keeping the
# planner target standalone makes its no-rewrite and scalar-fallback contract
# testable without changing the 47-leaf CTest denominator.
if (NOT TARGET zr_vm_vectorize_pass_test)
    add_executable(zr_vm_vectorize_pass_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_vectorize_pass.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)
    target_include_directories(zr_vm_vectorize_pass_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_vectorize_pass_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME vectorize_pass COMMAND zr_vm_vectorize_pass_test)
    set_tests_properties(vectorize_pass PROPERTIES LABELS "ssa")
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

if (NOT TARGET zr_vm_ssa_inferred_protocols_test)
    add_executable(zr_vm_ssa_inferred_protocols_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_inferred_protocols.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_protocols.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c)
    target_include_directories(zr_vm_ssa_inferred_protocols_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_inferred_protocols_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_inferred_protocols COMMAND zr_vm_ssa_inferred_protocols_test)
    set_tests_properties(ssa_inferred_protocols PROPERTIES LABELS "ssa")
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

# The C++ adapter is optional.  Keep the contract-only C test out of default
# builds so a normal C11 checkout remains independent of a C++ toolchain.
if (ZR_VM_ENABLE_HOST_JIT AND NOT TARGET zr_vm_ssa_host_jit_optional_test)
    add_executable(zr_vm_ssa_host_jit_optional_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_host_jit_optional.c)
    set_target_properties(zr_vm_ssa_host_jit_optional_test PROPERTIES
            LINKER_LANGUAGE CXX)
    target_include_directories(zr_vm_ssa_host_jit_optional_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_jit/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_link_libraries(zr_vm_ssa_host_jit_optional_test PRIVATE zr_vm_jit)
    target_compile_definitions(zr_vm_ssa_host_jit_optional_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_host_jit_optional
            COMMAND zr_vm_ssa_host_jit_optional_test)
    set_tests_properties(ssa_host_jit_optional PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_generated_fusion_test)
    add_executable(zr_vm_ssa_generated_fusion_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_generated_fusion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/call_binding.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/call_binding_graph.c
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

if (NOT TARGET zr_vm_ssa_core_roots_observation_test)
    add_executable(zr_vm_ssa_core_roots_observation_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_roots_observation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c)
    target_include_directories(zr_vm_ssa_core_roots_observation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_core_roots_observation_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_core_roots_observation
            COMMAND zr_vm_ssa_core_roots_observation_test)
    set_tests_properties(ssa_core_roots_observation PROPERTIES LABELS "ssa")
endif ()

# The AOT archive is an optional consumer of the shared parser/core libraries.
# Keep its adapter contract test independent from the dormant native emitters:
# descriptor facts and explicit artifact-unavailable status are still
# validated when no AOT archive target is enabled.
if (NOT TARGET zr_vm_ssa_aot_backend_adapters_test)
    add_executable(zr_vm_ssa_aot_backend_adapters_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_aot_backend_adapters.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c)
    target_include_directories(zr_vm_ssa_aot_backend_adapters_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_aot_backend_adapters_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_parser_core(zr_vm_ssa_aot_backend_adapters_test)
    add_test(NAME aot_backend_adapters COMMAND zr_vm_ssa_aot_backend_adapters_test)
    set_tests_properties(aot_backend_adapters PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_aot_runner_coverage_test)
    add_executable(zr_vm_ssa_aot_runner_coverage_test
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/test_ssa_aot_runner_coverage.c
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner/aot_coverage.c
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner/aot_runner.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_report.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c)
    target_include_directories(zr_vm_ssa_aot_runner_coverage_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner
            ${CMAKE_SOURCE_DIR}/tests/performance
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_aot_runner_coverage_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_aot_runner_coverage
            COMMAND zr_vm_ssa_aot_runner_coverage_test)
    set_tests_properties(ssa_aot_runner_coverage PROPERTIES LABELS "ssa")
    if (NOT WIN32)
        target_link_libraries(zr_vm_ssa_aot_runner_coverage_test PRIVATE m)
    endif ()
endif ()

# Build the process-facing runner as a separate artifact as well.  It has no
# generated entries in the repository, so it is intentionally not registered
# as a passing CTest; invoking it without a provider must report unavailable.
if (NOT TARGET zr_vm_aot_benchmark_runner)
    add_executable(zr_vm_aot_benchmark_runner
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner/main.c
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner/aot_runner.c
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner/aot_coverage.c)
    target_include_directories(zr_vm_aot_benchmark_runner PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/aot_runner
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_aot_benchmark_runner PRIVATE
            _CRT_SECURE_NO_WARNINGS)
endif ()

if (NOT TARGET zr_vm_ssa_optimization_remarks_test)
    add_executable(zr_vm_ssa_optimization_remarks_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_optimization_remarks.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/optimization_remark.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c)
    target_include_directories(zr_vm_ssa_optimization_remarks_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_optimization_remarks_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_optimization_remarks
            COMMAND zr_vm_ssa_optimization_remarks_test)
    set_tests_properties(ssa_optimization_remarks PROPERTIES LABELS "ssa")
endif ()

# CLI and LSP projections have separate process-facing entry points.  Keep
# this supplemental fixture outside the 47-leaf denominator while compiling
# the same pointer-free core schema and the canonical LSP range bridge.
if (NOT TARGET zr_vm_optimization_remarks_projection_test)
    add_executable(zr_vm_optimization_remarks_projection_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_optimization_remarks_projection.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/optimization_remark.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c)
    target_include_directories(zr_vm_optimization_remarks_projection_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/type_inference
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/writer
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_optimization_remarks_projection_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME optimization_remarks_projection
            COMMAND zr_vm_optimization_remarks_projection_test)
    set_tests_properties(optimization_remarks_projection PROPERTIES LABELS "ssa")
endif ()

# The release gate is intentionally self-contained: it validates the
# requirement/coverage manifest without linking the production runtime.  This
# keeps the denominator executable even in reduced builds where optional
# backends are disabled.
if (NOT TARGET zr_vm_ssa_release_acceptance_test)
    add_executable(zr_vm_ssa_release_acceptance_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_release_acceptance.c)
    target_include_directories(zr_vm_ssa_release_acceptance_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_release_acceptance_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_release_acceptance
            COMMAND zr_vm_ssa_release_acceptance_test)
    set_tests_properties(ssa_release_acceptance PROPERTIES LABELS "ssa")
endif ()
