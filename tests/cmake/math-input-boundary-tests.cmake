# Use the current real Core ABI and actual Library/Math implementations.
if(NOT TARGET zr_vm_lib_math_static OR NOT TARGET zr_vm_library_static)
    message(STATUS "Math input boundary tests require actual static Math and Library targets")
    return()
endif()
add_executable(zr_vm_math_input_boundaries_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_math_input_boundaries.c")
target_include_directories(zr_vm_math_input_boundaries_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include")
target_link_libraries(zr_vm_math_input_boundaries_test PRIVATE
        zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static
        zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_math_input_boundaries_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_math_input_boundaries_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_math_input_boundaries_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS int_controls int_finite_range int_nonfinite int_boundaries
        array_conversion shape_product tensor_data tensor_controls storage_conversion)
    add_test(NAME math_input_boundary_${case}
            COMMAND zr_vm_math_input_boundaries_test ${case})
    set_tests_properties(math_input_boundary_${case} PROPERTIES TIMEOUT 20)
endforeach()
