# Include roots for tests that borrow implementation sources or include private
# headers after the source layout migration. This table names only compiled
# consumers confirmed by CMake File API, compiler diagnostics and include checks.
# Keep existing include order and append roots as PRIVATE to these targets.

set(_zr_vm_layout_consumers_1
        zr_vm_language_server_lsp_capability_inventory_probe
        zr_vm_ssa_primitive_frame_validation_test
        zr_vm_ssa_source_finalized_module_test
        zr_vm_ssa_source_returned_execir_cleanup_test
)

foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_1)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_1)

set(_zr_vm_layout_consumers_2
        zr_vm_ssa_active_packed_frame_observation_test
        zr_vm_ssa_branch_arithmetic_constraints_test
        zr_vm_ssa_branch_loop_ranges_test
        zr_vm_ssa_branch_range_null_test
        zr_vm_ssa_builder_cfg_test
        zr_vm_ssa_builder_cleanup_dispatch_test
        zr_vm_ssa_builder_control_edges_test
        zr_vm_ssa_builder_dominance_test
        zr_vm_ssa_builder_fact_identity_test
        zr_vm_ssa_builder_iterator_invokes_test
        zr_vm_ssa_cfg_effects_builder_test
        zr_vm_ssa_cfg_effects_faults_test
        zr_vm_ssa_cleanup_exception_state_test
        zr_vm_ssa_compare_predicate_domain_test
        zr_vm_container_specialization_test
        zr_vm_ssa_core_model_test
        zr_vm_ssa_dce_phi_liveness_test
        zr_vm_ssa_dominator_cfg_test
        zr_vm_ssa_effects_verifier_test
        zr_vm_ssa_exec_ir_artifact_v6_test
        zr_vm_ssa_escape_ownership_test
        zr_vm_ssa_frame_layout_test
        zr_vm_ssa_function_aot_owner_test
        zr_vm_ssa_generated_fusion_test
        zr_vm_ssa_gvn_range_test
        zr_vm_ssa_interprocedural_inlining_test
        zr_vm_ssa_invoke_result_availability_test
        zr_vm_ssa_licm_scalar_legality_test
        zr_vm_ssa_linear_effects_builder_test
        zr_vm_ssa_loops_specialization_test
        zr_vm_ssa_oracle_parallel_edges_test
        zr_vm_ssa_oracle_projections_test
        zr_vm_ssa_owned_row_direct_call_graph_test
        zr_vm_ssa_pass_manager_scalar_test
        zr_vm_ssa_place_eligibility_test
        zr_vm_ssa_place_promotion_test
        zr_vm_ssa_retained_builder_cfg_test
        zr_vm_ssa_sccp_conversion_test
        zr_vm_ssa_sccp_copy_availability_test
        zr_vm_ssa_sccp_phi_copy_edges_test
        zr_vm_ssa_value_validation_test
        zr_vm_vectorize_pass_test
)

foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_2)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_2)

set(_zr_vm_layout_consumers_3
        zr_vm_aot_runtime_typed_direct_call_compatibility_test
        zr_vm_native_registry_descriptor_invalidation_test
        zr_vm_task_job_scheduler_test
)

foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_3)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_3)

set(_zr_vm_layout_consumers_4
        zr_vm_ssa_analysis_fact_capacity_test
        zr_vm_ssa_aot_backend_adapters_test
        zr_vm_ssa_branch_arithmetic_constraints_test
        zr_vm_ssa_branch_loop_ranges_test
        zr_vm_ssa_branch_range_null_test
        zr_vm_ssa_build_profiles_test
        zr_vm_ssa_builder_cfg_test
        zr_vm_ssa_builder_cleanup_dispatch_test
        zr_vm_ssa_builder_control_edges_test
        zr_vm_ssa_builder_dominance_test
        zr_vm_ssa_builder_fact_identity_test
        zr_vm_ssa_builder_iterator_invokes_test
        zr_vm_ssa_cfg_effects_builder_test
        zr_vm_ssa_cfg_effects_faults_test
        zr_vm_ssa_cleanup_exception_state_test
        zr_vm_ssa_dce_phi_liveness_test
        zr_vm_ssa_frame_layout_test
        zr_vm_ssa_generated_fusion_test
        zr_vm_ssa_interprocedural_inlining_test
        zr_vm_ssa_linear_effects_builder_test
        zr_vm_ssa_oracle_parallel_edges_test
        zr_vm_ssa_oracle_projections_test
        zr_vm_ssa_owned_row_direct_call_graph_test
        zr_vm_ssa_pass_manager_scalar_test
        zr_vm_ssa_place_eligibility_test
        zr_vm_ssa_place_promotion_test
        zr_vm_ssa_retained_builder_cfg_test
        zr_vm_ssa_roots_observation_test
        zr_vm_ssa_sccp_conversion_test
        zr_vm_ssa_sccp_copy_availability_test
        zr_vm_ssa_sccp_phi_copy_edges_test
        zr_vm_ssa_source_callable_query_test
)

foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_4)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_4)

# Both performance consumers also need the public common root: their report
# helper includes zr_vm_common/zr_common_conf.h. This was missing in the
# pre-migration baseline and is required by the complete include check.
set(_zr_vm_layout_consumers_5
        zr_vm_perf_runner
        zr_vm_perf_report_test
)
foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_5)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_common/include" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_5)

# CLI borrowing targets also resolve guarded debug public headers during the
# conservative literal-include check. Keep their existing feature definitions.
set(_zr_vm_layout_consumers_6
        zr_vm_cli_aot_compacted_metadata_sidecar_test
        zr_vm_cli_aot_writer_options_test
        zr_vm_cli_project_incremental_test
        zr_vm_cli_zrm_fixture_test
)
foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_6)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_6)

unset(_zr_vm_layout_target)
unset(_zr_vm_layout_existing)

# The same CLI sources contain a guarded thread public include. Append only
# its real public root for the literal audit, without enabling the feature.
set(_zr_vm_layout_consumers_7
        zr_vm_cli_aot_compacted_metadata_sidecar_test
        zr_vm_cli_aot_writer_options_test
        zr_vm_cli_executable
        zr_vm_cli_import_basic_fixture_test
        zr_vm_cli_project_incremental_test
        zr_vm_cli_zrm_fixture_test
)
foreach (_zr_vm_layout_target IN LISTS _zr_vm_layout_consumers_7)
    if (TARGET ${_zr_vm_layout_target})
        get_target_property(_zr_vm_layout_existing ${_zr_vm_layout_target} INCLUDE_DIRECTORIES)
        if (NOT "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include" IN_LIST _zr_vm_layout_existing)
            target_include_directories(${_zr_vm_layout_target} PRIVATE
                    "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include")
        endif ()
    endif ()
endforeach ()
unset(_zr_vm_layout_consumers_7)
