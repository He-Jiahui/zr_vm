# 主套件按 case、实现及已发现的宿主可执行文件选择持久会话命令。
# 未支持的组合返回 FALSE；返回命令是 CMake 列表，调用方须以列表参数传给进程启动器。
# ## Summary
# 按 case、implementation 和 tier 选择 steady benchmark 的持久命令，并返回准备范围与复用声明；只有 Lua、QuickJS、.NET、ZR binary 的支持分支可请求 persistent。
# ## Parameters
# implementation_id：lua、qjs、dotnet 或 zr_binary；case_name：当前仅 numeric_loops 或 dispatch_loops；tier：原样放入协议参数。
# out_supported、out_command、out_prepare_scope、out_runtime_reused、out_compiler_reused、out_jit_state_reused：调用者作用域输出变量名。
# 调用作用域还须提供 PERF_LUA_EXE/PERF_LUA_IS_LUAJIT、PERF_QJS_EXE、
# PERF_DOTNET_EXE/PERF_DOTNET_RUNNER_DLL、ZR_BENCHMARK_SERVER_EXE、zr_project_file
# 以及 BENCHMARKS_DIR；按分支读取 CMake truth 条件，不校验路径存在，也不保存跨调用资源。
# ## Returns
# 无函数返回值；通过 PARENT_SCOPE 写回六个输出变量。不可支持组合返回 FALSE、空 command、空 prepare 和 false 默认 flags。
# ## Constraints and lifecycle
# command 是 CMake 分号列表，调用方必须按列表参数启动；函数不执行外部程序。unsupported 必须 SKIP，不能回退 process。
# ## References
# tests/cmake/run_performance_suite.cmake:1054; tests/cmake/run_perf_runner_persistent_protocol_test.cmake:23; tests/cmake/run_perf_runner_persistent_protocol_test.cmake:31; tests/cmake/run_perf_runner_persistent_protocol_test.cmake:37; tests/cmake/run_perf_runner_persistent_protocol_test.cmake:43; tests/cmake/run_perf_runner_persistent_protocol_test.cmake:51
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

# ## Lua command block
# Lua 仅在 PERF_LUA_EXE 非空且 PERF_LUA_IS_LUAJIT 为 false 时支持 script_load_before_measurement。
    if ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
        implementation_id STREQUAL "lua" AND PERF_LUA_EXE AND NOT PERF_LUA_IS_LUAJIT)
        set(command "${PERF_LUA_EXE};${BENCHMARKS_DIR}/cases/${case_name}/lua/main.lua;--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "script_load_before_measurement")
# ## QuickJS command block
# QuickJS 用 -m 启动 server；PERF_QJS_EXE 为空时保持 unsupported。
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "qjs" AND PERF_QJS_EXE)
        set(command "${PERF_QJS_EXE};-m;${BENCHMARKS_DIR}/cases/${case_name}/qjs/main.js;--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "script_load_before_measurement")
# ## .NET command block
# .NET 需要 PERF_DOTNET_EXE 与 PERF_DOTNET_RUNNER_DLL 均非空；selector 初始声明 jit_state_reused=true，主套件会按 warmup/calibration 重新计算并可能覆盖该字段。
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "dotnet" AND PERF_DOTNET_RUNNER_DLL AND PERF_DOTNET_EXE)
        set(command "${PERF_DOTNET_EXE};${PERF_DOTNET_RUNNER_DLL};--benchmark-server;--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "runtime_start_before_measurement")
        set(jit_state_reused "true")
# ## ZR binary command block
# ZR server 携带非空 ZR_BENCHMARK_SERVER_EXE、zr_project_file、case、tier；bytecode compile/load 在 measurement 前，compiler/JIT false。
    elseif ((case_name STREQUAL "numeric_loops" OR case_name STREQUAL "dispatch_loops") AND
            implementation_id STREQUAL "zr_binary" AND ZR_BENCHMARK_SERVER_EXE AND zr_project_file)
        set(command "${ZR_BENCHMARK_SERVER_EXE};--benchmark-server;--project;${zr_project_file};--case;${case_name};--tier;${tier}")
        set(supported TRUE)
        set(prepare_scope "bytecode_compile_and_load_before_measurement")
    endif ()

# ## Supported-result block
# 仅支持组合声明 runtime_reused=true；其他 flags 保留分支默认值。
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
