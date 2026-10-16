# 此回归链接实际 Core，独立于替换异常边界的原生数组单元测试。
add_executable(zr_vm_core_allocation_lifecycle_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_core_allocation_lifecycle.c")
target_include_directories(zr_vm_core_allocation_lifecycle_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_core_allocation_lifecycle_test)
set_target_properties(zr_vm_core_allocation_lifecycle_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_core_allocation_lifecycle_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_core_allocation_lifecycle_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS main_state_oom import_array_oom main_stack_oom unlaunched_state
        worker_stack_oom_retry stack_size_overflow)
    add_test(NAME core_allocation_${case}
            COMMAND zr_vm_core_allocation_lifecycle_test ${case})
    set_tests_properties(core_allocation_${case} PROPERTIES TIMEOUT 30)
endforeach()
