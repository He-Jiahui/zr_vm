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

execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_PROJECT_STARTUP=1 "${CLI_EXE}"
        --require-strict-no-gc --compile "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "1" OR NOT error MATCHES "requires StrictNoGC: true" OR
        error MATCHES "\\[zr-common-state\\] global=")
    message(FATAL_ERROR "Strict CI assertion accepted a disabled flag (${status}): ${output}\n${error}")
endif()

file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
file(WRITE "${TEST_WORK_DIR}/src/main.zr" [=[
resource class Student { pub @constructor() {} }
class School { pub static let bad: Student = new Student(); }
fn unused(): int {
    if (false) { let never = new Student(); }
    let owned = own Student();
    let managed = intoGc(owned);
    return 0;
}
return 42;
]=])
execute_process(COMMAND "${CLI_EXE}" --require-strict-no-gc --compile "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
string(REGEX MATCHALL "strict_no_gc_new" new_diagnostics "${error}")
list(LENGTH new_diagnostics new_count)
if(NOT status STREQUAL "1" OR NOT new_count EQUAL 2 OR NOT error MATCHES "strict_no_gc_into_gc" OR
        error MATCHES "requires the strict runtime")
    message(FATAL_ERROR "Strict source check missed a declaration or dead code (${status}): ${output}\n${error}")
endif()

file(WRITE "${TEST_WORK_DIR}/src/main.zr" "let { helper } = import(\".dep\");\nreturn 42;\n")
file(WRITE "${TEST_WORK_DIR}/src/dep.zr" "pub fn helper(): int { let bad = new Student(); return 0; }\n")
execute_process(COMMAND "${CLI_EXE}" --compile "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "1" OR NOT error MATCHES "dep\\.zr:1:[0-9]+: error strict_no_gc_new" OR
        NOT error MATCHES "imported .*dep from .*main")
    message(FATAL_ERROR "Strict source check missed an imported module (${status}): ${output}\n${error}")
endif()

file(REMOVE "${TEST_WORK_DIR}/src/dep.zr")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/bin")
file(WRITE "${TEST_WORK_DIR}/bin/dep.zro" "unverified binary")
execute_process(COMMAND "${CLI_EXE}" --compile "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "1" OR NOT error MATCHES "StrictNoGC requires verifiable source for import")
    message(FATAL_ERROR "Strict source check accepted an unverified binary (${status}): ${output}\n${error}")
endif()
