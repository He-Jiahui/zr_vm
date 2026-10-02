if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(zr_vm_execution_checked_multiply_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_execution_checked_multiply.c)
    target_include_directories(zr_vm_execution_checked_multiply_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_execution_checked_multiply_test)
    add_test(NAME execution_checked_multiply
        COMMAND zr_vm_execution_checked_multiply_test)
    set_tests_properties(execution_checked_multiply PROPERTIES
        LABELS "ssa"
        TIMEOUT 120)
endif()
