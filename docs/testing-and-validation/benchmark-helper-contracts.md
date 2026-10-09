---
related_code:
  - tests/cmake/benchmark_measurement_contract.cmake
  - tests/cmake/benchmark_persistent_commands.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_task3_case_assembly.cmake
  - tests/cmake/run_benchmark_support_test.cmake
  - tests/cmake/run_perf_runner_persistent_protocol_test.cmake
  - tests/CMakeLists.txt
  - tests/benchmarks/registry.cmake
  - tests/performance/perf_runner.c
implementation_files:
  - tests/cmake/benchmark_measurement_contract.cmake
  - tests/cmake/benchmark_persistent_commands.cmake
plan_sources:
  - docs/plans/benchmark/optimize/01-measurement-and-gates.md
  - tests/acceptance/2026-08-29-benchmark-measurement-scope.md
tests:
  - tests/cmake/run_benchmark_support_test.cmake
  - tests/cmake/run_perf_runner_persistent_protocol_test.cmake
doc_type: module-detail
---

# 基准计时与持久命令 CMake 契约

## Summary

`benchmark_measurement_contract.cmake` 为性能套件和 support 契约测试提供统一的实现计时映射、字段词汇校验和同口径比率计算。`zr_benchmark_measurement_contract_get` 在套件的实现循环（`tests/cmake/run_performance_suite.cmake:792`）和 support 映射断言（`tests/cmake/run_benchmark_support_test.cmake:148`）中写出 process 口径；steady 选择成功后，套件在 `tests/cmake/run_performance_suite.cmake:1054` 由另一个 selector 替换为 persistent 口径。

`benchmark_persistent_commands.cmake` 只选择持久 server 命令，不启动进程。selector 支持 `numeric_loops`、`dispatch_loops` 中的 Lua、QuickJS、.NET 和 ZR binary，其中 Lua 另要求 `PERF_LUA_IS_LUAJIT` 为假；该标志当前由 suite 将可执行文件名包含 `luajit` 的情况设为真（`tests/cmake/run_performance_suite.cmake:543`）。缺少工具、项目或不支持的 case 返回 unsupported，套件在 `tests/cmake/run_performance_suite.cmake:1088` 标记 `SKIP`。QuickJS 没有 focused 直接调用，实际生产调用仍是 suite selector；协议脚本先在 `tests/cmake/run_perf_runner_persistent_protocol_test.cmake:30` 设置该排除输入，再由 `tests/cmake/run_perf_runner_persistent_protocol_test.cmake:31` 的 selector 调用核对。

## Parameters

六个函数的输入和输出变量名均由调用者传入，输出通过 `PARENT_SCOPE` 写回。映射函数接收 implementation id；validate 接收 measurement/prepare scope 和三个小写复用字段；decimal helper 接收 runner 报告中的非负整数加三位小数毫秒文本；format helper 接收 ratio 的无量纲千分刻度整数。ratio 接收实现值、基线值及各自 measurement scope。persistent selector 接收 implementation、case、tier 以及由调用方发现的宿主路径，并返回一组输出变量；command 是分号分隔的 CMake 列表，suite 将展开后的元素传给 runner。

## Returns

- `get` 写出 process measurement/prepare scope 和三个复用字段；未知实现保留空 scope 与空复用值。
- `validate` 写出 `TRUE` 或 `FALSE`，只检查协议词汇，不证明一次运行的真实性。
- `decimal_to_milli` 将固定三位小数毫秒扩展为整数千分之一毫秒刻度；格式失败写空值。`1.008` 的合法结果是 `1008`，不属于前导零八进制问题。
- `format_milli` 把无量纲 ratio 的千分刻度格式化为固定三位小数文本，例如 `500` 变为 `0.500`。
- `ratio` 在缺值、scope 不一致、格式非法或基线为零时写 JSON `null`；通过整数舍入后写 ratio 文本。
- `zr_benchmark_persistent_command_get` 写出 support 状态、command、prepare scope 和三个复用字段。unsupported 组合返回 `FALSE`、空 command/prepare 和三个 `false`；支持分支把 `runtime_reused` 设为 `true`，.NET 的 `jit_state_reused=true` 初值随后由套件在 `tests/cmake/run_performance_suite.cmake:1075` 按 warmup/calibration 重算。

## Constraints and lifecycle

两个模块都只处理值和命令列表，不读取 JSON、不探测文件、不启动外部工具。suite 在 `tests/cmake/run_performance_suite.cmake:1113` 调用 runner，并在 `tests/cmake/run_performance_suite.cmake:1190` 读取均值；ratio 的上游负责 PASS、STABLE、comparable 和 gate eligibility。TODO: runner 在 `tests/performance/perf_runner.c:619` 调用 JSON writer；`tests/performance/perf_report.c:243` 将均值写为 `%.3f`，`tests/cmake/benchmark_task3_suite.cmake:200` 从 summary 取值，suite 再在 `tests/cmake/run_performance_suite.cmake:1190` 消费。当前没有证明均值或 `value_milli * 1000` 的整数上界；应沿这条生产和解析链及 support fixture（`tests/cmake/run_benchmark_support_test.cmake:188`）核查，不能据此标记 BUG。持久 selector 的 command 必须按列表元素传给进程启动器，unsupported 不能回退到 process 计时。

## References

- `tests/cmake/run_performance_suite.cmake:27` 与 `tests/cmake/run_performance_suite.cmake:28` 引入两个helper；`tests/cmake/run_performance_suite.cmake:792` 和 `tests/cmake/run_performance_suite.cmake:799` 调用映射与校验。
- `tests/cmake/run_performance_suite.cmake:1054` 选择持久命令；`tests/cmake/run_performance_suite.cmake:1075` 重算.NET JIT复用；`tests/cmake/run_performance_suite.cmake:1089` 写入unsupported的SKIP结果。
- `tests/cmake/benchmark_task3_case_assembly.cmake:97` 计算单项比率；`tests/cmake/benchmark_task3_case_assembly.cmake:198`、`tests/cmake/benchmark_task3_case_assembly.cmake:199`、`tests/cmake/benchmark_task3_case_assembly.cmake:200`、`tests/cmake/benchmark_task3_case_assembly.cmake:201`、`tests/cmake/benchmark_task3_case_assembly.cmake:202`、`tests/cmake/benchmark_task3_case_assembly.cmake:203`、`tests/cmake/benchmark_task3_case_assembly.cmake:204`、`tests/cmake/benchmark_task3_case_assembly.cmake:205` 计算各比较项。
- `tests/cmake/run_benchmark_support_test.cmake:142` 引入映射helper；`tests/cmake/run_benchmark_support_test.cmake:148`、`tests/cmake/run_benchmark_support_test.cmake:178`、`tests/cmake/run_benchmark_support_test.cmake:188`、`tests/cmake/run_benchmark_support_test.cmake:190`、`tests/cmake/run_benchmark_support_test.cmake:192` 断言映射、词汇、正常比率、口径不匹配和缺失口径。
- `tests/cmake/run_perf_runner_persistent_protocol_test.cmake:14` 引入selector；`tests/cmake/run_perf_runner_persistent_protocol_test.cmake:23`、`tests/cmake/run_perf_runner_persistent_protocol_test.cmake:31`、`tests/cmake/run_perf_runner_persistent_protocol_test.cmake:37`、`tests/cmake/run_perf_runner_persistent_protocol_test.cmake:43`、`tests/cmake/run_perf_runner_persistent_protocol_test.cmake:51` 分别检查Lua、LuaJIT排除、.NET、ZR及unsupported；没有focused QuickJS调用。
- `tests/CMakeLists.txt:8675` 与 `tests/CMakeLists.txt:8692` 只传入module路径到CTest注册命令，不证明测试已执行。
- `tests/benchmarks/registry.cmake:154` 与 `tests/benchmarks/registry.cmake:166` 注册当前允许的numeric_loops和dispatch_loops。

现有focused断言覆盖正常比率、scope mismatch及缺失scope；没有在该块断言零基线或非法小数。当前文档说明实现门禁而不扩大测试覆盖。本次注释审查不表示benchmark进程已运行。
