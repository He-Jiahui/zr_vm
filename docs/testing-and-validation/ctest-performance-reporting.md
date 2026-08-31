---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_persistent_commands.cmake
  - tests/performance/perf_runner.c
  - tests/performance/persistent_protocol.c
  - tests/performance/perf_report.c
  - tests/benchmarks/zr_runner/benchmark_server.c
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
  - tests/TEST_EXECUTION_ORDER.md
implementation_files:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_persistent_commands.cmake
  - tests/performance/perf_runner.c
  - tests/performance/persistent_protocol.c
  - tests/performance/perf_report.c
  - tests/benchmarks/zr_runner/benchmark_server.c
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
plan_sources:
  - user: 2026-04-07 benchmark 集合迁入 tests/benchmarks 并比较 ZR/C/其他语言
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/performance/test_perf_runner_persistent_protocol.c
  - tests/cmake/run_perf_runner_persistent_protocol_test.cmake
  - tests/cmake/run_benchmark_task3_suite_contract_test.cmake
  - tests/benchmarks/test_benchmark_execution_plan.py
  - tests/benchmarks/test_benchmark_statistics.py
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
doc_type: testing-guide
---

# CTest Performance Reporting

## Purpose

`performance_report` 是专门的 benchmark/reporting suite，不承担功能回归阈值判断。

默认 **不会** 把它注册进 CTest：在 `tests/CMakeLists.txt` 中 `ZR_VM_REGISTER_PERFORMANCE_CTEST` 默认为 `OFF`，日常 `ctest` 只跑语言与功能回归；长耗时跨语言性能任务在需要时显式开启或改用构建目标 `run_performance_suite`（见下文「Validation Commands」）。

它的职责只有四件事：

- 从 `tests/benchmarks` 发现 benchmark case 和实现矩阵
- 为各实现做非测量阶段的准备工作
- 先过 correctness gate，再做运行时采样
- 产出统一的 Markdown/JSON 报告

## Source Of Truth

benchmark 定义不再散落在 `tests/fixtures/projects/benchmark_*`。

现在唯一事实来源是：

- `tests/benchmarks/registry.cmake`
- `tests/benchmarks/cases/<case>/...`

其中：

- `registry.cmake`
  负责 case 名称、tier、支持的实现、pass banner 与 per-tier checksum
- `cases/<case>/zr/`
  负责 ZR 项目版本
- `cases/<case>/{c,python,node,rust,dotnet}/`
  负责跨语言实现入口

## Implementation Matrix

主集合实现为：

- `ZR interp`
- `ZR binary`
- `C`
- `Python`
- `Node.js`
- `Rust`
- `C#/.NET`

当前 suite 按 `execution_plan.json` 的 flat job 顺序执行，不再假设 `C` 先执行。
所有 job 完成后才组装 case 与 `relative_to_c`，因此基线在 shuffle 中的位置不会改变结果。

## Case Set

正式 case 固定为 8 个：

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `sort_array`
- `prime_trial_division`
- `matrix_add_2d`
- `string_build`
- `map_object_access`

tier 规则：

- `smoke`
  - `numeric_loops`
  - `sort_array`
  - `prime_trial_division`
- `core`
  - 全部 8 个 case
- `stress`
  - 全部 8 个 case

scale 固定为：

- `smoke = 1`
- `core = 4`
- `stress = 16`

## Correctness Gate

每个 `case x implementation` 在进入 perf sampling 前，都会先做一次 correctness gate。

通过条件：

- 退出码为 0
- stdout 精确匹配两行：
  - `BENCH_<CASE>_PASS`
  - tier 对应 checksum

只要 correctness gate 失败，该实现会被记为 `FAIL`，报告仍会生成，但 `performance_report` 最终返回失败。

## Preparation Policy

采样只统计运行态，不把准备步骤计入 measured wall time。

当前准备策略：

- `ZR interp`
  - 直接运行生成后的 benchmark fixture
- `ZR binary`
  - 先 `zr_vm_cli --compile <project.zrp>`
  - 再 `--execution-mode binary`
- `C`
  - 由 `tests/CMakeLists.txt` 提前注册 `zr_vm_native_benchmark_runner`
- `Rust`
  - suite 内执行 `cargo build`
- `C#/.NET`
  - suite 内执行 `dotnet build`
- `Python` / `Node.js`
  - 无额外 build step

ZR fixture 会先复制到 `build/.../tests_generated/performance_suite/`，然后覆盖
`src/bench_config.zr` 为当前 tier 的 scale，避免改写仓库跟踪文件。

## Persistent Runtime Protocol

默认 scope 仍是 `process`：每个样本启动新进程，报告
`measurement_scope=process_end_to_end`，三个 reuse 字段均为 `false`。
`ZR_VM_PERF_SCOPE=steady` 只支持 `numeric_loops` 与 `dispatch_loops` 的显式
server 映射；不支持的 case/toolchain 写 `SKIP`，不能回退成 process 样本。

server 先写 `READY <contract>`，然后按顺序接受 `WARMUP <index> <repetitions>`、
`RUN <index> <repetitions>`，并返回 `DONE <index> <checksum>` 或
`ERROR <index> <code>`。父进程严格校验 ASCII、单空格、十进制正整数、
请求顺序和 checksum。`STOP` 后要求子进程在 deadline 内干净退出。

steady 报告将每次采样的 PID 保持为同一 session PID；每样本 RSS 为
`null`，仅在 `persistent_session.peak_working_set_bytes` 报告 session 峰值。
ZR binary server 使用同 build CLI 预编译的新 `.zro`，加载入口一次并重复
执行；它不驻留 compiler，因此 `compiler_reused=false`。

## Task 3 Sampling And Eligibility

默认 execution seed 为 `0`，可用 `ZR_VM_PERF_SEED` 覆盖为 unsigned 64-bit
十进制整数。case/implementation 过滤先于版本化 Fisher-Yates/SplitMix64
shuffle；报告嵌入实际执行的完整 plan。

steady 默认 warmup `5`、initial samples `10`。process 保留各 tier 的旧默认。
非 profile 测量使用 registry 的 per-case `MIN_SAMPLE_MS` 做 repetitions 校准，
最多追加 10 个样本，但 initial + extra 始终不超过 20。`ZR_VM_PERF_ITERATIONS`
为 12 时 extra 上限为 8，为 20 时为 0，超过 20 直接失败。

profile 强制 warmup `0`、sample `1`、repetitions `1`、extra `0`，并传
`--profile`；其 `comparable` 与 `gate_eligible` 都为 false。普通结果只有
`STABLE`、`comparable=true`、`gate_eligible=true` 时才能产生 ratio。完整字段和
schema 3 说明见 `benchmark-calibration-sampling-randomization.md`。

## Report Shape

Markdown 报告路径：

- `build/<config>/tests_generated/performance/benchmark_report.md`

JSON 报告路径：

- `build/<config>/tests_generated/performance/benchmark_report.json`

steady 原始报告和生成树使用独立路径：

- `build/<config>/tests_generated/performance_steady/`
- `build/<config>/tests_generated/performance_suite_steady/`

聚合 steady 报告时必须传
`--performance-subdir performance_steady`。默认写入
`benchmark_suite_summary__performance_steady.json`，且不会覆盖 process
默认使用的 `benchmark_html_viewer.json`。

Markdown 长表字段：

- `case`
- `implementation`
- `language`
- `status`
- `mean wall ms`
- `median wall ms`
- `min wall ms`
- `max wall ms`
- `stddev wall ms`
- `mean peak MiB`
- `max peak MiB`
- `relative_to_c`

JSON schema 3 结构：

- `schema_version = 3`
- `suite`
- `generated_at_utc`
- `tier`
- `scale`
- `warmup`
- `iterations`
- `execution_plan`
- `measurement_policy`
- `cases[]`
  - `name`
  - `description`
  - `pass_banner`
  - `expected_checksum`
  - `min_sample_ms`
  - `implementations[]`
    - `name`
    - `language`
    - `mode`
    - `status`
    - `command`
    - `working_directory`
    - `runs`
    - `summary`
    - `sample_count` / `extra_sample_count` / `repetitions`
    - `calibration`
    - `stability` / `comparable` / `gate_eligible`
    - `relative_to_c`
    - `note` when skipped or failed

## `SKIP` Semantics

`SKIP` 只表示“当前宿主没有可用实现路径”，例如：

- 没有 `python`
- 没有 `node`
- 没有 `cargo`
- 没有 `dotnet`

被 `SKIP` 的实现必须继续出现在 Markdown/JSON 报告里，不能直接消失。

## Validation Commands

### Register `performance_report` in CTest

首次需在配置阶段打开开关（缓存变量，会写入 CMake 缓存）：

```bash
cmake -S . -B build/codex-wsl-gcc-debug -DZR_VM_REGISTER_PERFORMANCE_CTEST=ON
cmake --build build/codex-wsl-gcc-debug -j 8
```

开启后，`performance_report` 会带上 CTest 标签 `benchmark` 与 `long_running`，可单独筛选：例如 `ctest -L benchmark` 只跑该类测试。

### Run without CTest（与 `performance_report` 同源脚本）

不改缓存也可直接跑同一套 `tests/cmake/run_performance_suite.cmake`：

```bash
cmake --build build/codex-wsl-gcc-debug --target run_performance_suite
```

环境变量 `ZR_VM_TEST_TIER`、`ZR_VM_PERF_WARMUP`、`ZR_VM_PERF_ITERATIONS` 等对 CTest 与自定义目标均生效（由 `run_performance_suite.cmake` 读取）。

### Windows MSVC

```powershell
. 'C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Import-VsDevCmdEnvironment.ps1'
cmake -S . -B build/codex-msvc-debug -G "Visual Studio 17 2022" -A x64 -DZR_VM_REGISTER_PERFORMANCE_CTEST=ON
cmake --build build/codex-msvc-debug --config Debug
ctest --test-dir build/codex-msvc-debug -C Debug --output-on-failure -R '^performance_report$'
```

### WSL gcc

```bash
cmake -S . -B build/codex-wsl-gcc-debug -DZR_VM_REGISTER_PERFORMANCE_CTEST=ON
cmake --build build/codex-wsl-gcc-debug -j 8
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R '^performance_report$'
```

### Override Sampling Counts

需已用 `-DZR_VM_REGISTER_PERFORMANCE_CTEST=ON` 配置过构建树，否则改用 `run_performance_suite` 目标。

```powershell
. 'C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Import-VsDevCmdEnvironment.ps1'
$env:ZR_VM_PERF_WARMUP='1'
$env:ZR_VM_PERF_ITERATIONS='2'
ctest --test-dir build/codex-msvc-debug -C Debug --output-on-failure -R '^performance_report$'
```

### Focused Persistent Protocol

构建树包含 `benchmark_persistent_protocol` CTest。它验证 READY/DONE/ERROR
严格语法、deadline、早退、checksum/index、同 PID、session RSS，以及
POSIX process group / Windows job object 清理。完整 steady suite 未运行时，
该 focused PASS 不能表述为 full steady PASS。

## Maintenance Rules

1. 新 benchmark 必须先落到 `tests/benchmarks/`，不要再回填旧 `tests/fixtures/projects/benchmark_*`。
2. correctness contract 改动时，先统一更新所有语言实现，再更新 `registry.cmake`。
3. 缺工具链属于 `SKIP`；实现逻辑错误属于 `FAIL`。
4. `relative_to_c` 只在同 case 的 C 基线与目标都是 `PASS`、同 scope、
   `STABLE`、comparable 且 gate eligible 时计算，否则写 `null`。
