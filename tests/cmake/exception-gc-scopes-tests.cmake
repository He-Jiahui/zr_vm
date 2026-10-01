if (BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB AND
        NOT TARGET zr_vm_exception_gc_scopes_test)
    zr_vm_add_unity_test_target(zr_vm_exception_gc_scopes_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_exception_gc_scopes.c)
    target_include_directories(zr_vm_exception_gc_scopes_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_exception_gc_scopes_test)
    target_link_libraries(zr_vm_exception_gc_scopes_test PRIVATE Threads::Threads)
    add_test(NAME exception_gc_scopes COMMAND zr_vm_exception_gc_scopes_test)
    set_tests_properties(exception_gc_scopes PROPERTIES LABELS "gc;exception" TIMEOUT 15)
endif ()
