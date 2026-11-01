# Runtime expected failures from the GD-H2 diagnosis (R3/R4). Compilation is a
# required success; runtime is explicitly skipped only after both the direct
# initializer and its explicit-local control fail with the expected diagnostic.
file(REMOVE_RECURSE "${TEST_WORK_DIR}")
foreach(variant original control)
    set(work_dir "${TEST_WORK_DIR}/${variant}")
    file(MAKE_DIRECTORY "${work_dir}/src")
    if(variant STREQUAL "original")
        set(source "${SOURCE_FILE}")
    else()
        set(source "${CONTROL_SOURCE_FILE}")
    endif()
    configure_file("${source}" "${work_dir}/src/main.zr" COPYONLY)
    set(project "${work_dir}/probe.zrp")
    file(WRITE "${project}" "{\"name\":\"inline_struct_known_failure\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}\n")

    execute_process(COMMAND "${CLI_EXE}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
    file(WRITE "${work_dir}/compile.log" "${output}\n${error}")
    if(NOT status EQUAL 0 OR NOT EXISTS "${work_dir}/bin/main.zro" OR
       "${output}\n${error}" MATCHES "Failed to build.*frame layout metadata")
        message(FATAL_ERROR "${DEFECT} ${variant} compilation failed (${status}): ${output}\n${error}")
    endif()
    if(EXECUTION_MODE STREQUAL "compile")
        continue()
    endif()
    if(EXECUTION_MODE STREQUAL "binary")
        file(RENAME "${work_dir}/src" "${work_dir}/source-hidden")
    endif()

    execute_process(COMMAND "${CLI_EXE}" "${project}" --execution-mode "${EXECUTION_MODE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 40)
    set(runtime_output "${output}\n${error}")
    file(WRITE "${work_dir}/runtime.log" "${runtime_output}")
    string(FIND "${runtime_output}" "Error: ${EXPECTED_DIAGNOSTIC}" diagnostic_position)
    if(NOT "${status}" STREQUAL "1" OR diagnostic_position LESS 0 OR
       runtime_output MATCHES "Failed to build.*frame layout metadata")
        message(FATAL_ERROR "${DEFECT} ${variant} had an unexpected runtime outcome (${status}): ${runtime_output}")
    endif()
    string(REGEX MATCH "Error: [^\r\n]+" runtime_diagnostic "${runtime_output}")
    if(variant STREQUAL "original")
        set(original_diagnostic "${runtime_diagnostic}")
    elseif(NOT runtime_diagnostic STREQUAL original_diagnostic)
        message(FATAL_ERROR "${DEFECT} original/control diagnostics differ: ${original_diagnostic} / ${runtime_diagnostic}")
    endif()
endforeach()
if(NOT EXECUTION_MODE STREQUAL "compile")
    message("XFAIL ${DEFECT}: ${EXPECTED_DIAGNOSTIC}; original and explicit-local control agree")
endif()
