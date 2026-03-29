if (NOT DEFINED SUITE_NAME OR SUITE_NAME STREQUAL "")
    set(SUITE_NAME "unnamed_suite")
endif()

if (NOT DEFINED EXECUTABLES OR EXECUTABLES STREQUAL "")
    message(FATAL_ERROR "No executables were provided for suite '${SUITE_NAME}'.")
endif()

if (NOT DEFINED RUN_WORKING_DIRECTORY OR RUN_WORKING_DIRECTORY STREQUAL "")
    set(RUN_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()

message("==========")
message("Running suite: ${SUITE_NAME}")
message("==========")

foreach(executable_path IN LISTS EXECUTABLES)
    if (executable_path STREQUAL "")
        continue()
    endif()

    get_filename_component(executable_name "${executable_path}" NAME)
    message("---- ${executable_name}")

    execute_process(
        COMMAND "${executable_path}"
        WORKING_DIRECTORY "${RUN_WORKING_DIRECTORY}"
        RESULT_VARIABLE executable_result
        OUTPUT_VARIABLE executable_stdout
        ERROR_VARIABLE executable_stderr
    )

    if (NOT executable_stdout STREQUAL "")
        message("${executable_stdout}")
    endif()

    if (NOT executable_stderr STREQUAL "")
        message("${executable_stderr}")
    endif()

    if (NOT executable_result EQUAL 0)
        message(FATAL_ERROR "Suite '${SUITE_NAME}' failed while running '${executable_name}' with exit code ${executable_result}.")
    endif()
endforeach()
