if (NOT TARGET zr_vm_ssa_module_constant_pool_verifier_test)
    add_executable(zr_vm_ssa_module_constant_pool_verifier_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_module_constant_pool_verifier.c)
    target_include_directories(zr_vm_ssa_module_constant_pool_verifier_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_module_constant_pool_verifier_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    zr_vm_link_core(zr_vm_ssa_module_constant_pool_verifier_test)
    add_test(NAME ssa_module_constant_pool_verifier
            COMMAND zr_vm_ssa_module_constant_pool_verifier_test)
    set_tests_properties(ssa_module_constant_pool_verifier PROPERTIES LABELS "ssa")
endif ()
