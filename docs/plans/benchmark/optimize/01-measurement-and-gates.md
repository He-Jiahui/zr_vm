---
related_code:
  - tests/performance/perf_runner.c
  - tests/cmake/run_performance_suite.cmake
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/dotnet_runner/Program.cs
  - tests/benchmarks/common/lua/benchmark_runner.lua
  - tests/benchmarks/common/qjs/benchmark_runner.js
  - scripts/benchmark/aggregate_benchmark_summary.py
implementation_files:
  - tests/performance/perf_runner.c
  - tests/cmake/run_performance_suite.cmake
  - tests/benchmarks/registry.cmake
plan_sources:
  - user: 2026-08-29 VM performance audit and optimization
  - docs/plans/benchmark/optimize/00-audit-and-baseline.md
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md
doc_type: testing-guide
---

# 测量协议与性能门禁计划

> **Goal:** 让 cold-start、源码编译、字节码加载和 steady-state 执行分别可测，并产生足以判断 3% 回归的统计证据。
>
> **Architecture:** 保留现有一次性进程 runner 作为 cold-start 通道；新增基于 stdin/stdout request-response 的持久子进程协议，由父 runner 计时同一已加载运行时内的每次执行。
>
> **Tech Stack:** C11 `clock_gettime`/`QueryPerformanceCounter`、CMake、JSON、ZR embed runner、Lua、QuickJS、.NET `Stopwatch`、Python 报告脚本。

## 缺陷

现有 warmup 与 measured iteration 均新建子进程，导致 .NET JIT、ZR 解析器和运行时状态不能跨样本保留。core 默认 1 次测量，固定实现顺序且没有环境指纹或置信区间。报告同时列出语言比值，却没有阻止 cold-start 数字被解释为 steady-state 吞吐。

## Task 1：显式区分测量 scope

**Files:**

- Modify: `tests/performance/perf_runner.c`
- Modify: `tests/cmake/run_performance_suite.cmake`
- Modify: `tests/benchmarks/README.md`
- Add: `tests/benchmarks/test_benchmark_support.c`
- Modify: `tests/CMakeLists.txt`

**Steps:**

1. 在测试中要求每条 JSON 记录包含 `measurement_scope`、`prepare_scope`、`runtime_reused`、`compiler_reused`、`jit_state_reused`。
2. 运行 `cmake --build build/codex-wsl-gcc-debug --target zr_vm_benchmark_support_test -j2`，确认字段缺失导致失败。
3. 为当前 runner 写入 `measurement_scope=process_end_to_end`，并明确 ZR interp/binary 的 prepare 边界。
4. 更新 Markdown/CSV/aggregate JSON，禁止 scope 为空时生成跨运行时比值。
5. 运行 `ctest --test-dir build/codex-wsl-gcc-debug -R 'benchmark_(registry|support)' --output-on-failure`。

## Task 2：持久子进程稳态协议

**Files:**

- Modify: `tests/performance/perf_runner.c`
- Add: `tests/benchmarks/zr_runner/benchmark_server.c`
- Modify: `tests/benchmarks/dotnet_runner/Program.cs`
- Modify: `tests/benchmarks/common/lua/benchmark_runner.lua`
- Modify: `tests/benchmarks/common/qjs/benchmark_runner.js`
- Modify: `tests/CMakeLists.txt`
- Add: `tests/performance/test_perf_runner_persistent_protocol.c`

**Protocol:**

- 父进程启动一次 `--benchmark-server --case <case> --tier <tier>`。
- 子进程完成运行时、模块和 case 加载后输出 `READY <checksum-contract>`。
- 父进程发送 `WARMUP <index> <repetitions>` 或 `RUN <index> <repetitions>`，从写请求前到读取响应后计时；子进程在一次请求内执行指定次数。
- 子进程执行一次完整 case 并输出 `DONE <index> <checksum>`；错误输出 `ERROR <index> <code>` 后退出非零。
- 父进程完成后发送 `STOP`，记录同一 PID、每次 wall time 和 peak RSS。

**Steps:**

1. 用一个小型 echo/compute fixture 写协议测试，覆盖 READY 超时、checksum 不匹配、子进程提前退出和 STOP。
2. 运行该测试并确认当前 `perf_runner` 不认识 `--persistent` 而失败。
3. 在 POSIX 侧用两组 pipe 和 `poll`，Windows 侧用继承 handle 与等待对象实现协议；不把 stdout banner 解析逻辑复制到各语言。
4. ZR server 在同一进程中加载一次 `.zro` 并重复调用入口；Lua、QuickJS 和 .NET server 在同一 runtime 中重复调用 case 函数。
5. 将 `runtime_reused=true`、`.NET jit_state_reused=true` 写入 steady-state JSON。
6. 分别运行 numeric/dispatch 的 process 和 persistent 模式，验证 checksum 相同且 PID 复用。

## Task 3：校准、样本与随机化

**Files:**

- Modify: `tests/cmake/run_performance_suite.cmake`
- Modify: `tests/benchmarks/registry.cmake`
- Modify: `scripts/benchmark/aggregate_benchmark_summary.py`
- Test: `tests/benchmarks/test_benchmark_registry.c`

**Steps:**

1. 为 case 增加 `MIN_SAMPLE_MS`，默认 750 ms；runner 在测量前倍增内部 repetition，直到单样本达到阈值。
2. steady-state 默认 warmup 5、measured 10；profile 固定 1 次并明确不可用于 wall-time 比较。
3. 用固定 seed Fisher-Yates 随机化 case/implementation 顺序，并把 seed 写入报告。
4. 报告 median、MAD、coefficient of variation、bootstrap 95% confidence interval；保留 mean/stddev 仅用于兼容。
5. 当 CV 大于 5% 时自动追加至多 10 个样本；仍大于 5% 时标记 `UNSTABLE`，不参与门禁。
6. 用合成固定耗时和抖动 fixture 验证校准、追加样本与 `UNSTABLE` 分支。

## Task 4：环境指纹与隔离

**Files:**

- Modify: `tests/cmake/run_performance_suite.cmake`
- Modify: `scripts/benchmark/aggregate_benchmark_summary.py`
- Add: `scripts/benchmark/capture_benchmark_environment.sh`
- Add: `tests/benchmarks/test_benchmark_environment_contract.py`

**Steps:**

1. 采集 CPU model、逻辑核数、kernel/WSL 版本、编译器和运行时版本、构建 flags、git commit/dirty、CPU governor、taskset mask、后台 load average。
2. 默认使用一个物理核；无法固定亲和性时将报告标记为 `NON_ISOLATED`。
3. 禁止比较 CPU model、build flags、scope 或 case checksum contract 不同的两份报告。
4. WSL 构建目录放到 `${HOME}/.cache/zr-vm-benchmark/<commit>/<toolchain>`；只把报告复制回工作区，避免 `/mnt/e` CMake glob/I/O 污染迭代时间。
5. 运行环境合同测试，确认缺字段、不同 flags 和不同 scope 都拒绝生成 speedup。

## Task 5：算法与表示等价性

**Files:**

- Modify: `tests/benchmarks/registry.cmake`
- Modify: `tests/benchmarks/test_benchmark_registry.c`
- Modify: `tests/benchmarks/native_runner/benchmark_support.c`
- Modify: `tests/benchmarks/common/lua/benchmark_runner.lua`
- Modify: `tests/benchmarks/common/qjs/benchmark_runner.js`
- Modify: `tests/benchmarks/common/node/benchmark_runner.js`
- Modify: `tests/benchmarks/common/python/benchmark_runner.py`
- Modify: `tests/benchmarks/dotnet_runner/BenchmarkSupport.cs`
- Add: `docs/benchmarks/cross-runtime-equivalence.md`

**Steps:**

1. 为每个 case 登记 `ALGORITHM_VERSION`、`REPRESENTATION_CLASS`、输入规模公式和允许的内建操作。
2. 审核排序、map/object、string 和 array case，确保没有一方使用预计算、不同复杂度算法或不等价容器。
3. 为所有语言输出 checksum 之外的 deterministic operation counters，在 smoke 档逐项比对。
4. representation 不等价的 case 保留为生态体验数据，但从 VM 核心吞吐几何平均中排除。

## 门禁规则

- PR micro gate：同机同 scope，10 个稳定样本，回归大于 3% 且 95% CI 不跨 0 时失败。
- nightly macro gate：全 core/stress，记录几何平均、每类 worst case 和 RSS；回归大于 5% 失败。
- 优化接受：目标 case 提升至少 3%，代表集几何平均不回退超过 1%，正确性与内存门禁全绿。
- cold-start、steady-state、profile/Callgrind 和 AOT compile time 分成四份报告，禁止聚合为一个“总性能”数字。
