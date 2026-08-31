include_guard(GLOBAL)

set(ZR_BENCHMARK_TASK3_EXECUTION_PLAN_SCHEMA_VERSION 1)
set(ZR_BENCHMARK_TASK3_EXECUTION_PLAN_ALGORITHM "fisher_yates_splitmix64")
set(ZR_BENCHMARK_TASK3_EXECUTION_PLAN_VERSION 1)
set(ZR_BENCHMARK_TASK3_MAX_TOTAL_SAMPLES 20)
set(ZR_BENCHMARK_TASK3_MAX_EXTRA_SAMPLES 10)

function(zr_benchmark_task3_resolve_policy
         scope tier process_default_warmup process_default_iterations requested_warmup requested_iterations
         out_warmup out_iterations out_extra_samples out_profile out_minimum_mode)
    if (tier STREQUAL "profile")
        set(warmup 0)
        set(iterations 1)
        set(extra_samples 0)
        set(profile true)
        set(minimum_mode disabled)
    else ()
        if (scope STREQUAL "steady")
            set(warmup 5)
            set(iterations 10)
        elseif (scope STREQUAL "process")
            set(warmup "${process_default_warmup}")
            set(iterations "${process_default_iterations}")
        else ()
            message(FATAL_ERROR "Unsupported benchmark scope: ${scope}")
        endif ()
        if (NOT requested_warmup STREQUAL "")
            set(warmup "${requested_warmup}")
        endif ()
        if (NOT requested_iterations STREQUAL "")
            set(iterations "${requested_iterations}")
        endif ()
        set(profile false)
        set(minimum_mode registry)
    endif ()

    if (NOT warmup MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Invalid benchmark warmup: ${warmup}")
    endif ()
    if (NOT iterations MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "Invalid benchmark initial sample count: ${iterations}")
    endif ()
    if (NOT profile)
        if (NOT iterations MATCHES "^([1-9]|1[0-9]|20)$")
            message(FATAL_ERROR
                    "Benchmark initial sample count ${iterations} exceeds the 20-sample runner limit")
        endif ()
        math(EXPR remaining_sample_budget
                "${ZR_BENCHMARK_TASK3_MAX_TOTAL_SAMPLES} - ${iterations}")
        if (remaining_sample_budget LESS ZR_BENCHMARK_TASK3_MAX_EXTRA_SAMPLES)
            set(extra_samples "${remaining_sample_budget}")
        else ()
            set(extra_samples "${ZR_BENCHMARK_TASK3_MAX_EXTRA_SAMPLES}")
        endif ()
    endif ()

    set(${out_warmup} "${warmup}" PARENT_SCOPE)
    set(${out_iterations} "${iterations}" PARENT_SCOPE)
    set(${out_extra_samples} "${extra_samples}" PARENT_SCOPE)
    set(${out_profile} "${profile}" PARENT_SCOPE)
    set(${out_minimum_mode} "${minimum_mode}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_task3_validate_seed seed out_valid)
    set(valid FALSE)
    if (seed MATCHES "^(0|[1-9][0-9]*)$")
        string(LENGTH "${seed}" seed_length)
        if (seed_length LESS 20)
            set(valid TRUE)
        elseif (seed_length EQUAL 20 AND NOT seed STRGREATER "18446744073709551615")
            set(valid TRUE)
        endif ()
    endif ()
    set(${out_valid} "${valid}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_task3_dotnet_jit_state_reused
         persistent warmup calibration_enabled out_reused)
    set(reused false)
    if (persistent AND (warmup GREATER 0 OR calibration_enabled))
        set(reused true)
    endif ()
    set(${out_reused} "${reused}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_task3_create_execution_plan
         python_executable execution_plan_script jobs_json seed cases_json implementations_json output_path out_json)
    zr_benchmark_task3_validate_seed("${seed}" seed_valid)
    if (NOT seed_valid)
        message(FATAL_ERROR "Invalid benchmark execution seed: ${seed}")
    endif ()
    if (NOT EXISTS "${python_executable}")
        message(FATAL_ERROR "Benchmark execution planner Python executable does not exist: ${python_executable}")
    endif ()
    if (NOT EXISTS "${execution_plan_script}")
        message(FATAL_ERROR "Benchmark execution planner does not exist: ${execution_plan_script}")
    endif ()
    if (cases_json STREQUAL "")
        set(cases_json null)
    endif ()
    if (implementations_json STREQUAL "")
        set(implementations_json null)
    endif ()

    get_filename_component(output_directory "${output_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")
    set(request_path "${output_path}.request.json")
    file(WRITE "${request_path}"
            "{\n"
            "  \"jobs\": ${jobs_json},\n"
            "  \"seed\": ${seed},\n"
            "  \"filters\": {\"cases\": ${cases_json}, \"implementations\": ${implementations_json}}\n"
            "}\n")
    execute_process(
            COMMAND "${python_executable}" "${execution_plan_script}"
                    --input "${request_path}" --output "${output_path}"
            RESULT_VARIABLE planner_result
            OUTPUT_VARIABLE planner_stdout
            ERROR_VARIABLE planner_stderr)
    if (NOT planner_result EQUAL 0 OR NOT EXISTS "${output_path}")
        message(FATAL_ERROR
                "Benchmark execution planner failed (${planner_result}):\n${planner_stdout}${planner_stderr}")
    endif ()

    file(READ "${output_path}" plan_json)
    string(JSON plan_schema ERROR_VARIABLE plan_schema_error GET "${plan_json}" schema_version)
    string(JSON plan_algorithm ERROR_VARIABLE plan_algorithm_error GET "${plan_json}" algorithm)
    string(JSON plan_version ERROR_VARIABLE plan_version_error GET "${plan_json}" version)
    string(JSON plan_seed ERROR_VARIABLE plan_seed_error GET "${plan_json}" seed)
    string(JSON plan_job_count ERROR_VARIABLE plan_count_error GET "${plan_json}" job_count)
    if (NOT plan_schema_error STREQUAL "NOTFOUND" OR
        NOT plan_algorithm_error STREQUAL "NOTFOUND" OR
        NOT plan_version_error STREQUAL "NOTFOUND" OR
        NOT plan_seed_error STREQUAL "NOTFOUND" OR
        NOT plan_count_error STREQUAL "NOTFOUND" OR
        NOT plan_schema EQUAL ZR_BENCHMARK_TASK3_EXECUTION_PLAN_SCHEMA_VERSION OR
        NOT plan_algorithm STREQUAL ZR_BENCHMARK_TASK3_EXECUTION_PLAN_ALGORITHM OR
        NOT plan_version EQUAL ZR_BENCHMARK_TASK3_EXECUTION_PLAN_VERSION OR
        NOT "${plan_seed}" STREQUAL "${seed}" OR
        plan_job_count LESS 1)
        message(FATAL_ERROR "Benchmark execution planner returned an invalid contract: ${plan_json}")
    endif ()
    string(JSON actual_job_count LENGTH "${plan_json}" jobs)
    if (NOT actual_job_count EQUAL plan_job_count)
        message(FATAL_ERROR "Benchmark execution plan job_count does not match jobs length")
    endif ()
    set(${out_json} "${plan_json}" PARENT_SCOPE)
endfunction()

function(_zr_benchmark_task3_json_get_required json out_var)
    string(JSON value ERROR_VARIABLE json_error GET "${json}" ${ARGN})
    if (NOT json_error STREQUAL "NOTFOUND")
        string(JOIN "." json_path ${ARGN})
        message(FATAL_ERROR "Runner report is missing or has invalid ${json_path}: ${json_error}")
    endif ()
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

function(_zr_benchmark_task3_json_get_nullable json out_var)
    string(JSON value_type ERROR_VARIABLE type_error TYPE "${json}" ${ARGN})
    if (NOT type_error STREQUAL "NOTFOUND")
        string(JOIN "." json_path ${ARGN})
        message(FATAL_ERROR "Runner report is missing ${json_path}: ${type_error}")
    endif ()
    if (value_type STREQUAL "NULL")
        set(value "")
    else ()
        _zr_benchmark_task3_json_get_required("${json}" value ${ARGN})
    endif ()
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

function(zr_benchmark_task3_parse_runner_report json prefix)
    foreach (field IN ITEMS iterations sample_count extra_sample_count repetitions warmup stability comparable gate_eligible)
        _zr_benchmark_task3_json_get_required("${json}" value "${field}")
        set("${prefix}_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    foreach (field IN ITEMS enabled repetitions)
        _zr_benchmark_task3_json_get_required("${json}" value calibration "${field}")
        set("${prefix}_calibration_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    foreach (field IN ITEMS min_sample_ms aggregate_wall_ms)
        _zr_benchmark_task3_json_get_nullable("${json}" value calibration "${field}")
        set("${prefix}_calibration_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    foreach (field IN ITEMS
            mean_wall_ms median_wall_ms min_wall_ms max_wall_ms stddev_wall_ms
            mad_wall_ms coefficient_of_variation)
        _zr_benchmark_task3_json_get_required("${json}" value summary "${field}")
        set("${prefix}_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    foreach (field IN ITEMS seed statistic resamples low high)
        _zr_benchmark_task3_json_get_required("${json}" value summary bootstrap "${field}")
        set("${prefix}_bootstrap_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    foreach (field IN ITEMS
            mean_peak_working_set_bytes median_peak_working_set_bytes
            min_peak_working_set_bytes max_peak_working_set_bytes)
        _zr_benchmark_task3_json_get_nullable("${json}" value summary "${field}")
        set("${prefix}_${field}" "${value}" PARENT_SCOPE)
    endforeach ()
    string(JSON persistent_type ERROR_VARIABLE persistent_error TYPE "${json}" persistent_session)
    if (NOT persistent_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Runner report is missing persistent_session: ${persistent_error}")
    endif ()
    if (persistent_type STREQUAL "NULL")
        set(persistent_peak "")
    else ()
        _zr_benchmark_task3_json_get_required("${json}" persistent_peak persistent_session peak_working_set_bytes)
    endif ()
    set("${prefix}_persistent_peak_working_set_bytes" "${persistent_peak}" PARENT_SCOPE)
endfunction()
