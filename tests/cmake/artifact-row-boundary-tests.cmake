# Public fixed-row view boundaries, against actual Core without source copies.
add_executable(zr_vm_artifact_row_boundaries_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_artifact_row_boundaries.c")
target_include_directories(zr_vm_artifact_row_boundaries_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_artifact_row_boundaries_test)
set_target_properties(zr_vm_artifact_row_boundaries_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_artifact_row_boundaries_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_artifact_row_boundaries_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS valid_rows wrong_kind null_data truncated inconsistent_count
        huge_count invalid_row diagnostic_reuse status_names)
    add_test(NAME artifact_row_boundary_${case}
            COMMAND zr_vm_artifact_row_boundaries_test ${case})
    set_tests_properties(artifact_row_boundary_${case} PROPERTIES TIMEOUT 20)
endforeach()
