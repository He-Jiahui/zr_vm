foreach(required IN ITEMS zr_vm_thread_static zr_vm_core_static zr_vm_library_static
        zr_vm_parser_static zr_vm_lib_ffi_static zr_vm_lib_math_static
        zr_vm_lib_system_static zr_vm_lib_container_static)
    if(NOT TARGET ${required})
        message(STATUS "Thread transport facade OOM tests require ${required}")
        return()
    endif()
endforeach()
add_executable(zr_vm_thread_transport_facade_oom_test
        "${CMAKE_SOURCE_DIR}/tests/thread/test_thread_transport_facade_oom.c")
target_include_directories(zr_vm_thread_transport_facade_oom_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/src/zr_vm_lib_thread")
target_link_libraries(zr_vm_thread_transport_facade_oom_test PRIVATE
        zr_vm_thread_static zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_thread_transport_facade_oom_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_thread_transport_facade_oom_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_thread_transport_facade_oom_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS shared weak_shared unique_mutex shared_mutex normal_controls)
    add_test(NAME thread_transport_facade_oom_${case}
            COMMAND zr_vm_thread_transport_facade_oom_test ${case})
    set_tests_properties(thread_transport_facade_oom_${case} PROPERTIES TIMEOUT 30)
endforeach()
