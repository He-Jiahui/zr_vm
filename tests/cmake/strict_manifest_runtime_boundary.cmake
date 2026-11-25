file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
file(WRITE "${TEST_WORK_DIR}/src/main.zr" "return 6 * 7;\n")
set(manifest "${TEST_WORK_DIR}/strict.zrp")
set(envelope [=[{"manifestVersion":3,"name":"strict_probe","version":"1.0.0","kind":"app","source":"src","binary":"bin","entry":"main"]=])

file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_PROJECT_STARTUP=1 "${CLI_EXE}"
        "${manifest}" --emit-executed-via
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
string(REPLACE "\r\n" "\n" output "${output}")
string(STRIP "${output}" output)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "42\nexecuted_via=strict-interp" OR
        error MATCHES "\\[zr-common-state\\] global=")
    message(FATAL_ERROR "Strict source execution failed (${status}): ${output}\n${error}")
endif()

execute_process(COMMAND "${CLI_EXE}" --require-strict-no-gc --compile "${manifest}" --emit-aot-c
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
set(binary "${TEST_WORK_DIR}/bin/main.zro")
set(aot_source "${TEST_WORK_DIR}/bin/aot_c/src/main.c")
if(NOT status STREQUAL "0" OR NOT EXISTS "${binary}" OR NOT EXISTS "${aot_source}")
    message(FATAL_ERROR "Strict compilation failed (${status}): ${output}\n${error}")
endif()

file(RENAME "${TEST_WORK_DIR}/src/main.zr" "${TEST_WORK_DIR}/main.hidden")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_CORE_BOOTSTRAP=1 ZR_VM_TRACE_PROJECT_STARTUP=1
        "${CLI_EXE}" "${manifest}" --execution-mode binary --emit-executed-via
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
string(REPLACE "\r\n" "\n" output "${output}")
string(STRIP "${output}" output)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "42\nexecuted_via=strict-binary" OR NOT error STREQUAL "")
    message(FATAL_ERROR "Strict binary needed source or a managed bootstrap (${status}): ${output}\n${error}")
endif()

set(aot_exe "${TEST_WORK_DIR}/strict-native${EXE_SUFFIX}")
if(MSVC)
    set(compile_args /nologo /TC /std:c11 "${MSVC_CRT}" "/I${STRICT_INCLUDE}" "${aot_source}" "${STRICT_ARCHIVE}"
            "/Fe${aot_exe}" "/Fo${TEST_WORK_DIR}/strict-native.obj")
else()
    set(compile_args -std=c11 "-I${STRICT_INCLUDE}" "${aot_source}" "${STRICT_ARCHIVE}" -o "${aot_exe}")
endif()
execute_process(COMMAND "${C_COMPILER}" ${compile_args} WORKING_DIRECTORY "${TEST_WORK_DIR}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Strict AOT C did not link against only the strict archive (${status}): ${output}\n${error}")
endif()
execute_process(COMMAND "${aot_exe}" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
string(STRIP "${output}" output)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "42" OR NOT error STREQUAL "")
    message(FATAL_ERROR "Strict AOT C and interpreter disagree (${status}): ${output}\n${error}")
endif()
file(RENAME "${TEST_WORK_DIR}/main.hidden" "${TEST_WORK_DIR}/src/main.zr")

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

execute_process(COMMAND "${CLI_EXE}" --compile "${manifest}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Could not build the managed binary control (${status}): ${output}\n${error}")
endif()
file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_CORE_BOOTSTRAP=1 ZR_VM_TRACE_PROJECT_STARTUP=1
        "${CLI_EXE}" "${manifest}" --execution-mode binary
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status STREQUAL "1" OR NOT error MATCHES "not a StrictNoGC executable" OR
        error MATCHES "\\[zr-(core|common-state)")
    message(FATAL_ERROR "Strict loader admitted a managed binary (${status}): ${output}\n${error}")
endif()

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
