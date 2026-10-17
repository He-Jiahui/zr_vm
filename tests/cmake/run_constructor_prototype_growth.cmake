file(READ "${SOURCE_FILE}" source)
execute_process(COMMAND "${CLI_EXE}" -e "${source}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30)
string(STRIP "${output}" output)
if(NOT result EQUAL 0 OR NOT output STREQUAL "7")
    message(FATAL_ERROR "Constructor prototype growth failed (${result}): ${output}\n${error}")
endif()
