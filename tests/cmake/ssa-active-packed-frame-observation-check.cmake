cmake_minimum_required(VERSION 3.31)
# Interpret the actual Unity child, including precisely expected failures.
if (NOT DEFINED ZR_ACTIVE_FRAME_EXECUTABLE OR NOT EXISTS "${ZR_ACTIVE_FRAME_EXECUTABLE}"
        OR NOT DEFINED ZR_ACTIVE_FRAME_OUTPUT_DIR
        OR NOT ZR_ACTIVE_FRAME_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Supply an actual active packed-frame executable, output directory and exact phase")
endif ()
set(_output_root "E:/cargo-targets/zr_vm/build")
cmake_path(ABSOLUTE_PATH ZR_ACTIVE_FRAME_OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE _output)
cmake_path(IS_PREFIX _output_root "${_output}" NORMALIZE _output_allowed)
cmake_path(ABSOLUTE_PATH ZR_ACTIVE_FRAME_EXECUTABLE NORMALIZE OUTPUT_VARIABLE _executable)
cmake_path(IS_PREFIX _output "${_executable}" NORMALIZE _executable_allowed)
if (NOT _output_allowed OR _output STREQUAL _output_root OR NOT _executable_allowed
        OR NOT IS_DIRECTORY "${_output}")
    message(FATAL_ERROR "Use the actual executable inside a separate E build child")
endif ()
set(_stdout_path "${_output}/active-packed-frame-observation-${ZR_ACTIVE_FRAME_EXPECTED_PHASE}-stdout.log")
set(_stderr_path "${_output}/active-packed-frame-observation-${ZR_ACTIVE_FRAME_EXPECTED_PHASE}-stderr.log")
if (EXISTS "${_stdout_path}" OR EXISTS "${_stderr_path}")
    message(FATAL_ERROR "Preserve existing raw logs; use a reviewed fresh single-writer build child")
endif ()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP --unset=ZR_VM_TRACE_PROJECT_STARTUP
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0 ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        "${_executable}" WORKING_DIRECTORY "${_output}"
        RESULT_VARIABLE _exit OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
        TIMEOUT 120 ENCODING UTF-8)
# The Root route supplies a unique fresh child and one writer. This refuses
# existing logs, but CMake file(WRITE) is not an OS exclusive-create operation.
file(WRITE "${_stdout_path}" "${_stdout}")
file(WRITE "${_stderr_path}" "${_stderr}")
message("ACTIVE_PACKED_FRAME_CHILD phase=${ZR_ACTIVE_FRAME_EXPECTED_PHASE} actual_exit=${_exit}")
message("${_stdout}")
message("${_stderr}")
if (ZR_ACTIVE_FRAME_EXPECTED_PHASE STREQUAL "RED")
    set(_expected_exit 15)
    set(_expected_failures 15)
else ()
    set(_expected_exit 1)
    set(_expected_failures 1)
endif ()
if (NOT "${_exit}" STREQUAL "${_expected_exit}")
    message(FATAL_ERROR "Unexpected actual Unity exit or timeout/crash; raw streams retained")
endif ()
if (NOT "${_stderr}" STREQUAL "")
    message(FATAL_ERROR "Active packed-frame stderr must be empty")
endif ()
string(REPLACE "\r\n" "\n" _stdout_normal "${_stdout}")
if (_stdout_normal MATCHES "runtime error:|UndefinedBehaviorSanitizer|[Ww]arning:|EXEC_OWNER_LEDGER")
    message(FATAL_ERROR "Unexpected warning, sanitizer or ledger diagnostic")
endif ()
set(_names
        test_active_reused_scalar_selection
        test_active_selection_order
        test_active_readonly_selection
        test_active_empty_selection
        test_active_scalar_widths
        test_active_mixed_width_reuse
        test_active_value_id_refusal
        test_active_conflict_refusal
        test_active_capacity_refusal
        test_active_layout_geometry_refusal
        test_active_unsupported_and_mixed_layout
        test_active_alias_refusal
        test_active_repeated_layout_lifetime
        test_active_legacy_contract
        test_active_prior_unity_failure
        test_active_prior_failure_witness)
# Columns: feature, builders, calls, accepted, observed, invalidations, acquired, prior_failed.
set(_table_RED
        "0,1,2,0,0,0,1,0"
        "0,1,1,0,0,0,1,0"
        "0,1,2,0,0,0,1,0"
        "0,2,3,0,0,0,2,0"
        "0,4,4,0,0,0,4,0"
        "0,1,1,0,0,0,1,0"
        "0,1,5,0,0,0,1,0"
        "0,1,2,0,0,0,1,0"
        "0,1,11,0,0,0,1,0"
        "0,1,20,0,0,0,1,0"
        "0,5,5,0,0,0,5,0"
        "0,1,6,0,0,0,1,0"
        "0,2,4098,0,0,0,2,0"
        "1,1,0,0,0,0,1,0"
        "0,1,1,0,0,0,1,1"
        "0,0,0,0,0,0,0,0")
set(_table_GREEN
        "1,1,2,2,2,2,1,0"
        "1,1,1,1,2,2,1,0"
        "1,1,2,2,2,2,1,0"
        "1,2,3,3,0,0,2,0"
        "1,4,4,4,4,4,4,0"
        "1,1,1,1,1,1,1,0"
        "1,1,5,0,0,0,1,0"
        "1,1,2,0,0,0,1,0"
        "1,1,11,0,0,0,1,0"
        "1,1,20,0,0,0,1,0"
        "1,5,5,1,1,1,5,0"
        "1,1,6,0,0,0,1,0"
        "1,2,4098,4097,4097,4097,2,0"
        "1,1,0,0,0,0,1,0"
        "1,1,1,1,1,1,1,1"
        "1,0,0,0,0,0,0,0")
function(table_expected case feature builders calls accepted observed invalidations acquired prior_failed output)
    set(${output} "ACTIVE_PACKED_FRAME_CASE case=${case} support=1 feature=${feature} drained=1 builders=${builders} calls=${calls} accepted=${accepted} observed=${observed} invalidations=${invalidations} layout_preserved=1 output_checked=1 acquired=${acquired} freed=${acquired} prior_failed=${prior_failed} prior_ignored=0 flags_preserved=1" PARENT_SCOPE)
endfunction()
set(_table "${_table_${ZR_ACTIVE_FRAME_EXPECTED_PHASE}}")
list(LENGTH _names _name_count)
list(LENGTH _table _table_count)
if (NOT _name_count EQUAL 16 OR NOT _table_count EQUAL 16)
    message(FATAL_ERROR "Checker requires sixteen names and sixteen explicit witness rows")
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
    if (_line MATCHES "^.*test_ssa_active_packed_frame_observation[.]c:[0-9]+:(test_[a-z_]+):(PASS|FAIL|IGNORE)(:(.*))?$")
        if (_result_count GREATER_EQUAL 16 OR NOT _result_count EQUAL _case_count
                OR _final_count OR _summary_count)
            message(FATAL_ERROR "Unexpected result count/order")
        endif ()
        set(_name "${CMAKE_MATCH_1}")
        set(_result "${CMAKE_MATCH_2}")
        set(_reason "${CMAKE_MATCH_4}")
        list(GET _names ${_result_count} _expected_name)
        math(EXPR _number "${_result_count}+1")
        if (_number EQUAL 15)
            set(_expected_result FAIL)
            set(_expected_reason ACTIVE_PACKED_FRAME_PRIOR_FAILURE)
        elseif (ZR_ACTIVE_FRAME_EXPECTED_PHASE STREQUAL "RED" AND NOT _number EQUAL 14)
            set(_expected_result FAIL)
            set(_expected_reason ACTIVE_PACKED_FRAME_MISSING_FEATURE)
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
    elseif (_line MATCHES "^ACTIVE_PACKED_FRAME_CASE ")
        math(EXPR _number "${_case_count}+1")
        if (_case_count GREATER_EQUAL 16 OR NOT _result_count EQUAL _number OR _final_count)
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
            message(FATAL_ERROR "Unexpected exact active packed-frame witness at ${_number}")
        endif ()
        math(EXPR _case_count "${_case_count}+1")
    elseif (_line STREQUAL "ACTIVE_PACKED_FRAME_FINAL drain=1 live=0 acquired=24 freed=24 prior_failed=0 prior_ignored=0 flags_preserved=1")
        if (NOT _result_count EQUAL 16 OR NOT _case_count EQUAL 16 OR _final_count OR _summary_count)
            message(FATAL_ERROR "Unexpected final-drain witness order/count")
        endif ()
        math(EXPR _final_count "${_final_count}+1")
    elseif (_line STREQUAL "-----------------------")
        if (NOT _final_count EQUAL 1 OR _separator_count OR _summary_count)
            message(FATAL_ERROR "Unexpected Unity summary separator")
        endif ()
        math(EXPR _separator_count "${_separator_count}+1")
    elseif (_line STREQUAL "16 Tests ${_expected_failures} Failures 0 Ignored")
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
        message(FATAL_ERROR "Unexpected active packed-frame stdout line: ${_line}")
    endif ()
endforeach ()
if (NOT _result_count EQUAL 16 OR NOT _case_count EQUAL 16 OR NOT _summary_count EQUAL 1
        OR NOT _final_count EQUAL 1 OR NOT _footer_count EQUAL 1 OR NOT _separator_count EQUAL 1
        OR NOT _failure_count EQUAL _expected_failures)
    message(FATAL_ERROR "Incomplete exact active packed-frame negative-run protocol")
endif ()
message("ACTIVE_PACKED_FRAME_PROTOCOL_ACCEPTED phase=${ZR_ACTIVE_FRAME_EXPECTED_PHASE} cases=16 expected_failures=${_expected_failures} ignored=0 actual_exit=${_exit}")
