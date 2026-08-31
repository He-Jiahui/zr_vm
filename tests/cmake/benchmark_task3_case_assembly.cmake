# Task 3 deferred case/report assembly.
# Contract: this include runs after PERF_RESULT_<case>_<implementation>_* has
# been populated for every planned job. It consumes PERF_CASE_ORDER,
# PERF_PLANNED_IMPLEMENTATIONS_<case>, the PERF_RESULT_* fields, registry
# metadata, and existing perf_* helper functions; it updates the report row
# accumulators and PERF_GC_* fields in the including script's scope.

foreach (case_name IN LISTS PERF_CASE_ORDER)
    perf_case_scale("${case_name}" case_scale)
    set(case_description "${ZR_VM_BENCHMARK_DESCRIPTION_${case_name}}")
    set(case_banner "${ZR_VM_BENCHMARK_PASS_BANNER_${case_name}}")
    set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_${PERF_REQUESTED_TIER}}")
    if (case_checksum STREQUAL "")
        set(case_checksum "${ZR_VM_BENCHMARK_CHECKSUM_${case_name}_core}")
    endif ()
    if (PERF_PROFILE_MODE)
        set(case_min_sample_ms 0)
    else ()
        set(case_min_sample_ms "${ZR_VM_BENCHMARK_MIN_SAMPLE_MS_${case_name}}")
    endif ()

    set(case_impl_jsons "")
    foreach (implementation_key IN ITEMS c interp python node qjs lua rust dotnet java)
        set("case_${implementation_key}_mean" "")
        set("case_${implementation_key}_measurement_scope" "")
    endforeach ()
    set(case_c_baseline_mean "")
    set(case_profile_report_path "")
    set(case_interp_command_list "")
    set(case_interp_working_directory "")
    set(case_interp_ready FALSE)

    foreach (implementation_id IN LISTS PERF_PLANNED_IMPLEMENTATIONS_${case_name})
        set(result_prefix "PERF_RESULT_${case_name}_${implementation_id}")
        set(result_status "${${result_prefix}_status}")
        set(result_comparable "${${result_prefix}_comparable}")
        set(result_stability "${${result_prefix}_stability}")
        set(result_gate_eligible "${${result_prefix}_gate_eligible}")
        if (result_status STREQUAL "PASS" AND result_comparable AND result_gate_eligible AND
            result_stability STREQUAL "STABLE")
            set(result_mean "${${result_prefix}_mean_wall_ms}")
            set(result_scope "${${result_prefix}_measurement_scope}")
            if (implementation_id STREQUAL "c")
                set(case_c_baseline_mean "${result_mean}")
                set(case_c_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "zr_interp")
                set(case_interp_mean "${result_mean}")
                set(case_interp_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "python")
                set(case_python_mean "${result_mean}")
                set(case_python_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "node")
                set(case_node_mean "${result_mean}")
                set(case_node_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "qjs")
                set(case_qjs_mean "${result_mean}")
                set(case_qjs_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "lua")
                set(case_lua_mean "${result_mean}")
                set(case_lua_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "rust")
                set(case_rust_mean "${result_mean}")
                set(case_rust_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "dotnet")
                set(case_dotnet_mean "${result_mean}")
                set(case_dotnet_measurement_scope "${result_scope}")
            elseif (implementation_id STREQUAL "java")
                set(case_java_mean "${result_mean}")
                set(case_java_measurement_scope "${result_scope}")
            endif ()
        endif ()
        if (implementation_id STREQUAL "zr_interp")
            set(case_profile_report_path "${${result_prefix}_profile_report_path}")
            set(case_interp_command_list "${${result_prefix}_interp_command_list}")
            set(case_interp_working_directory "${${result_prefix}_interp_working_directory}")
            set(case_interp_ready "${${result_prefix}_interp_ready}")
        endif ()
    endforeach ()

    foreach (implementation_id IN LISTS PERF_PLANNED_IMPLEMENTATIONS_${case_name})
        set(result_prefix "PERF_RESULT_${case_name}_${implementation_id}")
        foreach (result_field IN ITEMS
                implementation_name language mode status note working_directory
                measurement_scope prepare_scope runtime_reused compiler_reused jit_state_reused
                mean_wall_ms median_wall_ms min_wall_ms max_wall_ms stddev_wall_ms mad_wall_ms
                coefficient_of_variation bootstrap_low bootstrap_high sample_count extra_sample_count
                repetitions stability comparable gate_eligible calibration_enabled calibration_aggregate_wall_ms
                mean_peak_mib max_peak_mib profile_report_path perf_json_text command_list)
            set("${result_field}" "${${result_prefix}_${result_field}}")
        endforeach ()
        if (NOT PERF_TASK4_PROVISIONAL_COMPARABLE)
            set(comparable FALSE)
            set(gate_eligible FALSE)
        endif ()
        set(relative_to_c null)
        if (status STREQUAL "PASS" AND comparable AND gate_eligible AND stability STREQUAL "STABLE")
            zr_benchmark_measurement_contract_ratio(
                    "${mean_wall_ms}"
                    "${case_c_baseline_mean}"
                    "${measurement_scope}"
                    "${case_c_measurement_scope}"
                    relative_to_c)
        endif ()

        if (status STREQUAL "PASS")
            set(markdown_mean_wall "${mean_wall_ms}")
            set(markdown_median_wall "${median_wall_ms}")
            set(markdown_min_wall "${min_wall_ms}")
            set(markdown_max_wall "${max_wall_ms}")
            set(markdown_stddev_wall "${stddev_wall_ms}")
            set(markdown_mad_wall "${mad_wall_ms}")
            set(markdown_cv "${coefficient_of_variation}")
            set(markdown_bootstrap "[${bootstrap_low}, ${bootstrap_high}]")
            set(markdown_samples "${sample_count} (+${extra_sample_count}) x ${repetitions}")
            set(markdown_calibration "${calibration_enabled}:${calibration_aggregate_wall_ms}")
            set(markdown_stability "${stability}")
            set(markdown_comparable "${comparable}")
            set(markdown_gate "${gate_eligible}")
            set(markdown_mean_peak "${mean_peak_mib}")
            set(markdown_max_peak "${max_peak_mib}")
            if (relative_to_c STREQUAL "null")
                set(markdown_relative "-")
            else ()
                set(markdown_relative "${relative_to_c}")
            endif ()

            perf_escape_json_string("${implementation_id}" json_impl_id)
            perf_escape_json_string("${language}" json_language)
            perf_escape_json_string("${mode}" json_mode)
            string(JSON json_object SET "${perf_json_text}" id "\"${json_impl_id}\"")
            string(JSON json_object SET "${json_object}" language "\"${json_language}\"")
            string(JSON json_object SET "${json_object}" mode "\"${json_mode}\"")
            string(JSON json_object SET "${json_object}" status "\"PASS\"")
            string(JSON json_object SET "${json_object}" relative_to_c "${relative_to_c}")
            string(JSON json_object SET "${json_object}" note "\"\"")
        else ()
            foreach (markdown_field IN ITEMS
                    mean_wall median_wall min_wall max_wall stddev_wall mad_wall cv bootstrap samples calibration
                    stability comparable gate mean_peak max_peak relative)
                set("markdown_${markdown_field}" "-")
            endforeach ()
            perf_escape_json_string("${implementation_id}" json_impl_id)
            perf_escape_json_string("${implementation_name}" json_impl_name)
            perf_escape_json_string("${language}" json_language)
            perf_escape_json_string("${mode}" json_mode)
            perf_escape_json_string("${measurement_scope}" json_measurement_scope)
            perf_escape_json_string("${prepare_scope}" json_prepare_scope)
            perf_escape_json_string("${working_directory}" json_workdir)
            perf_escape_json_string("${note}" json_note)
            perf_json_array_from_list(json_command ${command_list})
            string(CONCAT json_object
                    "{\n"
                    "  \"id\": \"${json_impl_id}\",\n"
                    "  \"name\": \"${json_impl_name}\",\n"
                    "  \"language\": \"${json_language}\",\n"
                    "  \"mode\": \"${json_mode}\",\n"
                    "  \"status\": \"${status}\",\n"
                    "  \"measurement_scope\": \"${json_measurement_scope}\",\n"
                    "  \"prepare_scope\": \"${json_prepare_scope}\",\n"
                    "  \"runtime_reused\": ${runtime_reused},\n"
                    "  \"compiler_reused\": ${compiler_reused},\n"
                    "  \"jit_state_reused\": ${jit_state_reused},\n"
                    "  \"command\": ${json_command},\n"
                    "  \"working_directory\": \"${json_workdir}\",\n"
                    "  \"iterations\": null, \"sample_count\": 0, \"extra_sample_count\": 0, \"repetitions\": null,\n"
                    "  \"calibration\": {\"enabled\": false, \"min_sample_ms\": null, \"aggregate_wall_ms\": null, \"repetitions\": 1},\n"
                    "  \"stability\": \"NOT_MEASURED\", \"comparable\": false, \"gate_eligible\": false,\n"
                    "  \"runs\": [],\n"
                    "  \"summary\": null,\n"
                    "  \"relative_to_c\": null,\n"
                    "  \"note\": \"${json_note}\"\n"
                    "}")
        endif ()

        string(APPEND PERF_MARKDOWN_ROWS
                "| ${case_name} | ${implementation_name} | ${language} | ${status} | ${measurement_scope} | ${prepare_scope} | ${runtime_reused} | ${compiler_reused} | ${jit_state_reused} | ${markdown_mean_wall} | ${markdown_median_wall} | ${markdown_min_wall} | ${markdown_max_wall} | ${markdown_stddev_wall} | ${markdown_mad_wall} | ${markdown_cv} | ${markdown_bootstrap} | ${markdown_samples} | ${markdown_calibration} | ${markdown_stability} | ${markdown_comparable} | ${markdown_gate} | ${markdown_mean_peak} | ${markdown_max_peak} | ${markdown_relative} |\n")
        if (case_impl_jsons STREQUAL "")
            set(case_impl_jsons "${json_object}")
        else ()
            set(case_impl_jsons "${case_impl_jsons},\n${json_object}")
        endif ()
    endforeach ()

    perf_escape_json_string("${case_name}" json_case_name)
    perf_escape_json_string("${case_description}" json_case_description)
    perf_escape_json_string("${case_banner}" json_case_banner)
    perf_escape_json_string("${ZR_VM_BENCHMARK_WORKLOAD_TAG_${case_name}}" json_case_workload_tag)

    if (NOT case_interp_mean STREQUAL "")
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_c_baseline_mean}" "${case_interp_measurement_scope}" "${case_c_measurement_scope}" ratio_to_c)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_lua_mean}" "${case_interp_measurement_scope}" "${case_lua_measurement_scope}" ratio_to_lua)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_qjs_mean}" "${case_interp_measurement_scope}" "${case_qjs_measurement_scope}" ratio_to_qjs)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_node_mean}" "${case_interp_measurement_scope}" "${case_node_measurement_scope}" ratio_to_node)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_python_mean}" "${case_interp_measurement_scope}" "${case_python_measurement_scope}" ratio_to_python)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_dotnet_mean}" "${case_interp_measurement_scope}" "${case_dotnet_measurement_scope}" ratio_to_dotnet)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_java_mean}" "${case_interp_measurement_scope}" "${case_java_measurement_scope}" ratio_to_java)
        zr_benchmark_measurement_contract_ratio("${case_interp_mean}" "${case_rust_mean}" "${case_interp_measurement_scope}" "${case_rust_measurement_scope}" ratio_to_rust)
        foreach (ratio_var IN ITEMS ratio_to_c ratio_to_lua ratio_to_qjs ratio_to_node ratio_to_python ratio_to_dotnet ratio_to_java ratio_to_rust)
            if (${ratio_var} STREQUAL "null")
                set(${ratio_var}_json "null")
            else ()
                set(${ratio_var}_json "\"${${ratio_var}}\"")
            endif ()
        endforeach ()
        foreach (ratio_var IN ITEMS ratio_to_c ratio_to_lua ratio_to_qjs ratio_to_node ratio_to_python ratio_to_dotnet ratio_to_java ratio_to_rust)
            if (${ratio_var} STREQUAL "null")
                set(${ratio_var} "-")
            endif ()
        endforeach ()

        string(APPEND PERF_COMPARISON_MARKDOWN_ROWS
                "| ${case_name} | ${ZR_VM_BENCHMARK_WORKLOAD_TAG_${case_name}} | ${ratio_to_c} | ${ratio_to_lua} | ${ratio_to_qjs} | ${ratio_to_node} | ${ratio_to_python} | ${ratio_to_dotnet} | ${ratio_to_java} | ${ratio_to_rust} |\n")
        string(CONCAT comparison_case_json
                "    {\n"
                "      \"name\": \"${json_case_name}\",\n"
                "      \"workload_tag\": \"${json_case_workload_tag}\",\n"
                "      \"measurement_scope\": \"${case_interp_measurement_scope}\",\n"
                "      \"relative_to\": {\n"
                "        \"c\": ${ratio_to_c_json},\n"
                "        \"lua\": ${ratio_to_lua_json},\n"
                "        \"qjs\": ${ratio_to_qjs_json},\n"
                "        \"node\": ${ratio_to_node_json},\n"
                "        \"python\": ${ratio_to_python_json},\n"
                "        \"dotnet\": ${ratio_to_dotnet_json},\n"
                "        \"java\": ${ratio_to_java_json},\n"
                "        \"rust\": ${ratio_to_rust_json}\n"
                "      }\n"
                "    }")
        if (PERF_COMPARISON_JSON_CASES STREQUAL "")
            set(PERF_COMPARISON_JSON_CASES "${comparison_case_json}")
        else ()
            set(PERF_COMPARISON_JSON_CASES "${PERF_COMPARISON_JSON_CASES},\n${comparison_case_json}")
        endif ()
    endif ()

    perf_case_is_hotspot_representative("${case_name}" case_hotspot_representative)

    if (NOT case_profile_report_path STREQUAL "" AND EXISTS "${case_profile_report_path}")
        string(APPEND PERF_INSTRUCTION_MARKDOWN_ROWS
                "| ${case_name} | available | `${case_profile_report_path}` |\n")
        file(READ "${case_profile_report_path}" case_profile_json_text)
        string(STRIP "${case_profile_json_text}" case_profile_json_text)
        if (PERF_INSTRUCTION_JSON_CASES STREQUAL "")
            set(PERF_INSTRUCTION_JSON_CASES "${case_profile_json_text}")
        else ()
            set(PERF_INSTRUCTION_JSON_CASES "${PERF_INSTRUCTION_JSON_CASES},\n${case_profile_json_text}")
        endif ()
        if (case_hotspot_representative AND
                PERF_VALGRIND_EXE AND
                PERF_CALLGRIND_ANNOTATE_EXE AND
                PERF_PYTHON_EXE AND
                case_interp_ready AND
                NOT case_interp_working_directory STREQUAL "")
            set(case_callgrind_out_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.callgrind.out")
            set(case_callgrind_annotate_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.callgrind.annotate.txt")
            set(case_hotspot_summary_json_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.hotspot.json")
            set(case_hotspot_summary_md_path "${PERF_REPORT_DIR}/${case_name}__zr_interp.hotspot.md")
            set(_perf_callgrind_cmd
                    "${PERF_VALGRIND_EXE}"
                    "--tool=callgrind"
                    "--trace-children=no"
                    "--callgrind-out-file=${case_callgrind_out_path}")
            if (PERF_CALLGRIND_COUNTING_MODE)
                list(APPEND _perf_callgrind_cmd "--cache-sim=no" "--branch-sim=no")
            endif ()
            list(APPEND _perf_callgrind_cmd ${case_interp_command_list})
            execute_process(
                    COMMAND ${_perf_callgrind_cmd}
                    WORKING_DIRECTORY "${case_interp_working_directory}"
                    RESULT_VARIABLE case_callgrind_result
                    OUTPUT_VARIABLE case_callgrind_stdout
                    ERROR_VARIABLE case_callgrind_stderr
                    TIMEOUT 3600)
            if (NOT case_callgrind_result EQUAL 0 OR NOT EXISTS "${case_callgrind_out_path}")
                string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                        "### ${case_name}\n"
                        "- Instruction profile: `${case_profile_report_path}`\n"
                        "- Callgrind: failed during representative workload capture\n\n")
                string(CONCAT hotspot_case_json
                        "    {\n"
                        "      \"name\": \"${json_case_name}\",\n"
                        "      \"status\": \"callgrind_failed\",\n"
                        "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                        "      \"callgrind\": null\n"
                        "    }")
                perf_append_note("failure"
                        "${case_name}"
                        "ZR interp hotspot"
                        "callgrind capture failed.\n${case_callgrind_stdout}${case_callgrind_stderr}")
                set(PERF_HARD_FAILURE TRUE)
            else ()
                execute_process(
                        COMMAND
                        "${PERF_CALLGRIND_ANNOTATE_EXE}"
                        "--auto=no"
                        "--threshold=99"
                        "${case_callgrind_out_path}"
                        RESULT_VARIABLE case_annotate_result
                        OUTPUT_VARIABLE case_annotate_stdout
                        ERROR_VARIABLE case_annotate_stderr
                        TIMEOUT 600)
                if (NOT case_annotate_result EQUAL 0)
                    string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                            "### ${case_name}\n"
                            "- Instruction profile: `${case_profile_report_path}`\n"
                            "- Callgrind: annotate step failed\n\n")
                    string(CONCAT hotspot_case_json
                            "    {\n"
                            "      \"name\": \"${json_case_name}\",\n"
                            "      \"status\": \"callgrind_annotate_failed\",\n"
                            "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                            "      \"callgrind\": null\n"
                            "    }")
                    perf_append_note("failure"
                            "${case_name}"
                            "ZR interp hotspot"
                            "callgrind annotate failed.\n${case_annotate_stdout}${case_annotate_stderr}")
                    set(PERF_HARD_FAILURE TRUE)
                else ()
                    file(WRITE "${case_callgrind_annotate_path}" "${case_annotate_stdout}${case_annotate_stderr}")
                    execute_process(
                            COMMAND
                            "${PERF_PYTHON_EXE}"
                            "${PERF_HOTSPOT_SUMMARY_SCRIPT}"
                            "--case" "${case_name}"
                            "--instruction-profile" "${case_profile_report_path}"
                            "--callgrind-out" "${case_callgrind_out_path}"
                            "--callgrind-annotate" "${case_callgrind_annotate_path}"
                            "--json-out" "${case_hotspot_summary_json_path}"
                            "--markdown-out" "${case_hotspot_summary_md_path}"
                            RESULT_VARIABLE case_hotspot_summary_result
                            OUTPUT_VARIABLE case_hotspot_summary_stdout
                            ERROR_VARIABLE case_hotspot_summary_stderr
                            TIMEOUT 120)
                    if (NOT case_hotspot_summary_result EQUAL 0 OR
                            NOT EXISTS "${case_hotspot_summary_json_path}" OR
                            NOT EXISTS "${case_hotspot_summary_md_path}")
                        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                                "### ${case_name}\n"
                                "- Instruction profile: `${case_profile_report_path}`\n"
                                "- Callgrind: summary generation failed\n\n")
                        string(CONCAT hotspot_case_json
                                "    {\n"
                                "      \"name\": \"${json_case_name}\",\n"
                                "      \"status\": \"hotspot_summary_failed\",\n"
                                "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                                "      \"callgrind\": null\n"
                                "    }")
                        perf_append_note("failure"
                                "${case_name}"
                                "ZR interp hotspot"
                                "hotspot summary generation failed.\n${case_hotspot_summary_stdout}${case_hotspot_summary_stderr}")
                        set(PERF_HARD_FAILURE TRUE)
                    else ()
                        file(READ "${case_hotspot_summary_md_path}" case_hotspot_markdown_text)
                        file(READ "${case_hotspot_summary_json_path}" hotspot_case_json)
                        string(STRIP "${case_hotspot_markdown_text}" case_hotspot_markdown_text)
                        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES "${case_hotspot_markdown_text}\n\n")
                    endif ()
                endif ()
            endif ()
        elseif (case_hotspot_representative)
            string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                    "### ${case_name}\n"
                    "- Instruction profile: `${case_profile_report_path}`\n"
                    "- Callgrind: unavailable (missing valgrind, callgrind_annotate, python, or interp command)\n\n")
            string(CONCAT hotspot_case_json
                    "    {\n"
                    "      \"name\": \"${json_case_name}\",\n"
                    "      \"status\": \"instruction_profile_available\",\n"
                    "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                    "      \"callgrind\": null\n"
                    "    }")
        else ()
            string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                    "### ${case_name}\n"
                    "- Instruction profile: `${case_profile_report_path}`\n"
                    "- Callgrind: not captured in this tier (representative profile cases only)\n\n")
            string(CONCAT hotspot_case_json
                    "    {\n"
                    "      \"name\": \"${json_case_name}\",\n"
                    "      \"status\": \"instruction_profile_available\",\n"
                    "      \"instruction_profile\": \"${case_profile_report_path}\",\n"
                    "      \"callgrind\": null\n"
                    "    }")
        endif ()
    else ()
        string(APPEND PERF_INSTRUCTION_MARKDOWN_ROWS
                "| ${case_name} | missing | - |\n")
        string(APPEND PERF_HOTSPOT_MARKDOWN_CASES
                "### ${case_name}\n"
                "- Instruction profile: unavailable\n"
                "- Callgrind: unavailable in this run\n\n")
        string(CONCAT hotspot_case_json
                "    {\n"
                "      \"name\": \"${json_case_name}\",\n"
                "      \"status\": \"unavailable\",\n"
                "      \"instruction_profile\": null,\n"
                "      \"callgrind\": null\n"
                "    }")
    endif ()
    if (PERF_HOTSPOT_JSON_CASES STREQUAL "")
        set(PERF_HOTSPOT_JSON_CASES "${hotspot_case_json}")
    else ()
        set(PERF_HOTSPOT_JSON_CASES "${PERF_HOTSPOT_JSON_CASES},\n${hotspot_case_json}")
    endif ()

    string(CONCAT case_json
            "    {\n"
            "      \"name\": \"${json_case_name}\",\n"
            "      \"description\": \"${json_case_description}\",\n"
            "      \"workload_tag\": \"${json_case_workload_tag}\",\n"
            "      \"pass_banner\": \"${json_case_banner}\",\n"
            "      \"tier\": \"${PERF_REQUESTED_TIER}\",\n"
            "      \"scale\": ${case_scale},\n"
            "      \"min_sample_ms\": ${case_min_sample_ms},\n"
            "      \"expected_checksum\": ${case_checksum},\n"
            "      \"implementations\": [\n${case_impl_jsons}\n      ]\n"
            "    }")

    if (PERF_JSON_CASES STREQUAL "")
        set(PERF_JSON_CASES "${case_json}")
    else ()
        set(PERF_JSON_CASES "${PERF_JSON_CASES},\n${case_json}")
    endif ()
endforeach ()
