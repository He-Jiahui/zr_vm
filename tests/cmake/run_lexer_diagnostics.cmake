execute_process(COMMAND "${TEST_EXECUTABLE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "Lexer diagnostic regression failed: ${result}\n${output}\n${errors}")
endif ()
if (NOT output STREQUAL "" OR NOT errors STREQUAL "")
    message(FATAL_ERROR "Lexer wrote outside its token diagnostic sink:\n${output}\n${errors}")
endif ()
