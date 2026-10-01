# Real source -> SemIR -> ExecIR -> ExecBC VM materialization integration.
if (NOT TARGET zr_vm_ssa_source_execbc_vm_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_execbc_vm_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_execbc_vm.c)
    target_include_directories(zr_vm_ssa_source_execbc_vm_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_execbc_vm_test)
    add_test(NAME ssa_source_execbc_vm
            COMMAND zr_vm_ssa_source_execbc_vm_test)
    set_tests_properties(ssa_source_execbc_vm PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()
