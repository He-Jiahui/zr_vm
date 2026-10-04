# Separate finite RED target; Root owns inclusion and fresh execution evidence.
if (NOT TARGET zr_vm_ssa_branch_arithmetic_constraints_test)
    set(_zr_branch_arithmetic_core ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    add_executable(zr_vm_ssa_branch_arithmetic_constraints_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_branch_arithmetic_constraints.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_binding_rows.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_verify.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_verify_constants.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_verify_ssa.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_verify_effects.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_verify_effect_backedges.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_materialize.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_materialize_owners.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_state_map_liveness.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_deopt_aggregate.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_state_map_storage.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_owner_state.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_interpreter.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_interpreter_validate.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_interpreter_run.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_interpreter_phi.c
            ${_zr_branch_arithmetic_core}/exec_ir/exec_ir_interpreter_resume.c
            ${_zr_branch_arithmetic_core}/execution_contract.c)
    target_include_directories(zr_vm_ssa_branch_arithmetic_constraints_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_branch_arithmetic_constraints_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_branch_arithmetic_constraints
            COMMAND zr_vm_ssa_branch_arithmetic_constraints_test)
    set_tests_properties(ssa_branch_arithmetic_constraints PROPERTIES LABELS "ssa")
    unset(_zr_branch_arithmetic_core)
endif ()
