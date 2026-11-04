# Module callable controls need all source modules, including in binary mode.
file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
file(COPY "${SOURCE_DIR}/" DESTINATION "${TEST_WORK_DIR}/src")
set(project "${TEST_WORK_DIR}/probe.zrp")
file(WRITE "${project}" "{\"name\":\"e5_callable_regression\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n")

if(EXECUTION_MODE STREQUAL "compile" OR EXECUTION_MODE STREQUAL "binary")
    execute_process(COMMAND "${CLI_EXE}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
    file(WRITE "${TEST_WORK_DIR}/compile.log" "${output}\n${error}")
    if(NOT status EQUAL 0 OR NOT EXISTS "${TEST_WORK_DIR}/bin/main.zro")
        message(FATAL_ERROR "E5 callable compile failed (${status}): ${output}\n${error}")
    endif()
    if(EXECUTION_MODE STREQUAL "compile")
        return()
    endif()
    file(RENAME "${TEST_WORK_DIR}/src" "${TEST_WORK_DIR}/source-hidden")
endif()

execute_process(COMMAND "${CLI_EXE}" "${project}" --execution-mode "${EXECUTION_MODE}" --emit-executed-via
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
file(WRITE "${TEST_WORK_DIR}/runtime.log" "${output}\n${error}")
if(NOT "${status}" STREQUAL "${EXPECTED_EXIT}")
    message(FATAL_ERROR "E5 callable ${EXECUTION_MODE} failed (${status}): ${output}\n${error}")
endif()
if(NOT output MATCHES "executed_via=${EXECUTION_MODE}")
    message(FATAL_ERROR "Missing execution-mode marker: ${output}\n${error}")
endif()
