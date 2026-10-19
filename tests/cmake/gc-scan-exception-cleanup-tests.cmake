if(NOT TARGET zr_vm_core_static)
    message(STATUS "GC scan exception cleanup tests require actual Core")
    return()
endif()
find_package(Threads REQUIRED)
add_executable(zr_vm_gc_scan_exception_cleanup_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_gc_scan_exception_cleanup.c")
target_include_directories(zr_vm_gc_scan_exception_cleanup_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
target_link_libraries(zr_vm_gc_scan_exception_cleanup_test PRIVATE
        zr_vm_core_static Threads::Threads)
set_target_properties(zr_vm_gc_scan_exception_cleanup_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_gc_scan_exception_cleanup_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_gc_scan_exception_cleanup_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS incremental_throw generational_throw incremental_control generational_control)
    add_test(NAME core_gc_scan_exception_cleanup_${case}
            COMMAND zr_vm_gc_scan_exception_cleanup_test ${case})
    set_tests_properties(core_gc_scan_exception_cleanup_${case} PROPERTIES TIMEOUT 30)
endforeach()
