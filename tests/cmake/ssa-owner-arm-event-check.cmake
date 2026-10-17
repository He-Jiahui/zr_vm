cmake_minimum_required(VERSION 3.31)
# Interpret the actual Unity child, including precisely expected failures.
if (NOT DEFINED ZR_ARM_EVENT_EXECUTABLE OR NOT EXISTS "${ZR_ARM_EVENT_EXECUTABLE}"
        OR NOT DEFINED ZR_ARM_EVENT_OUTPUT_DIR
        OR NOT ZR_ARM_EVENT_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Supply an actual owner observer executable, output directory and exact phase")
endif ()
cmake_path(ABSOLUTE_PATH ZR_ARM_EVENT_OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE _output)
cmake_path(ABSOLUTE_PATH ZR_ARM_EVENT_EXECUTABLE NORMALIZE OUTPUT_VARIABLE _executable)
cmake_path(IS_PREFIX _output "${_executable}" NORMALIZE _executable_allowed)
if (NOT _executable_allowed
        OR NOT IS_DIRECTORY "${_output}")
    message(FATAL_ERROR "Use the actual executable inside the configured build directory")
endif ()
set(_stdout_path "${_output}/owner-arm-event-${ZR_ARM_EVENT_EXPECTED_PHASE}-stdout.log")
set(_stderr_path "${_output}/owner-arm-event-${ZR_ARM_EVENT_EXPECTED_PHASE}-stderr.log")

execute_process(COMMAND "${_executable}" WORKING_DIRECTORY "${_output}"
        RESULT_VARIABLE _exit OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
        TIMEOUT 120 ENCODING UTF-8)
# Each CTest case owns these logs; reruns replace its previous output.
file(WRITE "${_stdout_path}" "${_stdout}")
file(WRITE "${_stderr_path}" "${_stderr}")
message("OWNER_ARM_EVENT_CHILD phase=${ZR_ARM_EVENT_EXPECTED_PHASE} actual_exit=${_exit}")
message("${_stdout}")
message("${_stderr}")
if (ZR_ARM_EVENT_EXPECTED_PHASE STREQUAL "RED")
    set(_expected_exit 8)
    set(_expected_failures 8)
    set(_final_records 2)
else ()
    set(_expected_exit 1)
    set(_expected_failures 1)
    set(_final_records 1)
endif ()
if (NOT "${_exit}" STREQUAL "${_expected_exit}")
    message(FATAL_ERROR "Unexpected actual Unity exit or timeout/crash; raw streams retained")
endif ()
if (NOT "${_stderr}" STREQUAL "")
    message(FATAL_ERROR "Owner observer stderr must be empty")
endif ()
string(REPLACE "\r\n" "\n" _stdout_normal "${_stdout}")
if (_stdout_normal MATCHES "runtime error:|UndefinedBehaviorSanitizer|[Ww]arning:|EXEC_OWNER_LEDGER")
    message(FATAL_ERROR "Unexpected warning, sanitizer or ledger diagnostic")
endif ()
set(_names
        test_arm_first_malloc
        test_arm_second_calloc
        test_malloc_event_copy
        test_calloc_event_copy
        test_failed_realloc_event
        test_event_query_keeps_armed_fault
        test_arm_zero_refusal
        test_arm_uintmax_refusal
        test_second_arm_refusal
        test_event_invalid_preserves_output
        test_prior_unity_failure
        test_prior_failure_witness)
# Columns: feature, arm, event, next_event, attempts, records, failures, prior_failed.
# Common fields and the real per-record drain totals are generated below.
set(_table_RED
        "0,0,0,0,1,1,0,0" "0,0,0,0,2,2,0,0"
        "0,0,0,0,1,1,0,0" "0,0,0,0,1,1,0,0"
        "0,0,0,0,2,1,1,0" "0,0,0,0,2,1,1,0"
        "1,0,0,0,1,1,0,0" "1,0,0,0,2,2,0,0"
        "1,0,0,0,1,0,1,0" "1,0,0,0,2,1,1,0"
        "0,0,0,0,2,2,0,1" "0,0,0,0,2,2,0,0")
set(_table_GREEN
        "1,1,0,0,1,0,1,0" "1,1,0,0,2,1,1,0"
        "1,0,1,0,1,1,0,0" "1,0,1,0,1,1,0,0"
        "1,0,1,0,2,1,1,0" "1,0,1,0,2,1,1,0"
        "1,0,0,0,1,1,0,0" "1,0,0,0,2,2,0,0"
        "1,0,0,0,1,0,1,0" "1,0,0,0,2,1,1,0"
        "1,1,1,1,2,1,1,1" "1,1,1,1,2,1,1,0")
function(table_expected case feature arm event next_event attempts records failures prior_failed output)
    set(${output} "OWNER_ARM_EVENT_CASE case=${case} support=1 feature=${feature} drained=1 arm=${arm} event=${event} next_event=${next_event} attempts=${attempts} records=${records} live=0 frees=${records} errors=0 failures=${failures} queries=${records} record_frees=${records} prior_failed=${prior_failed} prior_ignored=0 flags_preserved=1" PARENT_SCOPE)
endfunction()
set(_table "${_table_${ZR_ARM_EVENT_EXPECTED_PHASE}}")
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
    if (_line MATCHES "^.*test_ssa_owner_arm_event[.]c:[0-9]+:(test_[a-z_]+):(PASS|FAIL|IGNORE)(:(.*))?$")
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
            set(_expected_reason OWNER_ARM_EVENT_PRIOR_FAILURE)
        elseif (ZR_ARM_EVENT_EXPECTED_PHASE STREQUAL "RED" AND
                (_number LESS_EQUAL 6 OR _number EQUAL 12))
            set(_expected_result FAIL)
            set(_expected_reason OWNER_ARM_EVENT_MISSING_FEATURE)
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
    elseif (_line MATCHES "^OWNER_ARM_EVENT_CASE ")
        math(EXPR _number "${_case_count}+1")
        if (_case_count GREATER_EQUAL 12 OR NOT _result_count EQUAL _number OR _final_count)
            message(FATAL_ERROR "Unexpected saved-case witness order")
        endif ()
        list(GET _table ${_case_count} _row)
        string(REPLACE "," ";" _values "${_row}")
        list(LENGTH _values _value_count)
        if (NOT _value_count EQUAL 8)
            message(FATAL_ERROR "Witness table row requires eight explicit varying fields")
        endif ()
        table_expected(${_number} ${_values} _expected_case)
        if (NOT _line STREQUAL _expected_case)
            message(FATAL_ERROR "Unexpected exact owner observer witness at ${_number}")
        endif ()
        math(EXPR _case_count "${_case_count}+1")
    elseif (_line STREQUAL "OWNER_ARM_EVENT_FINAL drain=1 live=0 errors=0 records=${_final_records} frees=${_final_records} queries=${_final_records} record_frees=${_final_records} flags_preserved=1")
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
        message(FATAL_ERROR "Unexpected owner observer stdout line: ${_line}")
    endif ()
endforeach ()
if (NOT _result_count EQUAL 12 OR NOT _case_count EQUAL 12 OR NOT _summary_count EQUAL 1
        OR NOT _final_count EQUAL 1 OR NOT _footer_count EQUAL 1 OR NOT _separator_count EQUAL 1
        OR NOT _failure_count EQUAL _expected_failures)
    message(FATAL_ERROR "Incomplete exact owner observer negative-run protocol")
endif ()
message("OWNER_ARM_EVENT_PROTOCOL_ACCEPTED phase=${ZR_ARM_EVENT_EXPECTED_PHASE} cases=12 expected_failures=${_expected_failures} ignored=0 actual_exit=${_exit}")
