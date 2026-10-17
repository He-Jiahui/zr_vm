function(expect_result source expected)
    execute_process(COMMAND "${CLI_EXE}" -e "${source}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
    string(STRIP "${output}" output)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "${expected}")
        message(FATAL_ERROR "Native lambda loop failed (${result}): ${output}\n${error}")
    endif()
endfunction()

set(job "new task.Job<int>(fn() => { var i: int = 0; while (i < 3) { i = i + 1; } return i; })")
expect_result("let task = import(\"zr.task\"); var a = ${job}; var b = ${job}; var x = task.currentScheduler.schedule<int>(a); return x.result();" 3)
expect_result("let task = import(\"zr.task\"); var k: int = 0; var total: int = 0; while (k < 2) { var j = ${job}; var t = task.currentScheduler.schedule<int>(j); total = total + t.result(); k = k + 1; } return total;" 6)
expect_result("var first = fn() => { var i: int = 0; while (i < 2) { i = i + 1; } return i; }; var second = fn() => { var i: int = 0; while (i < 3) { i = i + 1; } return i; }; return first() + second();" 5)
