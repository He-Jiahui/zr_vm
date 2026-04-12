---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/TEST_EXECUTION_ORDER.md
implementation_files:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
plan_sources:
  - user: 2026-04-07 benchmark 集合迁入 tests/benchmarks 并比较 ZR/C/其他语言
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
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
  负责 case 名称、tier、支持的实现、pass banner、per-tier checksum、是否要求真实 AOT 路径
- `cases/<case>/zr/`
  负责 ZR 项目版本
- `cases/<case>/{c,python,node,rust,dotnet}/`
  负责跨语言实现入口

## Implementation Matrix

主集合实现为：

- `ZR interp`
- `ZR binary`
- `ZR aot_c`
- `ZR aot_llvm`
  - LLVM host adapter 不可用时记为 `SKIP`
- `C`
- `Python`
- `Node.js`
- `Rust`
- `C#/.NET`

当前 suite 默认先处理 `C`，因为 `relative_to_c` 以同 case 的 C 基线为分母。

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
- `ZR aot_c`
  - 先 `zr_vm_cli --compile <project.zrp> --emit-aot-c`
  - 再 `--execution-mode aot_c --require-aot-path`
- `ZR aot_llvm`
  - 先 `zr_vm_cli --compile <project.zrp> --emit-aot-llvm`
  - 再 `--execution-mode aot_llvm --require-aot-path`
  - 缺少 LLVM host adapter 时记为 `SKIP`
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

## Report Shape

Markdown 报告路径：

- `build/<config>/tests_generated/performance/benchmark_report.md`

JSON 报告路径：

- `build/<config>/tests_generated/performance/benchmark_report.json`

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

JSON 结构：

- `suite`
- `generated_at_utc`
- `tier`
- `scale`
- `warmup`
- `iterations`
- `cases[]`
  - `name`
  - `description`
  - `pass_banner`
  - `expected_checksum`
  - `implementations[]`
    - `name`
    - `language`
    - `mode`
    - `status`
    - `command`
    - `working_directory`
    - `runs`
    - `summary`
    - `relative_to_c`
    - `note` when skipped or failed

## `SKIP` Semantics

`SKIP` 只表示“当前宿主没有可用实现路径”，例如：

- 没有 `python`
- 没有 `node`
- 没有 `cargo`
- 没有 `dotnet`
- 没有 `clang` / `clang-cl`，因此 `aot_llvm` 不可用

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

## Maintenance Rules

1. 新 benchmark 必须先落到 `tests/benchmarks/`，不要再回填旧 `tests/fixtures/projects/benchmark_*`。
2. correctness contract 改动时，先统一更新所有语言实现，再更新 `registry.cmake`。
3. 缺工具链属于 `SKIP`；实现逻辑错误属于 `FAIL`。
4. `relative_to_c` 只在同 case 的 C 基线为 `PASS` 时计算，否则写 `null`。
