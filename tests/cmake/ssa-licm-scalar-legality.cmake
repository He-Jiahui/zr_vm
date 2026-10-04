# Isolated semantic LICM gate. Include after ssa-tests.cmake creates the loop
# support target; reuse its source closure and add the actual Core Oracle.
if (NOT TARGET zr_vm_ssa_licm_scalar_legality_test)
    get_target_property(zr_licm_scalar_support_sources
            zr_vm_ssa_loops_specialization_test SOURCES)
    list(REMOVE_ITEM zr_licm_scalar_support_sources
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_loops_specialization.c)
    add_executable(zr_vm_ssa_licm_scalar_legality_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_licm_scalar_legality.c
            ${zr_licm_scalar_support_sources}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c)
    target_include_directories(zr_vm_ssa_licm_scalar_legality_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_licm_scalar_legality_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    if (NOT WIN32)
        target_link_libraries(zr_vm_ssa_licm_scalar_legality_test PRIVATE m)
    endif ()
    add_test(NAME ssa_licm_scalar_legality COMMAND zr_vm_ssa_licm_scalar_legality_test)
    set_tests_properties(ssa_licm_scalar_legality PROPERTIES LABELS "ssa" TIMEOUT 30)
endif ()
