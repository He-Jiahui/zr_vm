# 为主套件和 support 契约测试统一描述各实现的计时边界；未知实现留空，交由调用方判为不可比较。
# ## Summary
# 将 implementation_id 映射为 process runner 使用的 measurement_scope、prepare_scope 和三个 reuse 字段；调用者随后负责协议校验与命令执行。
# ## Parameters
# implementation_id：已知实现标识 c、rust、zr_interp、zr_binary、python、node、qjs、lua、dotnet 或 java。
# out_measurement_scope、out_prepare_scope、out_runtime_reused、out_compiler_reused、out_jit_state_reused：调用者作用域中的输出变量名。
# ## Returns
# 无函数返回值；通过 PARENT_SCOPE 写回五个输出变量。未知 id 保持空值，调用者必须 fail closed。
# ## Constraints and lifecycle
# 只描述 process 测量口径，不启动进程、不探测宿主工具；应在 runner 命令构造前调用并交给 validate 复核。persistent_runtime 由独立 selector 成功后由 suite 替换。
# ## References
# tests/cmake/run_performance_suite.cmake:792; tests/cmake/run_benchmark_support_test.cmake:148
function(zr_benchmark_measurement_contract_get
        implementation_id
        out_measurement_scope
        out_prepare_scope
        out_runtime_reused
        out_compiler_reused
        out_jit_state_reused)
    set(measurement_scope "")
    set(prepare_scope "")

# ## Native mapping block
# c/rust 使用 process_end_to_end 和 none；reuse flags 在函数尾部写 false。
    if (implementation_id STREQUAL "c" OR implementation_id STREQUAL "rust")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "none")
# ## Interpreter mapping block
# source load/compile 留在被计时进程内；该文本必须与 run_performance_suite 的命令口径一致。
    elseif (implementation_id STREQUAL "zr_interp")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "source_load_compile_in_measurement")
# ## Binary mapping block
# bytecode compile 发生在 measurement 前；steady selector 若选中 ZR server 会改用 persistent 口径，此函数不执行 CLI compile。
    elseif (implementation_id STREQUAL "zr_binary")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "bytecode_compile_before_measurement")
# ## Script mapping block
# Python、Node、QuickJS、Lua 均先映射为 process；persistent_runtime 只由独立 selector 在支持组合中产生。
    elseif (implementation_id STREQUAL "python" OR
            implementation_id STREQUAL "node" OR
            implementation_id STREQUAL "qjs" OR
            implementation_id STREQUAL "lua")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "script_load_in_measurement")
# ## Managed mapping block
# .NET/Java 将 runtime start/JIT 放入 process measurement；本分支不声明 persistent 复用，selector 的独立输出由 suite 另行处理。
    elseif (implementation_id STREQUAL "dotnet" OR implementation_id STREQUAL "java")
        set(measurement_scope "process_end_to_end")
        set(prepare_scope "runtime_start_jit_in_measurement")
    endif ()

    set(${out_measurement_scope} "${measurement_scope}" PARENT_SCOPE)
    set(${out_prepare_scope} "${prepare_scope}" PARENT_SCOPE)
# ## Unknown-id fallback block
# 未知实现的 scope 和复用字段均保持空值，避免空映射伪造 false。
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

# 检查 runner 报告中的计时范围和复用标志是否属于协议词汇；不替调用方决定某次运行是否可信。
# ## Summary
# 校验 scope、prepare_scope 与 reuse 文本是否属于共享协议词汇；格式合法不代表运行本身可信。
# ## Parameters
# measurement_scope：process_end_to_end 或 persistent_runtime。
# prepare_scope：valid_prepare_scopes 列表中的值。
# runtime_reused、compiler_reused、jit_state_reused：必须是小写 true 或 false。
# out_valid：调用者作用域的输出变量名。
# ## Returns
# 无函数返回值；通过 PARENT_SCOPE 写回 TRUE/FALSE。任何非法字段输出 FALSE。
# ## Constraints and lifecycle
# 不读取命令、JSON 或进程状态；主套件在 runner 调用前使用，support 测试另行覆盖 persistent 词汇。
# ## References
# tests/cmake/run_performance_suite.cmake:799; tests/cmake/run_benchmark_support_test.cmake:178
function(zr_benchmark_measurement_contract_validate
        measurement_scope
        prepare_scope
        runtime_reused
        compiler_reused
        jit_state_reused
        out_valid)
    set(valid TRUE)
# ## Measurement scope gate
# 仅接受协议定义的两个顶层计时范围。
    if (NOT measurement_scope STREQUAL "process_end_to_end" AND
            NOT measurement_scope STREQUAL "persistent_runtime")
        set(valid FALSE)
    endif ()

# ## Prepare scope vocabulary
# process/persistent 的准备边界集中维护；新增词必须同步 runner、报告消费者和 support fixture。
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

# ## Reuse flag gate
# 三项复用声明逐一限制为 canonical lowercase boolean。
    foreach (reuse_value IN ITEMS "${runtime_reused}" "${compiler_reused}" "${jit_state_reused}")
        if (NOT reuse_value STREQUAL "true" AND NOT reuse_value STREQUAL "false")
            set(valid FALSE)
        endif ()
    endforeach ()
    set(${out_valid} ${valid} PARENT_SCOPE)
endfunction()

# 比率计算只接受 runner 报告使用的三位小数，避免 CMake 浮点运算产生平台差异。
# ## Summary
# 将报告中的固定三位小数毫秒文本转换为整数千分之一毫秒，供 ratio 使用 CMake 整数运算。
# ## Parameters
# value：完整匹配非负整数和三位小数的毫秒文本；out_var：调用者作用域输出变量名。
# ## Returns
# 通过 PARENT_SCOPE 写入毫秒的千分之一整数刻度；格式不符时写空字符串。
# ## Constraints and lifecycle
# 不接受符号、指数、缺位或额外字符；调用者应把空值作为协议解析失败处理。
# TODO: JSON writer 用 %.3f 序列化均值，未给出毫秒均值上界；下一步核对 tests/performance/perf_report.c:243 与 tests/cmake/run_performance_suite.cmake:1190 的统计/解析约束，并在 tests/cmake/run_benchmark_support_test.cmake:188 加边界 fixture，不据此标 BUG。
# ## References
# tests/cmake/benchmark_measurement_contract.cmake:203; tests/cmake/benchmark_measurement_contract.cmake:204
function(zr_benchmark_measurement_contract_decimal_to_milli value out_var)
# ## Strict decimal guard
# 正则失败立即输出空值，只有三位小数文本进入整数计算。
    string(REGEX MATCH "^([0-9]+)\\.([0-9][0-9][0-9])$" matched "${value}")
    if (matched STREQUAL "")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif ()
    math(EXPR milli "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2}")
    set(${out_var} "${milli}" PARENT_SCOPE)
endfunction()

# 与 decimal_to_milli 配套，确保报告中的比率保持固定三位小数。
# ## Summary
# 将 ratio 的无量纲千分刻度格式化为固定三位小数文本，与整数 ratio 运算配套。
# ## Parameters
# milli_value：调用方已计算的无量纲 ratio 千分刻度整数；out_var：调用者作用域输出变量名。
# ## Returns
# 通过 PARENT_SCOPE 写回无量纲 whole.fraction 文本；不启动进程、不写文件，输出变量仍改变调用者作用域。
# ## Constraints and lifecycle
# 只负责显示无量纲 ratio 的固定三位小数，不验证负值或来源；必须在整数 ratio 运算后调用。
# ## References
# tests/cmake/benchmark_measurement_contract.cmake:211
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

# case assembly 仅比较同一计时范围的结果；缺值、零基线或口径不同都输出 JSON null。
# ## Summary
# 在 case assembly 中计算同一 measurement scope 的实现/基线 ratio；不满足门禁时输出 JSON null。
# ## Parameters
# value、base：固定三位小数均值文本；value_measurement_scope、base_measurement_scope：必须相等的口径；out_var：输出变量名。
# ## Returns
# 通过 PARENT_SCOPE 写回固定三位小数文本或 null；不修改输入报告，不保留跨调用状态。
# ## Constraints and lifecycle
# 缺值、scope mismatch、非法小数和零基线 fail closed；PASS/STABLE/gate eligibility 由 suite 上游负责。
# TODO: JSON writer 用 %.3f 序列化均值，未给出 value_milli * 1000 的上界；下一步核对 tests/performance/perf_report.c:243 与 tests/cmake/run_performance_suite.cmake:1190 的统计/解析约束，并在 tests/cmake/run_benchmark_support_test.cmake:188 加边界 fixture，不据此标 BUG。
# ## References
# tests/cmake/benchmark_task3_case_assembly.cmake:97; tests/cmake/benchmark_task3_case_assembly.cmake:198; tests/cmake/benchmark_task3_case_assembly.cmake:199; tests/cmake/benchmark_task3_case_assembly.cmake:200; tests/cmake/benchmark_task3_case_assembly.cmake:201; tests/cmake/benchmark_task3_case_assembly.cmake:202; tests/cmake/benchmark_task3_case_assembly.cmake:203; tests/cmake/benchmark_task3_case_assembly.cmake:204; tests/cmake/benchmark_task3_case_assembly.cmake:205; tests/cmake/run_benchmark_support_test.cmake:188; tests/cmake/run_benchmark_support_test.cmake:190; tests/cmake/run_benchmark_support_test.cmake:192
function(zr_benchmark_measurement_contract_ratio
        value
        base
        value_measurement_scope
        base_measurement_scope
        out_var)
# ## Missing/scope gate
# 缺值或跨 process/persistent scope 直接输出 null，不进入数值转换。
    if (value STREQUAL "" OR
            base STREQUAL "" OR
            value_measurement_scope STREQUAL "" OR
            base_measurement_scope STREQUAL "" OR
            NOT value_measurement_scope STREQUAL base_measurement_scope)
        set(${out_var} "null" PARENT_SCOPE)
        return()
    endif ()

# ## Parse and baseline gate
# 两个均值按同一严格格式解析，基线至少为一个整数刻度单位。
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
