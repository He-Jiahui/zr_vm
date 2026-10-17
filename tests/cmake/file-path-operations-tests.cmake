foreach(required IN ITEMS zr_vm_core_static zr_vm_library_static zr_vm_parser_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
    if(NOT TARGET ${required})
        return()
    endif()
endforeach()
add_executable(zr_vm_file_path_operations_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_file_path_operations.c")
target_include_directories(zr_vm_file_path_operations_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
target_link_libraries(zr_vm_file_path_operations_test PRIVATE
        zr_vm_library_static zr_vm_parser_static zr_vm_core_static zr_vm_lib_math_static
        zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_file_path_operations_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_file_path_operations_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_file_path_operations_test PRIVATE -Wall -Wextra -Werror)
endif()
set(ZR_VM_FILE_PATH_FIXTURE_ROOT "${CMAKE_BINARY_DIR}/tests/generated/file-path-operations"
        CACHE PATH "Explicitly registered generated filesystem fixture root")
set(file_path_cases move_self move_self_no_overwrite copy_ancestor move_ancestor move_missing_source
        copy_self_control copy_sibling_control move_sibling_control copy_component_prefix_control join_overflow join_control)
if(WIN32)
    list(APPEND file_path_cases unc_root drive_relative)
endif()
foreach(case IN LISTS file_path_cases)
    add_test(NAME library_file_path_${case}
            COMMAND ${CMAKE_COMMAND}
            "-DTEST_EXECUTABLE=$<TARGET_FILE:zr_vm_file_path_operations_test>"
            "-DTEST_CASE=${case}"
            "-DFIXTURE_PARENT=${ZR_VM_FILE_PATH_FIXTURE_ROOT}/${case}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/run_fresh_filesystem_fixture.cmake")
    set_tests_properties(library_file_path_${case} PROPERTIES TIMEOUT 30)
endforeach()
