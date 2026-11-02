# Every invocation owns a project, including a compile-only mode and a binary
# mode which hides the source before execution.
file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
if(DEFINED SUPPORT_SOURCE_DIR)
    file(COPY "${SUPPORT_SOURCE_DIR}/" DESTINATION "${TEST_WORK_DIR}/src")
endif()
configure_file("${SOURCE_FILE}" "${TEST_WORK_DIR}/src/main.zr" COPYONLY)
set(project "${TEST_WORK_DIR}/probe.zrp")
file(WRITE "${project}" "{\"name\":\"inline_struct_regression\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n")

if(EXECUTION_MODE STREQUAL "compile" OR EXECUTION_MODE STREQUAL "binary")
    execute_process(COMMAND "${CLI_EXE}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
    if(NOT status EQUAL 0 OR NOT EXISTS "${TEST_WORK_DIR}/bin/main.zro")
        message(FATAL_ERROR "Inline struct compile failed (${status}): ${output}\n${error}")
    endif()
    if(EXECUTION_MODE STREQUAL "compile")
        return()
    endif()
    file(RENAME "${TEST_WORK_DIR}/src" "${TEST_WORK_DIR}/source-hidden")
endif()

execute_process(COMMAND "${CLI_EXE}" "${project}" --execution-mode "${EXECUTION_MODE}" --emit-executed-via
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
if(NOT status EQUAL 0 OR NOT output MATCHES "executed_via=${EXECUTION_MODE}")
    message(FATAL_ERROR "Inline struct ${EXECUTION_MODE} failed (${status}): ${output}\n${error}")
endif()
