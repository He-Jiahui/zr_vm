---
related_code:
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
  - tests/fixtures/projects/benchmark_numeric_loops/benchmark_numeric_loops.zrp
  - tests/fixtures/projects/benchmark_numeric_loops/src/main.zr
  - tests/fixtures/projects/benchmark_dispatch_loops/benchmark_dispatch_loops.zrp
  - tests/fixtures/projects/benchmark_dispatch_loops/src/main.zr
  - tests/fixtures/projects/benchmark_container_pipeline/benchmark_container_pipeline.zrp
  - tests/fixtures/projects/benchmark_container_pipeline/src/main.zr
implementation_files:
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
plan_sources:
  - user: 2026-04-05 ctest 需要生成性能测试报告并包含耗时与内存占用
  - docs/zr_language_test_requirements.md
tests:
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/fixtures/projects/benchmark_numeric_loops/benchmark_numeric_loops.zrp
  - tests/fixtures/projects/benchmark_dispatch_loops/benchmark_dispatch_loops.zrp
  - tests/fixtures/projects/benchmark_container_pipeline/benchmark_container_pipeline.zrp
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
doc_type: testing-guide
---

# CTest Performance Reporting

## Purpose

`performance_report` 是一个单独的 CTest 入口，职责不是功能正确性回归，而是对稳定 benchmark 项目夹具做运行时测量，并产出可归档的性能报告。

它解决的三个问题是：

- 让 `ctest` 可以直接生成性能报告，而不是靠临时手工命令
- 把“耗时”和“峰值内存占用”统一落成固定产物
- 把 benchmark 工作负载固定在仓库内，避免每次都重新挑案例

## Suite Shape

`performance_report` 由三层组成：

1. `tests/fixtures/projects/benchmark_*`
   - 仓库内稳定 benchmark 夹具
   - 当前包含：
     - `benchmark_numeric_loops`
     - `benchmark_dispatch_loops`
     - `benchmark_container_pipeline`
2. `tests/performance/perf_runner.c`
   - 跨平台子进程测量器
   - Windows 走 `CreateProcessA` + `GetProcessMemoryInfo`
   - POSIX 走 `fork/execvp` + `wait4`
3. `tests/cmake/run_performance_suite.cmake`
   - 复制 fixture
   - 预编译到 `bin/*.zro`
   - 用 `--execution-mode binary` 做 correctness gate
   - 调用 `zr_vm_perf_runner` 重复测量
   - 聚合 Markdown/JSON 报告

## Why Binary Mode

性能采样固定在 `binary` 模式，而不是 source run。

原因是 source run 会把前端编译和项目加载噪声混进单次测量，导致“算法 workload” 与 “即时编译/文件系统” 两种成本缠在一起。当前 suite 的流程是：

1. 先执行 `zr_vm_cli --compile <project.zrp>`
2. 再执行一次 `zr_vm_cli <project.zrp> --execution-mode binary`
   - 用精确输出合同确认结果稳定
3. 最后重复执行 binary run 做正式采样

这样报告中的时间和内存主要反映运行态，而不是编译态。

## Tier Policy

`performance_report` 跟随现有 `smoke/core/stress` 过滤，但它的 tier 含义是“迭代次数和 case 集”而不是功能覆盖深度。

- `smoke`
  - 默认 `warmup=1`
  - 默认 `iterations=2`
  - 只跑 `benchmark_numeric_loops`
- `core`
  - 默认 `warmup=2`
  - 默认 `iterations=4`
  - 跑全部 3 个 benchmark case
- `stress`
  - 默认 `warmup=2`
  - 默认 `iterations=8`
  - 跑全部 3 个 benchmark case

可选覆盖环境变量：

- `ZR_VM_PERF_WARMUP=<n>`
- `ZR_VM_PERF_ITERATIONS=<n>`

这两个环境变量只影响 `performance_report`，不会改动其它功能 suite。

## Report Artifacts

默认产物位置：

- `build/<config>/tests_generated/performance/benchmark_report.md`
- `build/<config>/tests_generated/performance/benchmark_report.json`

Markdown 报告面向人读，包含：

- 生成时间
- tier
- warmup / measured iterations
- case 列表
- 每例 `mean / median / min / max / stddev` 耗时
- 每例平均和最大峰值内存

JSON 报告面向机器消费，包含：

- suite 元数据
- 每个 case 的命令、工作目录、逐次 run 样本
- 汇总统计字段

## Benchmark Cases

### `benchmark_numeric_loops`

- 类型：整数算术与分支热点
- 目标：给解释执行器一个低分配、纯控制流的稳定 workload
- 期望输出：
  - `BENCH_NUMERIC_LOOPS_PASS`
  - `939148`

### `benchmark_dispatch_loops`

- 类型：对象方法调用与状态更新
- 目标：覆盖方法分发与对象字段更新路径
- 期望输出：
  - `BENCH_DISPATCH_LOOPS_PASS`
  - `912035`

### `benchmark_container_pipeline`

- 类型：`LinkedList` / `Set` / `Map` 聚合
- 目标：覆盖容器分配、索引与聚合路径
- 期望输出：
  - `BENCH_CONTAINER_PIPELINE_PASS`
  - `1774511`

## Validation Commands

### Windows MSVC

```powershell
. 'C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Import-VsDevCmdEnvironment.ps1'
ctest --test-dir build/codex-msvc-debug -C Debug --output-on-failure -R '^performance_report$'
```

### WSL gcc

```bash
ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure -R '^performance_report$'
```

### Override Iteration Counts

```powershell
. 'C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Import-VsDevCmdEnvironment.ps1'
$env:ZR_VM_PERF_WARMUP='2'
$env:ZR_VM_PERF_ITERATIONS='6'
ctest --test-dir build/codex-msvc-debug -C Debug --output-on-failure -R '^performance_report$'
```

## Maintenance Rules

1. benchmark fixture 必须先通过 correctness gate，才能进入采样阶段。
2. 新增 benchmark case 时优先复用现有已稳定的语言特性组合，不要把性能 suite 变成新语法探索场。
3. 需要更多运行态维度时，优先扩展 `perf_runner.c` 与 JSON schema，而不是把 shell/PowerShell 临时逻辑塞进报告脚本。
4. `performance_report` 保持独立 suite，不把性能采样逻辑重新塞回 `projects` 或 `cli_integration`。
