# Public core verifier regression for throwing-result edge availability.
if (NOT TARGET zr_vm_ssa_invoke_result_availability_test)
    set(zr_invoke_result_core ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    add_executable(zr_vm_ssa_invoke_result_availability_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_invoke_result_availability.c
            ${zr_invoke_result_core}/exec_ir/exec_ir.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_binding_rows.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_verify.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_verify_constants.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_verify_ssa.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_verify_effects.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_verify_effect_backedges.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_materialize.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_materialize_owners.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_state_map_liveness.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_deopt_aggregate.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_state_map_storage.c
            ${zr_invoke_result_core}/exec_ir/exec_ir_owner_state.c
            ${zr_invoke_result_core}/execution_contract.c)
    target_include_directories(zr_vm_ssa_invoke_result_availability_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_invoke_result_availability_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_invoke_result_availability
            COMMAND zr_vm_ssa_invoke_result_availability_test)
    set_tests_properties(ssa_invoke_result_availability PROPERTIES LABELS "ssa")
    unset(zr_invoke_result_core)
endif ()
