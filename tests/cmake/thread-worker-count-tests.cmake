foreach(required IN ITEMS zr_vm_thread_static zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
    if(NOT TARGET ${required})
        message(STATUS "Thread worker count tests require ${required}")
        return()
    endif()
endforeach()
add_executable(zr_vm_thread_worker_count_test "${CMAKE_SOURCE_DIR}/tests/thread/test_thread_worker_count.c")
target_include_directories(zr_vm_thread_worker_count_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include" "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include")
target_link_libraries(zr_vm_thread_worker_count_test PRIVATE zr_vm_thread_static zr_vm_parser_static
        zr_vm_core_static zr_vm_library_static zr_vm_lib_math_static zr_vm_lib_system_static
        zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_thread_worker_count_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_thread_worker_count_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_thread_worker_count_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS wrap_zero wrap_one int64_max normal_one zero negative)
    add_test(NAME thread_worker_count_${case} COMMAND zr_vm_thread_worker_count_test ${case})
    set_tests_properties(thread_worker_count_${case} PROPERTIES TIMEOUT 30)
endforeach()
