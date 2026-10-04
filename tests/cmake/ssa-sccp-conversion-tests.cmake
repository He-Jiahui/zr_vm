get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if (NOT TARGET zr_vm_ssa_sccp_conversion_test)
    add_executable(zr_vm_ssa_sccp_conversion_test
            ${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_sccp_conversion.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_consumer.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/canonical_type_index.c)
    target_include_directories(zr_vm_ssa_sccp_conversion_test PRIVATE
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_sccp_conversion_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_core(zr_vm_ssa_sccp_conversion_test)
    add_test(NAME ssa_sccp_conversion COMMAND zr_vm_ssa_sccp_conversion_test)
    set_tests_properties(ssa_sccp_conversion PROPERTIES LABELS "ssa" TIMEOUT 120)
endif ()

if (NOT TARGET zr_vm_ssa_dce_phi_liveness_test)
    if (NOT TARGET zr_vm_core_shared AND NOT TARGET zr_vm_core_static)
        message(FATAL_ERROR "ssa_dce_phi_liveness requires a zr_vm_core library target")
    endif ()
    get_target_property(zr_vm_ssa_dce_sources zr_vm_ssa_sccp_conversion_test SOURCES)
    list(REMOVE_ITEM zr_vm_ssa_dce_sources
            ${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_sccp_conversion.c)
    list(APPEND zr_vm_ssa_dce_sources
            ${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_dce_phi_liveness.c)
    add_executable(zr_vm_ssa_dce_phi_liveness_test ${zr_vm_ssa_dce_sources})
    get_target_property(zr_vm_ssa_dce_includes
            zr_vm_ssa_sccp_conversion_test INCLUDE_DIRECTORIES)
    get_target_property(zr_vm_ssa_dce_definitions
            zr_vm_ssa_sccp_conversion_test COMPILE_DEFINITIONS)
    target_include_directories(zr_vm_ssa_dce_phi_liveness_test PRIVATE
            ${zr_vm_ssa_dce_includes})
    target_compile_definitions(zr_vm_ssa_dce_phi_liveness_test PRIVATE
            ${zr_vm_ssa_dce_definitions})
    zr_vm_link_core(zr_vm_ssa_dce_phi_liveness_test)
    add_test(NAME ssa_dce_phi_liveness COMMAND zr_vm_ssa_dce_phi_liveness_test)
    set_tests_properties(ssa_dce_phi_liveness PROPERTIES LABELS "ssa" TIMEOUT 120)
endif ()
