if (NOT TARGET zr_vm_ssa_execbc_vm_dead_place_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_execbc_vm_dead_place_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_execbc_vm_dead_place.c)
    target_include_directories(zr_vm_ssa_execbc_vm_dead_place_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core(zr_vm_ssa_execbc_vm_dead_place_test)
    add_test(NAME ssa_execbc_vm_dead_place
            COMMAND zr_vm_ssa_execbc_vm_dead_place_test)
    set_tests_properties(ssa_execbc_vm_dead_place PROPERTIES
            LABELS "ssa" TIMEOUT 30)
endif ()
