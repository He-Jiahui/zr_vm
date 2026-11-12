# Link the actual Core; the fixture does not replace Throw, TryRun, ResetThread, or GC.
add_executable(zr_vm_worker_exception_forward_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_worker_exception_forward.c")
target_include_directories(zr_vm_worker_exception_forward_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_worker_exception_forward_test)
set_target_properties(zr_vm_worker_exception_forward_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_worker_exception_forward_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_worker_exception_forward_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME core_exception_worker_forward
        COMMAND zr_vm_worker_exception_forward_test)
set_tests_properties(core_exception_worker_forward PROPERTIES TIMEOUT 30)
