if (BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB AND
        NOT TARGET zr_vm_ssa_cross_domain_clone_oom_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_cross_domain_clone_oom_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_cross_domain_clone_oom.c)
    target_compile_definitions(zr_vm_ssa_cross_domain_clone_oom_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    target_include_directories(zr_vm_ssa_cross_domain_clone_oom_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_ssa_cross_domain_clone_oom_test)
    add_test(NAME ssa_cross_domain_clone_oom
            COMMAND zr_vm_ssa_cross_domain_clone_oom_test)
    set_tests_properties(ssa_cross_domain_clone_oom PROPERTIES
            LABELS "ssa" TIMEOUT 15)
endif ()
