# Real source compilation and interpreter execution for eligible and failed dynamic tail calls.
if (NOT TARGET zr_vm_ssa_tail_dispatch_runtime_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_tail_dispatch_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_tail_dispatch_runtime.c)
    target_include_directories(zr_vm_ssa_tail_dispatch_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_tail_dispatch_runtime_test)
    add_test(NAME ssa_tail_dispatch_runtime
            COMMAND zr_vm_ssa_tail_dispatch_runtime_test)
    set_tests_properties(ssa_tail_dispatch_runtime PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()
