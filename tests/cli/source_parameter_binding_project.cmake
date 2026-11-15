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
fn genericCount<T>(first: T, ...params: T[]): int { return params.length; }
fn tailCount<T>(...params: T[]): int { return params.length; }
fn onlyFixed(first: int, ...params: string[]): int { return first + params.length; }
fn keep(...params: int[]): int[] { return params; }
fn scopedCount(): int {
    let T = string[];
    let result = genericCount<int>(1, 20, 22);
    let values: T = ["caller alias"];
    return result + values.length;
}
class Item {}
let Element = string[];
fn nominalCount(...items: Item[]): int { return items.length; }
fn genericNominalCount<T>(first: T, ...items: Item[]): int { return items.length; }
fn aliasCount(...items: Element[]): int { return items.length; }
fn genericAliasCount<T>(first: T, ...items: Element[]): int { return items.length; }
fn declarationScopedCount(item: Item, items: Item[]): int {
    let Item = string[];
    let Element = int[];
    return nominalCount(item) + nominalCount(...items) + genericNominalCount(1, item)
        + aliasCount(["declared"]) + genericAliasCount(1, ["declared"]);
}
if (declarationScopedCount(new Item(), [new Item()]) != 5) {
    throw "caller aliases must not replace rest declaration types";
}
let tail: int[] = [20, 22];
let fixed: int[1] = [42];
if (genericCount(1, 20, 22) != 2 || genericCount<int>(1, ...tail) != 2 || scopedCount() != 3) {
    throw "generic fixed-prefix rest binding failed";
}
if (tailCount(20, 22) != 2 || tailCount<int>(...tail) != 2) {
    throw "generic rest-only binding failed";
}
if (onlyFixed(...fixed) != 42 || onlyFixed(...[42]) != 42) {
    throw "fixed-only spread must leave an empty rest array";
}
let first = keep(20);
let second = keep(22);
first[0] = 7;
if (first[0] != 7 || second[0] != 22 || withDefault(value: 11) != 11) {
    throw "rest arrays must be fresh and supplied defaults preserved";
}
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
