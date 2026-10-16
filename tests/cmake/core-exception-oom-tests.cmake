# Link the actual Core; the fixture does not replace Throw, TryRun, or GC.
add_executable(zr_vm_core_exception_oom_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_core_exception_oom.c")
target_include_directories(zr_vm_core_exception_oom_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_core_exception_oom_test)
set_target_properties(zr_vm_core_exception_oom_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_core_exception_oom_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_core_exception_oom_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS persistent_allocation_oom runtime_normalization_oom
        normal_memory_error existing_error_oom nested_oom)
    add_test(NAME core_exception_${case}
            COMMAND zr_vm_core_exception_oom_test ${case})
    set_tests_properties(core_exception_${case} PROPERTIES TIMEOUT 30)
endforeach()
