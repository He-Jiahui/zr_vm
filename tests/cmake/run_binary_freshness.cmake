if(NOT DEFINED CLI_EXE OR NOT DEFINED TEST_WORK_DIR)
    message(FATAL_ERROR "CLI_EXE and TEST_WORK_DIR are required")
endif()
set(root "${TEST_WORK_DIR}/cli_binary_freshness")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}/src" "${root}/bin")
set(project "${root}/fresh.zrp")
file(WRITE "${project}" "{\"name\":\"fresh\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n")

function(compile_project)
    execute_process(COMMAND "${CLI_EXE}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Compile failed: ${status}\n${output}\n${error}")
    endif()
endfunction()

function(expect_run value via)
    execute_process(COMMAND "${CLI_EXE}" ${ARGN} --execution-mode binary --emit-executed-via
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
    string(REPLACE "\r\n" "\n" output "${output}")
    string(STRIP "${output}" output)
    if(NOT status EQUAL 0 OR NOT output STREQUAL "${value}\nexecuted_via=${via}")
        message(FATAL_ERROR "Expected ${value} via ${via}; got status=${status}\n${output}\n${error}")
    endif()
endfunction()

file(WRITE "${root}/src/main.zr" "return 7;\n")
compile_project()
expect_run(7 binary "${project}")

# A newer binary mtime must not hide a source content change.
file(WRITE "${root}/src/main.zr" "return 8;\n")
file(TOUCH "${root}/bin/main.zro")
expect_run(8 interp "${project}")
file(REMOVE "${root}/bin/main.zro")
expect_run(8 interp "${project}")
file(WRITE "${root}/src/other.zr" "return 9;\n")
expect_run(9 interp --project "${project}" -m other)

compile_project()
expect_run(8 binary "${project}")
file(REMOVE "${root}/src/main.zr")
expect_run(8 binary "${project}")

# Imported modules use the same freshness rule even when the entry is current.
file(WRITE "${root}/src/main.zr" "let valueModule = import(\"value\"); return valueModule.answer();\n")
file(WRITE "${root}/src/value.zr" "pub fn answer(): int { return 11; }\n")
compile_project()
expect_run(11 binary "${project}")
file(WRITE "${root}/src/value.zr" "pub fn answer(): int { return 13; }\n")
file(TOUCH "${root}/bin/value.zro")
expect_run(13 binary "${project}")
compile_project()
expect_run(13 binary "${project}")
file(REMOVE "${root}/src/main.zr" "${root}/src/value.zr")
expect_run(13 binary "${project}")

# A corrupt artifact is still an error, including when matching source exists.
file(WRITE "${root}/src/main.zr" "return 1;\n")
file(WRITE "${root}/bin/main.zro" "invalid binary")
execute_process(COMMAND "${CLI_EXE}" "${project}" --execution-mode binary --emit-executed-via
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
if(status EQUAL 0 OR output MATCHES "executed_via=")
    message(FATAL_ERROR "Corrupt artifact was accepted: ${output}\n${error}")
endif()
