if (NOT DEFINED CLI_EXE OR CLI_EXE STREQUAL "")
    message(FATAL_ERROR "CLI_EXE is required")
endif ()
if (NOT DEFINED PROJECT_FILE OR PROJECT_FILE STREQUAL "")
    message(FATAL_ERROR "PROJECT_FILE is required")
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")
get_filename_component(reference_root "${PROJECT_FILE}" DIRECTORY)

function(run_testing_reference_case expected_exit expected_regex)
    execute_process(
            COMMAND "${CLI_EXE}" ${ARGN}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr
            TIMEOUT 30
    )
    set(combined "${stdout}${stderr}")
    if (NOT result EQUAL expected_exit)
        message(FATAL_ERROR
                "test command exit mismatch: expected ${expected_exit}, got ${result}\n${combined}")
    endif ()
    if (NOT combined MATCHES "${expected_regex}")
        message(FATAL_ERROR
                "test command output mismatch: expected /${expected_regex}/\n${combined}")
    endif ()
endfunction()

run_testing_reference_case(
        0
        "test-result: passed=5 failed=0 skipped=1 timedout=0 crashed=0"
        test "${PROJECT_FILE}" --jobs 2 --timeout 5s
)
run_testing_reference_case(
        0
        "main::orderedPair#0\\(1,2\\).*main::orderedPair#1\\(-1,1\\)"
        test "${PROJECT_FILE}" --filter "*::orderedPair#*" --list
)
run_testing_reference_case(
        0
        "test-result: passed=2 failed=0 skipped=0 timedout=0 crashed=0"
        test "${PROJECT_FILE}" --filter "*::orderedPair#*" --timeout 5s
)
run_testing_reference_case(
        1
        "test-result: passed=0 failed=1 skipped=0 timedout=0 crashed=0"
        test "${reference_root}/negative/failure.zr" --timeout 5s
)
run_testing_reference_case(
        1
        "test-result: passed=0 failed=0 skipped=0 timedout=1 crashed=0"
        test "${reference_root}/negative/timeout.zr" --timeout 100ms
)
