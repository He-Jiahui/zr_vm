if (NOT TARGET zr_vm_ssa_sccp_conversion_test)
    add_executable(zr_vm_ssa_sccp_conversion_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_sccp_conversion.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_consumer.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/canonical_type_index.c)
    target_include_directories(zr_vm_ssa_sccp_conversion_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_sccp_conversion_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_core(zr_vm_ssa_sccp_conversion_test)
    add_test(NAME ssa_sccp_conversion COMMAND zr_vm_ssa_sccp_conversion_test)
    set_tests_properties(ssa_sccp_conversion PROPERTIES LABELS "ssa" TIMEOUT 120)
endif ()
