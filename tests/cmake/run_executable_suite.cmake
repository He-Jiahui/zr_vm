if (NOT DEFINED SUITE_NAME OR SUITE_NAME STREQUAL "")
    set(SUITE_NAME "unnamed_suite")
endif ()

if (NOT DEFINED EXECUTABLES OR EXECUTABLES STREQUAL "")
    message(FATAL_ERROR "No executables were provided for suite '${SUITE_NAME}'.")
endif ()

if (NOT DEFINED RUN_WORKING_DIRECTORY OR RUN_WORKING_DIRECTORY STREQUAL "")
    set(RUN_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif ()

if (DEFINED TIER AND NOT TIER STREQUAL "")
    string(TOLOWER "${TIER}" REQUESTED_TIER)
elseif (DEFINED ENV{ZR_VM_TEST_TIER} AND NOT "$ENV{ZR_VM_TEST_TIER}" STREQUAL "")
    string(TOLOWER "$ENV{ZR_VM_TEST_TIER}" REQUESTED_TIER)
else ()
    set(REQUESTED_TIER "")
endif ()

set(SELECTED_EXECUTABLES "${EXECUTABLES}")
if (REQUESTED_TIER STREQUAL "smoke" AND DEFINED EXECUTABLES_SMOKE AND NOT EXECUTABLES_SMOKE STREQUAL "")
    set(SELECTED_EXECUTABLES "${EXECUTABLES_SMOKE}")
elseif (REQUESTED_TIER STREQUAL "core" AND DEFINED EXECUTABLES_CORE AND NOT EXECUTABLES_CORE STREQUAL "")
    set(SELECTED_EXECUTABLES "${EXECUTABLES_CORE}")
elseif (REQUESTED_TIER STREQUAL "stress" AND DEFINED EXECUTABLES_STRESS AND NOT EXECUTABLES_STRESS STREQUAL "")
    set(SELECTED_EXECUTABLES "${EXECUTABLES_STRESS}")
endif ()

set(CLI_EXE "")
foreach (_zr_suite_exe_candidate IN LISTS SELECTED_EXECUTABLES)
    if (NOT _zr_suite_exe_candidate STREQUAL "")
        set(CLI_EXE "${_zr_suite_exe_candidate}")
        break()
    endif ()
endforeach ()

include("${CMAKE_CURRENT_LIST_DIR}/zr_vm_test_host_env.cmake")

message("==========")
message("Running suite: ${SUITE_NAME}")
if (NOT REQUESTED_TIER STREQUAL "")
    message("Tier filter: ${REQUESTED_TIER}")
endif ()
message("==========")

foreach (executable_path IN LISTS SELECTED_EXECUTABLES)
    if (executable_path STREQUAL "")
        continue()
    endif ()

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
    endif ()

    if (NOT executable_stderr STREQUAL "")
        message("${executable_stderr}")
    endif ()

    if (NOT executable_result EQUAL 0)
        message(FATAL_ERROR "Suite '${SUITE_NAME}' failed while running '${executable_name}' with exit code ${executable_result}.")
    endif ()
endforeach ()
