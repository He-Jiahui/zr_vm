function(zr_benchmark_measurement_contract_get
        implementation_id
        out_measurement_scope
        out_prepare_scope
        out_runtime_reused
        out_compiler_reused
        out_jit_state_reused)
    set(measurement_scope "")
    set(prepare_scope "")

    if (implementation_id STREQUAL "c" OR implementation_id STREQUAL "rust")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "none")
    elseif (implementation_id STREQUAL "zr_interp")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "source_load_compile_in_measurement")
    elseif (implementation_id STREQUAL "zr_binary")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "bytecode_compile_before_measurement")
    elseif (implementation_id STREQUAL "python" OR
            implementation_id STREQUAL "node" OR
            implementation_id STREQUAL "qjs" OR
            implementation_id STREQUAL "lua")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "script_load_in_measurement")
    elseif (implementation_id STREQUAL "dotnet" OR implementation_id STREQUAL "java")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "runtime_start_jit_in_measurement")
    endif ()

    set(${out_measurement_scope} "${measurement_scope}" PARENT_SCOPE)
    set(${out_prepare_scope} "${prepare_scope}" PARENT_SCOPE)
    if (measurement_scope STREQUAL "")
        set(${out_runtime_reused} "" PARENT_SCOPE)
        set(${out_compiler_reused} "" PARENT_SCOPE)
        set(${out_jit_state_reused} "" PARENT_SCOPE)
    else ()
        set(${out_runtime_reused} "false" PARENT_SCOPE)
        set(${out_compiler_reused} "false" PARENT_SCOPE)
        set(${out_jit_state_reused} "false" PARENT_SCOPE)
    endif ()
endfunction()

function(zr_benchmark_measurement_contract_validate
        measurement_scope
        prepare_scope
        runtime_reused
        compiler_reused
        jit_state_reused
        out_valid)
    set(valid TRUE)
    if (NOT measurement_scope STREQUAL "process_end_to_end" AND
            NOT measurement_scope STREQUAL "persistent_runtime")
        set(valid FALSE)
    endif ()

    set(valid_prepare_scopes
            none
            source_load_compile_in_measurement
            bytecode_compile_before_measurement
            script_load_in_measurement
            runtime_start_jit_in_measurement
            runtime_and_case_loaded_before_measurement
            script_load_before_measurement
            runtime_start_before_measurement
            bytecode_compile_and_load_before_measurement)
    list(FIND valid_prepare_scopes "${prepare_scope}" prepare_scope_index)
    if (prepare_scope_index EQUAL -1)
        set(valid FALSE)
    endif ()

    foreach (reuse_value IN ITEMS "${runtime_reused}" "${compiler_reused}" "${jit_state_reused}")
        if (NOT reuse_value STREQUAL "true" AND NOT reuse_value STREQUAL "false")
            set(valid FALSE)
        endif ()
    endforeach ()
    set(${out_valid} ${valid} PARENT_SCOPE)
endfunction()

function(zr_benchmark_measurement_contract_decimal_to_milli value out_var)
    string(REGEX MATCH "^([0-9]+)\\.([0-9][0-9][0-9])$" matched "${value}")
    if (matched STREQUAL "")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif ()
    math(EXPR milli "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
    set(${out_var} "${milli}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_measurement_contract_format_milli milli_value out_var)
    math(EXPR whole "${milli_value} / 1000")
    math(EXPR fraction "${milli_value} % 1000")
    if (fraction LESS 10)
        set(fraction_text "00${fraction}")
    elseif (fraction LESS 100)
        set(fraction_text "0${fraction}")
    else ()
        set(fraction_text "${fraction}")
    endif ()
    set(${out_var} "${whole}.${fraction_text}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_measurement_contract_ratio
        value
        base
        value_measurement_scope
        base_measurement_scope
        out_var)
    if (value STREQUAL "" OR
            base STREQUAL "" OR
            value_measurement_scope STREQUAL "" OR
            base_measurement_scope STREQUAL "" OR
            NOT value_measurement_scope STREQUAL base_measurement_scope)
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

    zr_benchmark_measurement_contract_decimal_to_milli("${value}" value_milli)
    zr_benchmark_measurement_contract_decimal_to_milli("${base}" base_milli)
    if (value_milli STREQUAL "" OR base_milli STREQUAL "" OR base_milli LESS 1)
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

    math(EXPR ratio_milli "((${value_milli} * 1000) + (${base_milli} / 2)) / ${base_milli}")
    zr_benchmark_measurement_contract_format_milli("${ratio_milli}" ratio_text)
    set(${out_var} "${ratio_text}" PARENT_SCOPE)
endfunction()
