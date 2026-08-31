function(zr_benchmark_persistent_command_get
        implementation_id
        case_name
        tier
        out_supported
        out_command
        out_prepare_scope
        out_runtime_reused
        out_compiler_reused
        out_jit_state_reused)
    set(supported FALSE)
    set(command "")
    set(prepare_scope "")
    set(runtime_reused "false")
    set(compiler_reused "false")
    set(jit_state_reused "false")

    if ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
        implementation_id STREQUAL "lua" AND PERF_LUA_EXE AND NOT PERF_LUA_IS_LUAJIT)
        set(command "${PERF_LUA_EXE};${BENCHMARKS_DIR}/cases/${case_name}/lua/main.lua;--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "script_load_before_measurement")
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "qjs" AND PERF_QJS_EXE)
        set(command "${PERF_QJS_EXE};-m;${BENCHMARKS_DIR}/cases/${case_name}/qjs/main.js;--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "script_load_before_measurement")
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "dotnet" AND PERF_DOTNET_RUNNER_DLL AND PERF_DOTNET_EXE)
        set(command "${PERF_DOTNET_EXE};${PERF_DOTNET_RUNNER_DLL};--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "runtime_start_before_measurement")
        set(jit_state_reused "true")
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "zr_binary" AND ZR_BENCHMARK_SERVER_EXE AND zr_project_file)
        set(command "${ZR_BENCHMARK_SERVER_EXE};--benchmark-server;--project;${zr_project_file};--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "bytecode_compile_and_load_before_measurement")
    endif ()

    if (supported)
        set(runtime_reused "true")
    endif ()
    set(${out_supported} "${supported}" PARENT_SCOPE)
    set(${out_command} "${command}" PARENT_SCOPE)
    set(${out_prepare_scope} "${prepare_scope}" PARENT_SCOPE)
    set(${out_runtime_reused} "${runtime_reused}" PARENT_SCOPE)
    set(${out_compiler_reused} "${compiler_reused}" PARENT_SCOPE)
    set(${out_jit_state_reused} "${jit_state_reused}" PARENT_SCOPE)
endfunction()
