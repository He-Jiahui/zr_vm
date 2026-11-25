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
# Append to strict_manifest_runtime_boundary.cmake after the scalar and
# compiler policy controls. Use a fresh project for each ownership fixture.
function(check_strict_owner name source expected fault)
    set(work "${TEST_WORK_DIR}/owner-${name}")
    set(src "${work}/src/main.zr")
    set(manifest "${work}/owner.zrp")
    file(MAKE_DIRECTORY "${work}/src")
    file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
    file(WRITE "${src}" "${source}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ZR_VM_TRACE_CORE_BOOTSTRAP=1 ZR_VM_TRACE_PROJECT_STARTUP=1
            ZR_VM_TRACE_STRICT_EXECUTION=1 "${CLI_EXE}" "${manifest}" --emit-executed-via
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    string(REPLACE "\r\n" "\n" output "${output}")
    string(STRIP "${output}" output)
    string(REPLACE "\r\n" "\n" error "${error}")
    # The CLI compiles source before entering the strict runtime. Compiler
    # bootstrap is outside the StrictNoGC execution contract.
    set(begin "[zr-strict-execution] begin\n")
    set(end "[zr-strict-execution] end\n")
    string(FIND "${error}" "${begin}" begin_at)
    if(begin_at LESS 0)
        string(LENGTH "${error}" length)
        math(EXPR last "${length} - 1024")
        if(last LESS 0)
            set(last 0)
        endif()
        string(SUBSTRING "${error}" ${last} -1 error_tail)
        message(FATAL_ERROR "${name}: Did not reach strict execution (${status}): ${output}\n${error_tail}")
    endif()
    string(LENGTH "${begin}" begin_size)
    math(EXPR inside "${begin_at} + ${begin_size}")
    string(SUBSTRING "${error}" ${inside} -1 error)
    string(FIND "${error}" "${end}" end_at)
    if(NOT end_at EQUAL 0)
        message(FATAL_ERROR "${name}: Unexpected output during strict execution: ${error}")
    endif()
    string(LENGTH "${end}" end_size)
    string(SUBSTRING "${error}" ${end_size} -1 error)
    string(STRIP "${error}" error)
    if(fault)
        if(NOT status STREQUAL "1" OR NOT output STREQUAL "" OR NOT error MATCHES "^StrictNoGC: .* at instruction [0-9]+$")
            message(FATAL_ERROR "${name}: Strict interpreter failed to report the expected runtime fault (${status}): ${output}\n${error}")
        endif()
        if(ARGC GREATER 4 AND NOT error MATCHES "${ARGV4}")
            message(FATAL_ERROR "${name}: wrong strict runtime fault: ${error}")
        endif()
        set(reference_fault "${error}")
    elseif(NOT status STREQUAL "0" OR NOT output STREQUAL "${expected}\nexecuted_via=strict-interp" OR NOT error STREQUAL "")
        message(FATAL_ERROR "${name}: Strict ownership interpreter failed (${status}): ${output}\n${error}")
    endif()

    execute_process(COMMAND "${CLI_EXE}" --require-strict-no-gc --compile "${manifest}" --emit-aot-c
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    set(binary "${work}/bin/main.zro")
    set(aot_source "${work}/bin/aot_c/src/main.c")
    if(NOT status STREQUAL "0" OR NOT EXISTS "${binary}" OR NOT EXISTS "${aot_source}")
        message(FATAL_ERROR "${name}: Strict ownership compilation failed (${status}): ${output}\n${error}")
    endif()
    file(RENAME "${src}" "${work}/main.hidden")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ZR_VM_TRACE_CORE_BOOTSTRAP=1 ZR_VM_TRACE_PROJECT_STARTUP=1
            "${CLI_EXE}" "${manifest}" --execution-mode binary --emit-executed-via
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    string(REPLACE "\r\n" "\n" output "${output}")
    string(STRIP "${output}" output)
    string(STRIP "${error}" error)
    if(fault)
        if(NOT status STREQUAL "1" OR NOT output STREQUAL "" OR NOT error STREQUAL reference_fault)
            message(FATAL_ERROR "${name}: Strict binary fault diverged (${status}): ${output}\n${error}")
        endif()
    elseif(NOT status STREQUAL "0" OR NOT output STREQUAL "${expected}\nexecuted_via=strict-binary" OR NOT error STREQUAL "")
        message(FATAL_ERROR "${name}: Strict binary needed source or a managed bootstrap (${status}): ${output}\n${error}")
    endif()

    set(aot_exe "${work}/strict-native${EXE_SUFFIX}")
    if(MSVC)
        set(compile_args /nologo /TC /std:c11 "${MSVC_CRT}" "/I${STRICT_INCLUDE}" "${aot_source}" "${STRICT_ARCHIVE}"
                "/Fe${aot_exe}" "/Fo${work}/strict-native.obj")
    else()
        set(compile_args -std=c11 "-I${STRICT_INCLUDE}" "${aot_source}" "${STRICT_ARCHIVE}" -o "${aot_exe}")
    endif()
    execute_process(COMMAND "${C_COMPILER}" ${compile_args} WORKING_DIRECTORY "${work}"
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${name}: Strict ownership native code did not link against the strict archive (${status}): ${output}\n${error}")
    endif()
    execute_process(COMMAND "${aot_exe}" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    string(STRIP "${output}" output)
    string(STRIP "${error}" error)
    if(fault)
        if(NOT status STREQUAL "1" OR NOT output STREQUAL "" OR NOT error STREQUAL reference_fault)
            message(FATAL_ERROR "${name}: Strict native fault diverged (${status}): ${output}\n${error}")
        endif()
    elseif(NOT status STREQUAL "0" OR NOT output STREQUAL expected OR NOT error STREQUAL "")
        message(FATAL_ERROR "${name}: Strict ownership AOT C and interpreter disagree (${status}): ${output}\n${error}")
    endif()
endfunction()

set(student_decl [=[
resource class Student {
    pub var id: int;
    pub @constructor(id: int) { this.id = id; }
}
]=])
set(borrow_program [=[
struct Grade { pub var score: int = 10; }
let grade = init Grade();
let student: Unique<Student> = own Student(32);
if (grade.score == 10) {
    let view: ref Student = ref student;
    view.id = view.id + grade.score;
} else {
    return 0;
}
let owner = share(student);
return owner.id;
]=])
check_strict_owner(borrow "${student_decl}${borrow_program}" 42 FALSE)

set(loop_program [=[
let student: Unique<Student> = own Student(30);
while (student.id < 42) {
    let increment: Unique<Student> = own Student(3);
    let view: ref Student = ref student;
    view.id = view.id + increment.id;
}
let owner = share(student);
return owner.id;
]=])
check_strict_owner(loop "${student_decl}${loop_program}" 42 FALSE)

set(weak_start [=[
let student: Unique<Student> = own Student(42);
let owner: Shared<Student> = share(student);
let clone = owner;
let observer: Weak<Student> = degrade(owner);
drop(owner);
]=])
check_strict_owner(weak_live "${student_decl}${weak_start}\nreturn wake(observer) != null;" true FALSE)
check_strict_owner(weak_expired "${student_decl}${weak_start}\ndrop(clone);\nreturn wake(observer) == null;" true FALSE)
set(fault_program [=[
let student: Unique<Student> = own Student(42);
return student.id / 0;
]=])
check_strict_owner(fault_unwind "${student_decl}${fault_program}" "" TRUE)

function(reject_strict_owner name source expected_diagnostic)
    set(work "${TEST_WORK_DIR}/reject-owner-${name}")
    set(manifest "${work}/owner.zrp")
    file(MAKE_DIRECTORY "${work}/src")
    file(WRITE "${manifest}" "${envelope},\"StrictNoGC\":true}\n")
    file(WRITE "${work}/src/main.zr" "${student_decl}${source}\n")
    execute_process(COMMAND "${CLI_EXE}" --require-strict-no-gc --compile "${manifest}" --emit-aot-c
            RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(NOT status STREQUAL "1" OR NOT error MATCHES "${expected_diagnostic}" OR EXISTS "${work}/bin/main.zro")
        message(FATAL_ERROR "${name}: Illegal strict ownership source was not rejected (${status}): ${output}\n${error}")
    endif()
endfunction()

set(moved_program [=[
let student: Unique<Student> = own Student(42);
let owner = share(student);
drop(student);
return owner.id;
]=])
reject_strict_owner(use_after_move "${moved_program}" "use after move or drop")
set(borrowed_program [=[
let student: Unique<Student> = own Student(42);
let view: ref Student = ref student;
drop(student);
return view.id;
]=])
reject_strict_owner(drop_while_borrowed "${borrowed_program}" "conflicting borrow")
set(branch_move_program [=[
let student: Unique<Student> = own Student(42);
if (student.id != 0) { drop(student); }
return student.id;
]=])
reject_strict_owner(path_dependent_move "${branch_move_program}" "agree on which owners are live")
set(shared_write_program [=[
let student: Unique<Student> = own Student(42);
let owner = share(student);
owner.id = 0;
return owner.id;
]=])
reject_strict_owner(write_through_shared "${shared_write_program}" "readonly field")
set(string_program [=[
let student: Unique<Student> = own Student(42);
let text = "no managed allocation workaround";
return student.id;
]=])
reject_strict_owner(hidden_allocation "${string_program}" "no verified StrictNoGC lowering")

set(byte_program [=[
let memory: ManualBlock = ManualBlock.tryAlloc(2);
if (memory == null) { return -1; }
if (true) {
    let writable: ref ManualBlock = ref memory;
    writable[0] = 40;
}
let moved = memory;
let bytes: ref readonly ManualBlock = ref moved;
return bytes[0] + bytes[1] + bytes.length;
]=])
check_strict_owner(manual_bytes "${byte_program}" 42 FALSE)

set(invalid_byte_count [=[
let memory = ManualBlock.tryAlloc(-1);
return memory == null;
]=])
check_strict_owner(manual_fallible "${invalid_byte_count}" true FALSE)

set(byte_bounds_program [=[
let memory = ManualBlock.tryAlloc(1);
if (memory == null) { return -1; }
let bytes: ref ManualBlock = ref memory;
bytes[bytes.length] = 1;
return 0;
]=])
check_strict_owner(manual_bounds "${byte_bounds_program}" "" TRUE)

set(readonly_byte_program [=[
let memory = ManualBlock.tryAlloc(1);
let bytes: ref readonly ManualBlock = ref memory;
bytes[0] = 1;
return 0;
]=])
reject_strict_owner(manual_readonly "${readonly_byte_program}" "writable view")

set(moved_byte_program [=[
let memory = ManualBlock.tryAlloc(1);
let bytes: ref ManualBlock = ref memory;
let moved = memory;
return bytes.length;
]=])
reject_strict_owner(manual_move_while_borrowed "${moved_byte_program}" "conflicting borrow")

set(call_program [=[
resource class Counter {
    var value: int;
    pub @constructor(value: int) { this.value = value; }
    pub static fn create(value: int): Unique<Counter> { return own Counter(value); }
    pub fn add(delta: int): void { this.value = this.value + delta; }
    pub const fn read(): int { return this.value; }
}
struct Offset {
    var value: int = 2;
    pub const fn readOffset(): int { return this.value; }
}
fn MakeCounter(value: int): Unique<Counter> {
    if (value < 0) { return Counter.create(0); }
    return Counter.create(value);
}
fn ReadCounter(view: ref readonly Counter): int { return view.read(); }
let counter = MakeCounter(40);
let offset = init Offset();
counter.add(offset.readOffset());
let negative = MakeCounter(-1);
let shared = share(counter);
return ReadCounter(ref shared) + negative.read();
]=])
check_strict_owner(call_ownership "${call_program}" 42 FALSE)

set(scalar_call_program [=[
fn PlusOne(value: int): int { return value + 1; }
return PlusOne(41);
]=])
check_strict_owner(call_scalar "${scalar_call_program}" 42 FALSE)

set(call_alias_program [=[
fn Touch(a: ref Student, b: ref Student): void {
    a.id = 1;
    b.id = 2;
}
let student = own Student(0);
let view: ref Student = ref student;
Touch(view, view);
return view.id;
]=])
reject_strict_owner(call_alias "${call_alias_program}" "conflicting borrow")

set(call_escape_program [=[
fn Leak(view: ref Student): ref Student { return ref view; }
return 0;
]=])
reject_strict_owner(call_borrow_escape "${call_escape_program}" "scoped view cannot escape")

set(call_hidden_program [=[
fn Unused(): int {
    let value = [1, 2, 3];
    return 0;
}
return 1;
]=])
reject_strict_owner(call_hidden "${call_hidden_program}" "managed or unsupported value form")

set(destructor_decl [=[
resource class Cleanup {
    var token: int;
    pub @constructor(token: int) { this.token = token; }
    pub const fn divisor(): int { return this.token; }
    pub @destructor() { let result = 42 / this.divisor(); }
}
]=])
check_strict_owner(destructor_normal "${destructor_decl}\nlet owner = own Cleanup(2);\ndrop(owner);\nreturn 42;" 42 FALSE)

set(destructor_last_shared [=[
let owner = own Cleanup(0);
let shared = share(owner);
let finalOwner = shared;
drop(shared);
drop(finalOwner);
return 42;
]=])
check_strict_owner(destructor_last_shared "${destructor_decl}${destructor_last_shared}" "" TRUE "strict destructor fault")
check_strict_owner(destructor_scope "${destructor_decl}\nlet owner = own Cleanup(0);\nreturn 42;" "" TRUE "strict destructor fault")
check_strict_owner(destructor_preserves_fault "${destructor_decl}\nlet owner = own Cleanup(0);\nreturn 2 / 0;" "" TRUE "arithmetic fault")

set(destructor_allocates [=[
fn OpenBuffer(): ManualBlock { return ManualBlock.tryAlloc(8); }
resource class InvalidCleanup {
    pub @constructor() {}
    pub @destructor() { let buffer = OpenBuffer(); }
}
return 0;
]=])
reject_strict_owner(destructor_callee_allocates "${destructor_allocates}" "destructor and its callees cannot allocate")

set(construct_decl [=[
resource class Staged {
    pub var initial: int;
    pub var result: int;
    pub @constructor(divisor: int) {
        this.initial = 42;
        this.result = this.initial / divisor;
    }
    pub @destructor() { let result = 1 / (this.result - 21); }
}
]=])
check_strict_owner(constructor_fault "${construct_decl}\nlet owner = own Staged(0);\nreturn 0;" "" TRUE "arithmetic fault")
check_strict_owner(constructor_committed "${construct_decl}\nlet owner = own Staged(2);\nreturn owner.result;" "" TRUE "strict destructor fault")

set(uninitialized_field [=[
resource class Uninitialized {
    pub var first: int;
    pub var second: int;
    pub @constructor() { this.first = this.second; this.second = 42; }
}
let bad = own Uninitialized();
return 0;
]=])
reject_strict_owner(constructor_uninitialized "${uninitialized_field}" "uninitialized record field")
