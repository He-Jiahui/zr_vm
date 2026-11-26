file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/src")
set(manifest "${TEST_WORK_DIR}/lint.zrp")
set(envelope [=[{"manifestVersion":3,"name":"lint_probe","version":"1.0.0","kind":"app","source":"src","binary":"bin","entry":"main"]=])
set(main "${TEST_WORK_DIR}/src/main.zr")
set(bin "${TEST_WORK_DIR}/bin/main.zro")
file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
file(WRITE "${main}" "return 42;\n")

function(check_lint mode expected_status expected_diagnostic)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env ZR_VM_TRACE_STRICT_EXECUTION=1
            "${CLI_EXE}" "${mode}" "${manifest}" ${ARGN}
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
    if(NOT status STREQUAL "${expected_status}" OR NOT output STREQUAL "" OR
            error MATCHES "\\[zr-strict-execution\\]" OR
            (NOT expected_diagnostic STREQUAL "" AND NOT error MATCHES "${expected_diagnostic}"))
        message(FATAL_ERROR "Lint expected ${expected_status} / '${expected_diagnostic}', got ${status}:\n${output}\n${error}")
    endif()
endfunction()

check_lint(lint 0 "")
file(GLOB_RECURSE project_files LIST_DIRECTORIES TRUE RELATIVE "${TEST_WORK_DIR}" "${TEST_WORK_DIR}/*")
if(NOT project_files STREQUAL "lint.zrp;src;src/main.zr")
    message(FATAL_ERROR "Lint changed the contents of the project directory: ${project_files}")
endif()

# A stale artifact must neither satisfy lint nor be overwritten.
file(MAKE_DIRECTORY "${TEST_WORK_DIR}/bin")
file(WRITE "${bin}" "stale main binary")
file(WRITE "${main}" "let { helper } = import(\".dep\");\nreturn helper();\n")
file(WRITE "${TEST_WORK_DIR}/src/dep.zr" "let { leaf } = import(\".leaf\");\npub fn helper(): int { return leaf(); }\n")
set(leaf "${TEST_WORK_DIR}/src/leaf.zr")
file(WRITE "${leaf}" "pub fn leaf(): int { return 42; }\n")
check_lint(--lint 0 "" --require-strict-no-gc)
file(APPEND "${leaf}" [=[
fn unused(): int { if (false) { let bad = new Student(); } return 0; }
]=])
check_lint(lint 1 "leaf\\.zr:2:[0-9]+: error strict_no_gc_new")
file(WRITE "${leaf}" [=[
pub fn leaf(): int { return 42; }
fn unused(): int { let student = own Student(); let managed = intoGc(student); return 0; }
]=])
check_lint(lint 1 "leaf\\.zr:2:[0-9]+: error strict_no_gc_into_gc")
file(REMOVE "${leaf}")
file(WRITE "${TEST_WORK_DIR}/bin/leaf.zro" "unverified dependency binary")
check_lint(lint 1 "StrictNoGC requires verifiable source for import.*leaf")

file(WRITE "${main}" [=[
resource class Student { pub @constructor() {} }
let student = own Student();
let owner = share(student);
drop(student);
return 0;
]=])
check_lint(lint 1 "ownership analysis: use after move or drop")
file(READ "${bin}" saved_binary)
if(NOT saved_binary STREQUAL "stale main binary")
    message(FATAL_ERROR "Lint overwrote an existing binary")
endif()

check_lint(lint 1 "lint cannot be combined with run or build options" --execution-mode binary)
check_lint(lint 1 "lint cannot be combined with run or build options" --execution-mode interp)
check_lint(lint 1 "lint cannot be combined with run or build options" --emit-aot-c)
file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":false}\n")
check_lint(lint 1 "requires StrictNoGC: true")
