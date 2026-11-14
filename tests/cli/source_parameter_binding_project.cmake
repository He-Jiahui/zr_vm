file(MAKE_DIRECTORY "${ZR_VM_TEST_WORK_DIR}/src")
set(project "${ZR_VM_TEST_WORK_DIR}/parameters.zrp")
file(WRITE "${project}" [=[
{"name":"parameter_binding","source":"src","binary":"bin","entry":"main"}
]=])
file(WRITE "${ZR_VM_TEST_WORK_DIR}/src/main.zr" [=[
fn withDefault(value: int = 7): int { return value; }
fn count(first: int = 7, ...params: int[]): int {
    return first + params.length;
}
let tail: int[] = [20, 22];
return withDefault() + count() + count(26, ...tail);
]=])

function(expect_project_result mode)
    execute_process(
            COMMAND "${ZR_VM_CLI}" "${project}" --execution-mode "${mode}" --emit-executed-via
            RESULT_VARIABLE status OUTPUT_VARIABLE result ERROR_VARIABLE diagnostic
            OUTPUT_STRIP_TRAILING_WHITESPACE TIMEOUT 25)
    string(REPLACE "\r\n" "\n" result "${result}")
    if (NOT status EQUAL 0 OR NOT result STREQUAL "42\nexecuted_via=${mode}")
        message(SEND_ERROR "${mode} project call: exit=${status}, output=${result}\n${diagnostic}")
    endif ()
endfunction()

expect_project_result(interp)
execute_process(
        COMMAND "${ZR_VM_CLI}" --compile "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE result ERROR_VARIABLE diagnostic TIMEOUT 45)
if (NOT status EQUAL 0 OR NOT EXISTS "${ZR_VM_TEST_WORK_DIR}/bin/main.zro")
    message(FATAL_ERROR "Project compile: exit=${status}, output=${result}\n${diagnostic}")
endif ()
expect_project_result(binary)
