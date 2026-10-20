if(NOT TARGET zr_vm_core_static OR NOT TARGET zr_vm_cli_executable)
    return()
endif()
add_executable(zr_vm_cli_command_boundaries_test
        "${CMAKE_SOURCE_DIR}/tests/cli/test_cli_command_boundaries.c"
        "${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/command/command.c"
        "${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c")
target_include_directories(zr_vm_cli_command_boundaries_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_cli/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
target_link_libraries(zr_vm_cli_command_boundaries_test PRIVATE zr_vm_core_static)
set_target_properties(zr_vm_cli_command_boundaries_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_cli_command_boundaries_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_cli_command_boundaries_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS timeout_overflow timeout_huge timeout_control jobs_overflow jobs_control
        compile_interp compile_defaults stale_errno)
    add_test(NAME cli_command_boundary_${case} COMMAND zr_vm_cli_command_boundaries_test ${case})
    set_tests_properties(cli_command_boundary_${case} PROPERTIES TIMEOUT 15)
endforeach()
