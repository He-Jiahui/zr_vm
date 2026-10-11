cmake_minimum_required(VERSION 3.31)
# Interpret the actual Unity child, including precisely expected failures.
if (NOT DEFINED ZR_TRY_CAPTURE_EXECUTABLE OR NOT EXISTS "${ZR_TRY_CAPTURE_EXECUTABLE}"
        OR NOT DEFINED ZR_TRY_CAPTURE_OUTPUT_DIR
        OR NOT ZR_TRY_CAPTURE_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Supply an actual capture observer executable, output directory and exact phase")
endif ()
set(_output_root "E:/cargo-targets/zr_vm/build")
cmake_path(ABSOLUTE_PATH ZR_TRY_CAPTURE_OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE _output)
cmake_path(IS_PREFIX _output_root "${_output}" NORMALIZE _output_allowed)
cmake_path(ABSOLUTE_PATH ZR_TRY_CAPTURE_EXECUTABLE NORMALIZE OUTPUT_VARIABLE _executable)
cmake_path(IS_PREFIX _output "${_executable}" NORMALIZE _executable_allowed)
if (NOT _output_allowed OR _output STREQUAL _output_root OR NOT _executable_allowed
        OR NOT IS_DIRECTORY "${_output}")
    message(FATAL_ERROR "Use the actual executable inside a separate E build child")
endif ()
set(_stdout_path "${_output}/owner-try-capture-${ZR_TRY_CAPTURE_EXPECTED_PHASE}-stdout.log")
set(_stderr_path "${_output}/owner-try-capture-${ZR_TRY_CAPTURE_EXPECTED_PHASE}-stderr.log")
if (EXISTS "${_stdout_path}" OR EXISTS "${_stderr_path}")
    message(FATAL_ERROR "Preserve existing raw logs; use a reviewed fresh single-writer build child")
endif ()
execute_process(COMMAND "${_executable}" WORKING_DIRECTORY "${_output}"
        RESULT_VARIABLE _exit OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
        TIMEOUT 120 ENCODING UTF-8)
# The Root route supplies a unique fresh child and one writer. This refuses
# existing logs, but CMake file(WRITE) is not an OS exclusive-create operation.
file(WRITE "${_stdout_path}" "${_stdout}")
file(WRITE "${_stderr_path}" "${_stderr}")
message("OWNER_TRY_CAPTURE_CHILD phase=${ZR_TRY_CAPTURE_EXPECTED_PHASE} actual_exit=${_exit}")
message("${_stdout}")
message("${_stderr}")
if (ZR_TRY_CAPTURE_EXPECTED_PHASE STREQUAL "RED")
    set(_expected_exit 9)
    set(_expected_failures 9)
else ()
    set(_expected_exit 1)
    set(_expected_failures 1)
endif ()
if (NOT "${_exit}" STREQUAL "${_expected_exit}")
    message(FATAL_ERROR "Unexpected actual Unity exit or timeout/crash; raw streams retained")
endif ()
if (NOT "${_stderr}" STREQUAL "")
    message(FATAL_ERROR "Capture observer stderr must be empty")
endif ()
string(REPLACE "\r\n" "\n" _stdout_normal "${_stdout}")
if (_stdout_normal MATCHES "runtime error:|UndefinedBehaviorSanitizer|[Ww]arning:|EXEC_OWNER_LEDGER")
    message(FATAL_ERROR "Unexpected warning, sanitizer or ledger diagnostic")
endif ()
set(_names
        test_capture_empty_module
        test_capture_scalar_contract_order
        test_capture_module_pools
        test_capture_function_pools
        test_capture_optional_nested_maps
        test_capture_excluded_storage_details
        test_capture_exact_word_budget
        test_capture_null_refusal
        test_capture_budget_refusal
        test_capture_structural_refusal
        test_capture_prior_unity_failure
        test_capture_prior_failure_witness)
# Columns: feature, calls, accepted, words, expected_words, acquired, prior_failed.
# Common flags and freed=acquired are generated from the independently reviewed main.
set(_table_RED
        "0,1,0,0,19,0,0"
        "0,1,0,0,19,0,0"
        "0,1,0,0,35,3,0"
        "0,1,0,0,182,18,0"
        "0,1,0,0,255,32,0"
        "0,3,0,0,22,2,0"
        "0,1,0,0,4096,1,0"
        "1,2,0,0,0,0,0"
        "1,1,0,0,4099,1,0"
        "1,60,0,0,0,32,0"
        "0,1,0,0,22,1,1"
        "0,0,0,0,22,0,0")
set(_table_GREEN
        "1,1,1,19,19,0,0"
        "1,1,1,19,19,0,0"
        "1,1,1,35,35,3,0"
        "1,1,1,182,182,18,0"
        "1,1,1,255,255,32,0"
        "1,3,3,22,22,2,0"
        "1,1,1,4096,4096,1,0"
        "1,2,0,0,0,0,0"
        "1,1,0,0,4099,1,0"
        "1,60,0,0,0,32,0"
        "1,1,1,22,22,1,1"
        "1,0,0,0,22,0,0")
function(table_expected case feature calls accepted words expected_words acquired prior_failed output)
    set(${output} "OWNER_TRY_CAPTURE_CASE case=${case} support=1 feature=${feature} drained=1 calls=${calls} accepted=${accepted} words=${words} expected_words=${expected_words} graph_preserved=1 output_checked=1 acquired=${acquired} freed=${acquired} prior_failed=${prior_failed} prior_ignored=0 flags_preserved=1" PARENT_SCOPE)
endfunction()
set(_table "${_table_${ZR_TRY_CAPTURE_EXPECTED_PHASE}}")
list(LENGTH _names _name_count)
list(LENGTH _table _table_count)
if (NOT _name_count EQUAL 12 OR NOT _table_count EQUAL 12)
    message(FATAL_ERROR "Checker requires twelve names and twelve explicit witness rows")
endif ()
set(_result_count 0)
set(_case_count 0)
set(_summary_count 0)
set(_final_count 0)
set(_footer_count 0)
set(_failure_count 0)
set(_separator_count 0)
string(REGEX MATCHALL "[^\n]+" _lines "${_stdout_normal}")
foreach (_line IN LISTS _lines)
    string(STRIP "${_line}" _line)
    if (_line STREQUAL "")
        continue()
    endif ()
    if (_line MATCHES "^.*test_ssa_owner_try_capture[.]c:[0-9]+:(test_[a-z_]+):(PASS|FAIL|IGNORE)(:(.*))?$")
        if (_result_count GREATER_EQUAL 12 OR NOT _result_count EQUAL _case_count
                OR _final_count OR _summary_count)
            message(FATAL_ERROR "Unexpected result count/order")
        endif ()
        set(_name "${CMAKE_MATCH_1}")
        set(_result "${CMAKE_MATCH_2}")
        set(_reason "${CMAKE_MATCH_4}")
        list(GET _names ${_result_count} _expected_name)
        math(EXPR _number "${_result_count}+1")
        if (_number EQUAL 11)
            set(_expected_result FAIL)
            set(_expected_reason OWNER_TRY_CAPTURE_PRIOR_FAILURE)
        elseif (ZR_TRY_CAPTURE_EXPECTED_PHASE STREQUAL "RED" AND
                (_number LESS_EQUAL 7 OR _number EQUAL 12))
            set(_expected_result FAIL)
            set(_expected_reason OWNER_TRY_CAPTURE_MISSING_FEATURE)
        else ()
            set(_expected_result PASS)
            set(_expected_reason "")
        endif ()
        if (NOT _name STREQUAL _expected_name OR NOT _result STREQUAL _expected_result
                OR NOT _reason STREQUAL _expected_reason)
            message(FATAL_ERROR "Unexpected exact case name/result/reason at ${_number}")
        endif ()
        if (_result STREQUAL "FAIL")
            math(EXPR _failure_count "${_failure_count}+1")
        endif ()
        math(EXPR _result_count "${_result_count}+1")
    elseif (_line MATCHES "^OWNER_TRY_CAPTURE_CASE ")
        math(EXPR _number "${_case_count}+1")
        if (_case_count GREATER_EQUAL 12 OR NOT _result_count EQUAL _number OR _final_count)
            message(FATAL_ERROR "Unexpected saved-case witness order")
        endif ()
        list(GET _table ${_case_count} _row)
        string(REPLACE "," ";" _values "${_row}")
        list(LENGTH _values _value_count)
        if (NOT _value_count EQUAL 7)
            message(FATAL_ERROR "Witness table row requires seven explicit varying fields")
        endif ()
        table_expected(${_number} ${_values} _expected_case)
        if (NOT _line STREQUAL _expected_case)
            message(FATAL_ERROR "Unexpected exact capture observer witness at ${_number}")
        endif ()
        math(EXPR _case_count "${_case_count}+1")
    elseif (_line STREQUAL "OWNER_TRY_CAPTURE_FINAL drain=1 live=0 acquired=90 freed=90 prior_failed=0 prior_ignored=0 flags_preserved=1")
        if (NOT _result_count EQUAL 12 OR NOT _case_count EQUAL 12 OR _final_count OR _summary_count)
            message(FATAL_ERROR "Unexpected final-drain witness order/count")
        endif ()
        math(EXPR _final_count "${_final_count}+1")
    elseif (_line STREQUAL "-----------------------")
        if (NOT _final_count EQUAL 1 OR _separator_count OR _summary_count)
            message(FATAL_ERROR "Unexpected Unity summary separator")
        endif ()
        math(EXPR _separator_count "${_separator_count}+1")
    elseif (_line STREQUAL "12 Tests ${_expected_failures} Failures 0 Ignored")
        if (NOT _separator_count EQUAL 1 OR _summary_count)
            message(FATAL_ERROR "Unexpected actual Unity summary order/count")
        endif ()
        math(EXPR _summary_count "${_summary_count}+1")
    elseif (_line STREQUAL "FAIL")
        if (NOT _summary_count EQUAL 1 OR _footer_count)
            message(FATAL_ERROR "Unexpected actual Unity failure footer")
        endif ()
        math(EXPR _footer_count "${_footer_count}+1")
    else ()
        message(FATAL_ERROR "Unexpected capture observer stdout line: ${_line}")
    endif ()
endforeach ()
if (NOT _result_count EQUAL 12 OR NOT _case_count EQUAL 12 OR NOT _summary_count EQUAL 1
        OR NOT _final_count EQUAL 1 OR NOT _footer_count EQUAL 1 OR NOT _separator_count EQUAL 1
        OR NOT _failure_count EQUAL _expected_failures)
    message(FATAL_ERROR "Incomplete exact capture observer negative-run protocol")
endif ()
message("OWNER_TRY_CAPTURE_PROTOCOL_ACCEPTED phase=${ZR_TRY_CAPTURE_EXPECTED_PHASE} cases=12 expected_failures=${_expected_failures} ignored=0 actual_exit=${_exit}")
