add_executable(zr_vm_gc_root_leases_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_gc_root_leases.c")
target_include_directories(zr_vm_gc_root_leases_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_gc_root_leases_test)
target_link_libraries(zr_vm_gc_root_leases_test PRIVATE Threads::Threads)
set_target_properties(zr_vm_gc_root_leases_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_gc_root_leases_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_gc_root_leases_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS state_temporary_roots elision_foreign_handoff explicit_before_pin explicit_during_pin same_object_threads reserve_oom temporary_roots distinct_root_threads region_reserve_oom)
    add_test(NAME core_gc_root_leases_${case} COMMAND zr_vm_gc_root_leases_test ${case})
    set_tests_properties(core_gc_root_leases_${case} PROPERTIES TIMEOUT 30)
endforeach()
