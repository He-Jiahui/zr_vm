if (NOT TARGET zr_vm_exec_ir_scalar_scratch_eligibility_test)
    add_executable(
            zr_vm_exec_ir_scalar_scratch_eligibility_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_exec_ir_scalar_scratch_eligibility.c)
    target_include_directories(zr_vm_exec_ir_scalar_scratch_eligibility_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_exec_ir_scalar_scratch_eligibility_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_parser_core(zr_vm_exec_ir_scalar_scratch_eligibility_test)
    add_test(NAME exec_ir_scalar_scratch_eligibility
            COMMAND zr_vm_exec_ir_scalar_scratch_eligibility_test)
    set_tests_properties(exec_ir_scalar_scratch_eligibility PROPERTIES
            LABELS "ssa;exec-ir;scalar-scratch")
endif ()
