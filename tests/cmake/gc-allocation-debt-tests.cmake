add_executable(zr_vm_gc_allocation_debt_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_gc_allocation_debt.c")
target_include_directories(zr_vm_gc_allocation_debt_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_gc_allocation_debt_test)
target_link_libraries(zr_vm_gc_allocation_debt_test PRIVATE Threads::Threads)
set_target_properties(zr_vm_gc_allocation_debt_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_gc_allocation_debt_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_gc_allocation_debt_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS saturation concurrent_allocations)
    add_test(NAME core_gc_allocation_debt_${case}
            COMMAND zr_vm_gc_allocation_debt_test ${case})
    set_tests_properties(core_gc_allocation_debt_${case} PROPERTIES TIMEOUT 30)
endforeach()
