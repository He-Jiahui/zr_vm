# This fragment also supports inclusion from a bounded standalone C project.
get_filename_component(_zr_compare_source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if (NOT TARGET zr_vm_ssa_compare_predicate_domain_test)
    set(_zr_compare_core_dir "${_zr_compare_source_root}/zr_vm_core/src/zr_vm_core")
    add_executable(zr_vm_ssa_compare_predicate_domain_test
            "${_zr_compare_source_root}/tests/parser/test_ssa_compare_predicate_domain.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_binding_rows.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_verify.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_verify_constants.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_verify_ssa.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_verify_effects.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_verify_effect_backedges.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_materialize.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_materialize_owners.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_state_map_liveness.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_deopt_aggregate.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_state_map_storage.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_owner_state.c"
            "${_zr_compare_core_dir}/execution_contract.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_interpreter.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_interpreter_validate.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_interpreter_run.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_interpreter_resume.c"
            "${_zr_compare_core_dir}/exec_ir/exec_ir_interpreter_phi.c")
    set_target_properties(zr_vm_ssa_compare_predicate_domain_test PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_include_directories(zr_vm_ssa_compare_predicate_domain_test PRIVATE
            "${_zr_compare_source_root}/zr_vm_core/include"
            "${_zr_compare_source_root}/zr_vm_common/include")
    target_compile_definitions(zr_vm_ssa_compare_predicate_domain_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_compare_predicate_domain_test PRIVATE /UNDEBUG)
    else ()
        target_compile_options(zr_vm_ssa_compare_predicate_domain_test PRIVATE -UNDEBUG)
    endif ()
    if (UNIX)
        target_link_libraries(zr_vm_ssa_compare_predicate_domain_test PRIVATE m)
    endif ()
    add_test(NAME ssa_compare_predicate_domain COMMAND zr_vm_ssa_compare_predicate_domain_test)
    set_tests_properties(ssa_compare_predicate_domain PROPERTIES LABELS "ssa;core" TIMEOUT 30)
    unset(_zr_compare_core_dir)
endif ()

unset(_zr_compare_source_root)
