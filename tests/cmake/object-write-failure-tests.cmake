# The actual private Reflection helper is intentionally tested through its
# header and static Core archive; no exported replacement or mock is supplied.
foreach(required_target IN ITEMS zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
    if(NOT TARGET ${required_target})
        message(STATUS "Object write failure tests require actual target ${required_target}")
        return()
    endif()
endforeach()
add_executable(zr_vm_object_write_failure_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_object_write_failures.c")
target_include_directories(zr_vm_object_write_failure_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include")
target_link_libraries(zr_vm_object_write_failure_test PRIVATE
        zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static
        zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_object_write_failure_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_object_write_failure_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_object_write_failure_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS init_core rehash_member rehash_index rehash_library
        rehash_reflection pair_core pair_library pair_reflection struct_clone_pair struct_write_bucket)
    add_test(NAME core_object_write_failure_${case}
            COMMAND zr_vm_object_write_failure_test ${case})
    set_tests_properties(core_object_write_failure_${case} PROPERTIES TIMEOUT 30)
endforeach()
