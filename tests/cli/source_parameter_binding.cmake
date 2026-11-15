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

expect_result(generic_rest_inferred [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
return f(1, 20, 22);
]=] 2)
expect_result(generic_rest_explicit [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
return f<int>(1, 20, 22);
]=] 2)
expect_result(generic_rest_only_inferred [=[
fn f<T>(...params: T[]): int { return params.length; }
return f(20, 22);
]=] 2)
expect_result(generic_rest_only_explicit [=[
fn f<T>(...params: T[]): int { return params.length; }
return f<int>(20, 22);
]=] 2)
expect_result(generic_rest_dynamic_spread [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
let values: int[] = [20, 22];
return f(1, ...values);
]=] 2)
expect_result(generic_rest_only_dynamic_spread [=[
fn f<T>(...params: T[]): int { return params.length; }
let values: int[] = [20, 22];
return f(...values);
]=] 2)
expect_result(typed_spread_only_fills_fixed_parameters [=[
fn f(first: int, ...params: string[]): int { return first + params.length; }
let values: int[1] = [42];
return f(...values);
]=] 42)
expect_result(generic_rest_explicit_empty [=[
fn f<T>(...params: T[]): int { return params.length; }
return f<int>();
]=] 0)
expect_result(generic_rest_explicit_empty_spread [=[
fn f<T>(...params: T[]): int { return params.length; }
return f<int>(...[]);
]=] 0)
expect_result(generic_rest_inferred_empty_tail [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
return f(42);
]=] 0)
expect_result(generic_rest_respects_class_constraint [=[
class Item {}
fn f<T>(...params: T[]): int where T: class { return params.length; }
return f(new Item());
]=] 1)
expect_result(generic_rest_keeps_declaration_type_parameter [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
fn run(): int {
    let T = string[];
    let result = f<int>(1, 2) + f(1, 2);
    let values: T = ["alias is still available"];
    return result + values.length;
}
return run();
]=] 3)
expect_result(generic_body_keeps_local_type_alias [=[
fn f<T>(first: T, ...params: T[]): int {
    let T = string[];
    let values: T = ["body alias remains available"];
    return params.length + values.length;
}
return f<int>(1, 2);
]=] 2)
# These signatures must keep declaration types, not caller-visible aliases.
expect_result(rest_nominal_type_survives_caller_alias [=[
class Item {}
fn f(...params: Item[]): int { return params.length; }
fn run(item: Item): int {
    let Item = string[];
    return f(item);
}
return run(new Item());
]=] 1)
expect_result(rest_nominal_spread_survives_caller_alias [=[
class Item {}
fn f(...params: Item[]): int { return params.length; }
fn run(items: Item[]): int {
    let Item = string[];
    return f(...items);
}
return run([new Item()]);
]=] 1)
expect_result(generic_rest_concrete_type_survives_caller_alias [=[
class Item {}
fn f<T>(first: T, ...params: Item[]): int { return params.length; }
fn run(item: Item): int {
    let Item = string[];
    return f(1, item);
}
return run(new Item());
]=] 1)
expect_result(rest_keeps_declaration_type_alias [=[
let Element = string[];
fn f(...params: Element[]): int { return params.length; }
fn run(): int {
    let Element = int[];
    return f(["declaration alias"]);
}
return run();
]=] 1)
expect_result(generic_rest_keeps_concrete_declaration_alias [=[
let Element = string[];
fn f<T>(first: T, ...params: Element[]): int { return params.length; }
fn run(): int {
    let Element = int[];
    return f(1, ["declaration alias"]);
}
return run();
]=] 1)
expect_rejection(rest_caller_alias_cannot_replace_nominal_contract [=[
class Item {}
fn f(...params: Item[]): int { return params.length; }
fn run(): int {
    let Item = string[];
    return f(["not an Item"]);
}
return run();
]=] "No matching overload for function 'f'")
expect_rejection(rest_spread_caller_alias_cannot_replace_nominal_contract [=[
class Item {}
fn f(...params: Item[]): int { return params.length; }
fn run(): int {
    let Item = string[];
    let values: string[][] = [["not an Item"]];
    return f(...values);
}
return run();
]=] "No matching overload for function 'f'")
expect_rejection(generic_rest_caller_alias_cannot_replace_concrete_contract [=[
class Item {}
fn f<T>(first: T, ...params: Item[]): int { return params.length; }
fn run(): int {
    let Item = string[];
    return f(1, ["not an Item"]);
}
return run();
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(rest_caller_alias_cannot_replace_declaration_alias [=[
let Element = string[];
fn f(...params: Element[]): int { return params.length; }
fn run(): int {
    let Element = int[];
    return f([1]);
}
return run();
]=] "No matching overload for function 'f'")
expect_rejection(generic_rest_caller_alias_cannot_replace_declaration_alias [=[
let Element = string[];
fn f<T>(first: T, ...params: Element[]): int { return params.length; }
fn run(): int {
    let Element = int[];
    return f(1, [1]);
}
return run();
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(generic_rest_empty_requires_type_argument [=[
fn f<T>(...params: T[]): int { return params.length; }
return f();
]=] "cannot infer generic argument 'T'")
expect_rejection(generic_rest_rejects_class_constraint_violation [=[
fn f<T>(...params: T[]): int where T: class { return params.length; }
return f(1);
]=] "does not satisfy class constraint")
expect_rejection(generic_rest_checks_constraints_for_empty_tail [=[
fn f<T>(...params: T[]): int where T: class { return params.length; }
return f<int>();
]=] "does not satisfy class constraint")

expect_rejection(generic_rest_conflicting_inference [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
return f(1, "wrong");
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(generic_rest_conflicting_explicit_type [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
return f<int>(1, "wrong");
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(generic_rest_only_conflicting_inference [=[
fn f<T>(...params: T[]): int { return params.length; }
return f(1, "wrong");
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(generic_rest_conflicting_spread [=[
fn f<T>(first: T, ...params: T[]): int { return params.length; }
let values: string[] = ["wrong"];
return f(1, ...values);
]=] "Generic function 'f'|No matching overload for function 'f'")
expect_rejection(rest_elements_require_no_conversion [=[
fn f(...params: float[]): int { return params.length; }
return f(1);
]=] "No matching overload for function 'f'")
expect_rejection(rest_spread_elements_require_no_conversion [=[
fn f(...params: float[]): int { return params.length; }
let values: int[] = [1];
return f(...values);
]=] "No matching overload for function 'f'")
expect_rejection(rest_parenthesized_ref_must_be_by_value [=[
fn f(...params: (ref int[])): int { return 0; }
return 42;
]=] "Variadic parameter must be passed by value")
expect_rejection(rest_parenthesized_readonly_ref_must_be_by_value [=[
fn f(...params: (ref readonly int[])): int { return 0; }
return 42;
]=] "Variadic parameter must be passed by value")
expect_rejection(rest_parenthesized_scoped_ref_must_be_by_value [=[
fn f(...params: (scoped ref int[])): int { return 0; }
return 42;
]=] "Variadic parameter must be passed by value")
