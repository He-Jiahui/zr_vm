foreach(required IN ITEMS zr_vm_core_static zr_vm_library_static zr_vm_parser_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
    if(NOT TARGET ${required})
        message(STATUS "System FS failure tests require ${required}")
        return()
    endif()
endforeach()
add_executable(zr_vm_system_fs_failures_test
        "${CMAKE_SOURCE_DIR}/tests/system/test_system_fs_failures.c")
target_include_directories(zr_vm_system_fs_failures_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include")
target_link_libraries(zr_vm_system_fs_failures_test PRIVATE
        zr_vm_library_static zr_vm_parser_static zr_vm_core_static zr_vm_lib_math_static
        zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_system_fs_failures_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_system_fs_failures_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_system_fs_failures_test PRIVATE -Wall -Wextra -Werror)
endif()
set(ZR_VM_SYSTEM_FS_FAILURE_FIXTURE_ROOT
        "${CMAKE_CURRENT_BINARY_DIR}/system-fs-failures"
        CACHE PATH "Registered fixture parent; root cleans children by hash after process exit")
set(system_fs_failure_cases module_nul stream_nul module_plain module_empty stream_plain stream_empty open_control)
if(WIN32)
    list(APPEND system_fs_failure_cases open_oom)
endif()
foreach(case IN LISTS system_fs_failure_cases)
    add_test(NAME system_fs_failure_${case}
            COMMAND zr_vm_system_fs_failures_test ${case} "${ZR_VM_SYSTEM_FS_FAILURE_FIXTURE_ROOT}/${case}")
    set_tests_properties(system_fs_failure_${case} PROPERTIES TIMEOUT 30)
endforeach()
