if (BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB AND
        NOT TARGET zr_vm_gc_nested_mutation_test)
    zr_vm_add_unity_test_target(
            zr_vm_gc_nested_mutation_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_gc_nested_mutation.c)
    target_include_directories(zr_vm_gc_nested_mutation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_gc_nested_mutation_test)
    target_link_libraries(zr_vm_gc_nested_mutation_test PRIVATE Threads::Threads)
    add_test(NAME gc_nested_mutation COMMAND zr_vm_gc_nested_mutation_test)
    set_tests_properties(gc_nested_mutation PROPERTIES
            LABELS "gc" TIMEOUT 15)
endif ()
