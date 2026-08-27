if (NOT DEFINED PROBE_EXE OR PROBE_EXE STREQUAL "")
    message(FATAL_ERROR "PROBE_EXE is required")
endif ()

execute_process(
        COMMAND "${PROBE_EXE}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_stdout
        ERROR_VARIABLE probe_stderr
)

set(probe_output "${probe_stdout}\n${probe_stderr}")

if (NOT probe_result EQUAL 1)
    message(FATAL_ERROR
            "ZR_TEST_FAIL probe must return exactly one Unity failure; result=${probe_result}\n${probe_output}")
endif ()

if (NOT probe_output MATCHES "1 Tests 1 Failures 0 Ignored")
    message(FATAL_ERROR "ZR_TEST_FAIL probe did not report exactly one Unity failure\n${probe_output}")
endif ()

if (NOT probe_output MATCHES "ZR_TEST_FAIL_PROBE cleanup_reached=1 unity_exit=1")
    message(FATAL_ERROR "ZR_TEST_FAIL skipped cleanup after marking Unity failed\n${probe_output}")
endif ()

message(STATUS "ZR_TEST_FAIL reports one failure and preserves cleanup control flow")
