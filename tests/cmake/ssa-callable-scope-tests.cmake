if (NOT TARGET zr_vm_ssa_callable_type_scope_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_callable_type_scope_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_callable_type_scope.c)
    target_include_directories(zr_vm_ssa_callable_type_scope_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core(zr_vm_ssa_callable_type_scope_test)
    add_test(NAME ssa_callable_type_scope
            COMMAND zr_vm_ssa_callable_type_scope_test)
    set_tests_properties(ssa_callable_type_scope PROPERTIES LABELS "ssa")
endif ()
