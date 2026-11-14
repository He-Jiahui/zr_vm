function(expect_result name source expected)
    execute_process(
            COMMAND "${ZR_VM_CLI}" -e "${source}"
            RESULT_VARIABLE status OUTPUT_VARIABLE result ERROR_VARIABLE diagnostic
            OUTPUT_STRIP_TRAILING_WHITESPACE TIMEOUT 15)
    if (NOT status EQUAL 0 OR NOT result STREQUAL expected)
        message(SEND_ERROR "${name}: expected ${expected}, exit=${status}, output=${result}\n${diagnostic}")
    endif ()
endfunction()

function(expect_rejection name source expected)
    execute_process(
            COMMAND "${ZR_VM_CLI}" -e "${source}"
            RESULT_VARIABLE status OUTPUT_VARIABLE result ERROR_VARIABLE diagnostic
            TIMEOUT 15)
    if (NOT status EQUAL 1 OR NOT "${result}${diagnostic}" MATCHES "${expected}")
        message(SEND_ERROR "${name}: expected ${expected}, exit=${status}\n${result}${diagnostic}")
    endif ()
endfunction()

expect_result(omitted_argument [=[
fn f(a: int = 7): int { return a; }
return f();
]=] 7)
expect_result(all_defaults [=[
fn f(a: int = 7, b: int = 2): int { return a * b; }
return f();
]=] 14)
expect_result(default_overridden [=[
fn f(a: int = 7): int { return a; }
return f(11);
]=] 11)
expect_result(explicit_null [=[
fn f(a = 7) { return a; }
return f(null);
]=] null)
expect_result(named_argument_keeps_other_default [=[
fn f(a: int = 7, b: int = 2): int { return a * b; }
return f(b: 3);
]=] 21)
expect_result(single_rest_argument [=[
fn f(...params: int[]): int { return params[0] + params.length; }
let result = f(41);
return result;
]=] 42)
expect_result(rest_only [=[
fn f(...params: int[]): int { return params[0] + params[1]; }
return f(20, 22);
]=] 42)
expect_result(fixed_prefix_and_rest [=[
fn f(first: int, ...params: int[]): int { return first + params[0] * params[1]; }
return f(2, 5, 8);
]=] 42)
expect_result(empty_rest_and_default [=[
fn f(first: int = 7, ...params: int[]): int { return first + params.length; }
return f();
]=] 7)
expect_result(spread_into_rest [=[
fn f(first: int, ...params: int[]): int { return first + params[0] + params[1]; }
let values: int[] = [20, 21];
return f(1, ...values);
]=] 42)
expect_result(empty_spread [=[
fn f(...params: int[]): int { return params.length; }
return f(...[]);
]=] 0)
expect_result(fresh_rest_array [=[
fn f(...params: int[]): int[] { return params; }
let a = f(41);
let b = f(12);
a[0] = 30;
return a[0] + b[0];
]=] 42)
string(REPEAT "1, " 256 tail_arguments)
expect_result(large_rest_argument_list
        "fn f(...params: int[]): int { return params.length + params[256]; } return f(${tail_arguments} 3);"
        260)
expect_rejection(rest_must_be_array [=[
fn f(...params: int): int { return 0; }
return 42;
]=] "Variadic parameter must have an array type")
expect_rejection(rest_must_be_final [=[
fn f(first: int, ...params: int[], last: int): int { return first; }
return 42;
]=] "Variadic parameter must be last")
expect_rejection(rest_must_be_unsized [=[
fn f(...params: int[2]): int { return 0; }
return 42;
]=] "Variadic parameter requires an unsized array type")
expect_rejection(rest_has_no_default [=[
fn f(...params: int[] = [1]): int { return 0; }
return 42;
]=] "Variadic parameter cannot have a default value")
expect_rejection(rest_element_must_match [=[
fn f(...params: int[]): int { return params.length; }
return f("wrong");
]=] "No matching overload for function 'f'")
