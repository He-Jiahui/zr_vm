# Each mode owns its project and imported provider artifacts.
file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
file(COPY "${SOURCE_DIR}/" DESTINATION "${TEST_WORK_DIR}/src")
set(project "${TEST_WORK_DIR}/probe.zrp")
file(WRITE "${project}" "{\"name\":\"member_override_regression\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n")

if(EXECUTION_MODE STREQUAL "compile" OR EXECUTION_MODE STREQUAL "binary")
    execute_process(COMMAND "${CLI_EXE}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
    if(NOT status EQUAL 0 OR NOT EXISTS "${TEST_WORK_DIR}/bin/main.zro")
        message(FATAL_ERROR "Member override compile failed (${status}): ${output}\n${error}")
    endif()
    if(EXISTS "${SOURCE_DIR}/base.zr" AND NOT EXISTS "${TEST_WORK_DIR}/bin/base.zro")
        message(FATAL_ERROR "Imported base artifact was not emitted")
    endif()
    if(EXECUTION_MODE STREQUAL "compile")
        return()
    endif()
    file(REMOVE_RECURSE "${TEST_WORK_DIR}/src")
endif()

execute_process(COMMAND "${CLI_EXE}" "${project}" --execution-mode "${EXECUTION_MODE}" --emit-executed-via
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
string(REPLACE "\r\n" "\n" output "${output}")
string(STRIP "${output}" output)
if(NOT status EQUAL 0 OR NOT output STREQUAL "42\nexecuted_via=${EXECUTION_MODE}")
    message(FATAL_ERROR "Member override ${EXECUTION_MODE} failed (${status}): ${output}\n${error}")
endif()
