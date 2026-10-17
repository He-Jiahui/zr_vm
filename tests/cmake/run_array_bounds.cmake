function(expect_bounds_error name source)
    execute_process(COMMAND "${CLI_EXE}" -e "${source}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
    if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "Array index out of range")
        message(FATAL_ERROR "${name}: expected a bounds error (${result}): ${output}\n${error}")
    endif()
endfunction()

foreach(index IN ITEMS -1 2 9223372036854775807)
    expect_bounds_error("int read ${index}" "var a: int[] = [1, 2]; return a[${index}];")
    expect_bounds_error("int write ${index}" "var a: int[] = [1, 2]; a[${index}] = 3; return a[0];")
    expect_bounds_error("string read ${index}" "var a: string[] = [\"a\", \"b\"]; return a[${index}];")
    expect_bounds_error("string write ${index}" "var a: string[] = [\"a\", \"b\"]; a[${index}] = \"c\"; return a[0];")
    expect_bounds_error("native read ${index}"
            "let c = import(\"zr.container\"); var a = new c.Array<int>(); a.add(1); a.add(2); return a[${index}];")
    expect_bounds_error("native loop read ${index}"
            "let c = import(\"zr.container\"); var a = new c.Array<int>(); a.add(1); a.add(2); var s: int = 0; var j: int = 0; while (j < 2) { s = s + a[${index}]; j = j + 1; } return s;")
    expect_bounds_error("native write ${index}"
            "let c = import(\"zr.container\"); var a = new c.Array<int>(); a.add(1); a.add(2); a[${index}] = 3; return a[0];")
endforeach()
expect_bounds_error("empty read" "var a = []; return a[0];")
expect_bounds_error("empty write" "var a = []; a[0] = 1; return 0;")

execute_process(COMMAND "${CLI_EXE}" -e
        "var a: int[] = [1, 2]; a[0] = 3; return a[0] + a[1];"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
string(STRIP "${output}" output)
if(NOT result EQUAL 0 OR NOT output STREQUAL "5")
    message(FATAL_ERROR "Valid array bounds failed (${result}): ${output}\n${error}")
endif()

foreach(source IN ITEMS
        "var a: string[] = [\"a\", \"b\"]; a[1] = \"c\"; return a[1];"
        "let c = import(\"zr.container\"); var a = new c.Array<string>(); a.add(\"a\"); a.add(\"b\"); a[1] = \"c\"; return a[1];")
    execute_process(COMMAND "${CLI_EXE}" -e "${source}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
    string(STRIP "${output}" output)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "c")
        message(FATAL_ERROR "Valid string array access failed (${result}): ${output}\n${error}")
    endif()
endforeach()
