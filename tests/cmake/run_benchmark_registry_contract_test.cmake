foreach (required_variable IN ITEMS REGISTRY_FILE FIXTURE_SCRIPT)
    if (NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif ()
endforeach ()

execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DREGISTRY_FILE=${REGISTRY_FILE}"
                -DMODE=valid
                -P "${FIXTURE_SCRIPT}"
        RESULT_VARIABLE valid_result
        OUTPUT_VARIABLE valid_stdout
        ERROR_VARIABLE valid_stderr)
if (NOT valid_result EQUAL 0)
    message(FATAL_ERROR "valid benchmark registry contract failed:\n${valid_stdout}${valid_stderr}")
endif ()

foreach (invalid_mode IN ITEMS zero negative non_integer)
    execute_process(
            COMMAND "${CMAKE_COMMAND}"
                    "-DREGISTRY_FILE=${REGISTRY_FILE}"
                    "-DMODE=${invalid_mode}"
                    -P "${FIXTURE_SCRIPT}"
            RESULT_VARIABLE invalid_result
            OUTPUT_VARIABLE invalid_stdout
            ERROR_VARIABLE invalid_stderr)
    if (invalid_result EQUAL 0)
        message(FATAL_ERROR "${invalid_mode} MIN_SAMPLE_MS unexpectedly succeeded")
    endif ()
    if (NOT "${invalid_stdout}${invalid_stderr}" MATCHES
            "MIN_SAMPLE_MS to be a positive integer")
        message(FATAL_ERROR
                "${invalid_mode} MIN_SAMPLE_MS failed without the contract diagnostic:\n${invalid_stdout}${invalid_stderr}")
    endif ()
endforeach ()

message(STATUS "Benchmark registry MIN_SAMPLE_MS contract PASS")
