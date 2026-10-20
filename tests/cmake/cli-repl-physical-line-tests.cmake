if(NOT TARGET zr_vm_cli_executable)
    message(STATUS "REPL physical-line tests require the real CLI executable")
    return()
endif()
set(ZR_VM_REPL_LINE_PYTHON_EXECUTABLE "" CACHE FILEPATH
        "Python interpreter for real CLI REPL physical-line tests")
if(NOT ZR_VM_REPL_LINE_PYTHON_EXECUTABLE)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(NOT Python3_Interpreter_FOUND)
        message(STATUS "REPL physical-line tests require Python")
        return()
    endif()
    set(ZR_VM_REPL_LINE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()
set(ZR_VM_REPL_LINE_TEMP_ROOT "${CMAKE_CURRENT_BINARY_DIR}/cli-repl-physical-lines"
        CACHE PATH "Owned REPL per-case TEMP/TMP parent (must be outside checkout)")
foreach(case IN ITEMS short_control cross_1023 cross_2046 exact_boundary crlf_split crlf_control)
    add_test(NAME cli_repl_physical_lines_${case}
            COMMAND "${ZR_VM_REPL_LINE_PYTHON_EXECUTABLE}" -B
                    "${CMAKE_SOURCE_DIR}/tests/cli/test_cli_repl_physical_lines.py"
                    --cli "$<TARGET_FILE:zr_vm_cli_executable>"
                    --temp-root "${ZR_VM_REPL_LINE_TEMP_ROOT}" --case "${case}")
    set_tests_properties(cli_repl_physical_lines_${case} PROPERTIES TIMEOUT 30)
endforeach()
