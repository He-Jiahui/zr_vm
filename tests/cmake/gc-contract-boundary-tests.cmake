if(NOT TARGET zr_vm_core_static)
    return()
endif()
add_executable(zr_vm_gc_contract_boundaries_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_gc_contract_boundaries.c")
target_include_directories(zr_vm_gc_contract_boundaries_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include")
target_link_libraries(zr_vm_gc_contract_boundaries_test PRIVATE zr_vm_core_static)
set_target_properties(zr_vm_gc_contract_boundaries_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_gc_contract_boundaries_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_gc_contract_boundaries_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS reserved invalid_budget null_budget idle_phase complete_phase negative_phase
        work_control major_overflow major_negative_phase major_negative_next_phase compact_negative_mode compact_partition
        compact_partition_overflow compact_control)
    add_test(NAME gc_contract_boundary_${case} COMMAND zr_vm_gc_contract_boundaries_test ${case})
    set_tests_properties(gc_contract_boundary_${case} PROPERTIES TIMEOUT 15)
endforeach()
