if(NOT TARGET zr_vm_core_static)
    return()
endif()
add_executable(zr_vm_value_debug_string_bounds_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_value_debug_string_bounds.c")
target_include_directories(zr_vm_value_debug_string_bounds_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include")
target_link_libraries(zr_vm_value_debug_string_bounds_test PRIVATE zr_vm_core_static)
set_target_properties(zr_vm_value_debug_string_bounds_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_value_debug_string_bounds_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_value_debug_string_bounds_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS long_type long_key long_value long_array object_exact_fit
        object_exact_capacity short_object short_array preview_10 preview_11)
    add_test(NAME value_debug_string_boundary_${case} COMMAND zr_vm_value_debug_string_bounds_test ${case})
    set_tests_properties(value_debug_string_boundary_${case} PROPERTIES TIMEOUT 15)
endforeach()
