file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
file(WRITE "${TEST_WORK_DIR}/src/main.zr" "return 42;\n")
set(manifest "${TEST_WORK_DIR}/strict.zrp")
set(envelope [=[{"manifestVersion":3,"name":"strict_probe","version":"1.0.0","kind":"app","source":"src","binary":"bin","entry":"main"]=])

file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
foreach(mode IN ITEMS run compile)
    set(args "${manifest}")
    if(mode STREQUAL "compile")
        list(PREPEND args --compile)
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_PROJECT_STARTUP=1 "${CLI_EXE}" ${args}
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(NOT status STREQUAL "1" OR NOT error MATCHES "StrictNoGC project requires the strict runtime" OR
            error MATCHES "\\[zr-common-state\\] global=")
        message(FATAL_ERROR "Strict project crossed the managed ${mode} boundary (${status}): ${output}\n${error}")
    endif()
endforeach()

file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":false}\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_PROJECT_STARTUP=1 "${CLI_EXE}" "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
string(STRIP "${output}" output)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "42" OR
        NOT error MATCHES "\\[zr-common-state\\] global=")
    message(FATAL_ERROR "Managed v3 project failed (${status}): ${output}\n${error}")
endif()
