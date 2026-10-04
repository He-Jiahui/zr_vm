# Finite Core argument staging consumers; the parent inclusion is a separate lease.
if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(zr_vm_ssa_argument_staging_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_argument_staging.c)
    target_include_directories(zr_vm_ssa_argument_staging_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    target_compile_definitions(zr_vm_ssa_argument_staging_test PRIVATE
        ZR_ARGUMENT_STAGING_TEST_CANDIDATE)
    zr_vm_link_core(zr_vm_ssa_argument_staging_test)
    add_test(NAME ssa_argument_staging COMMAND zr_vm_ssa_argument_staging_test)
    set_tests_properties(ssa_argument_staging PROPERTIES LABELS "ssa" TIMEOUT 120)

    zr_vm_add_unity_test_target(zr_vm_ssa_argument_staging_vm_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_argument_staging_vm.c)
    target_include_directories(zr_vm_ssa_argument_staging_vm_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_core(zr_vm_ssa_argument_staging_vm_test)
    add_test(NAME ssa_argument_staging_vm COMMAND zr_vm_ssa_argument_staging_vm_test)
    set_tests_properties(ssa_argument_staging_vm PROPERTIES LABELS "ssa" TIMEOUT 120)
endif()
