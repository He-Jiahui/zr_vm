---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/core/gdb_clang_execution_dispatch_assert.gdb
implementation_files:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
plan_sources:
  - user: 2026-04-09 接入 Java 并补齐 benchmark case
  - user: 2026-04-09 生成完整测试报告在 docs/benchmarks 里面
  - user: 2026-04-09 WSL clang performance_report 仍是现有 ZR interp 在 execution_dispatch.c (line 1752) 的断言失败 先修复，然后再release构建
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/core/gdb_clang_execution_dispatch_assert.gdb
  - tests/acceptance/2026-04-09-wsl-clang-performance-report-rebuild.md
doc_type: category-index
---

# Benchmarks

本目录收敛 `zr_vm` 当前 benchmark 体系的正式说明、语言对比结果和 WSL Release 验证结论。

## 当前文档

- `wsl-profile-benchmark-validation-report.md`
  - `2026-04-09` 的最新 WSL Release `profile` 报告
  - 覆盖 clang Release 主链路和 gcc Release 交叉复核
  - 明确记录：
    - `execution_dispatch.c:1752` clang 断言的真实根因与恢复方式
    - `Release + -O3 -DNDEBUG` 构建口径
    - 14-case 语言定位和热点摘要
- `vm-performance-instruction-optimization-w1.md`
  - 记录二阶段 VM 性能计划中 W1 runtime-body 优化的 live continuation
  - 覆盖 readonly-inline stack-operands index 快路、dispatch-local helper 计数、
    receiver 类型门槛去重，以及相邻 rejected probe
- `vm-performance-gc-stabilization.md`
  - 记录 `2026-05-02` GC stress stabilization 的 rejected no-pin `SetValue` probes
    和 accepted `ZrLib_Array_PushValue` dense pair-pool append probe
  - 追加记录 `2026-05-03` public plain append no-pin probe：correctness 过但因
    WSL/Callgrind 证据不可完成且对 GC stress 主要是 miss lane，已撤回
  - 追加记录 `container.Array.add` dense pair-pool append：`gc_fragment_stress`
    manual Callgrind 从 `892,164,677 Ir` 降到 `873,623,765 Ir`
  - 保留 fresh baseline、rejected probes、accepted Callgrind、focused correctness、
    full wall gate 未闭合原因和剩余缺口

## 阅读顺序

1. 先看 `wsl-profile-benchmark-validation-report.md`，获取当前 Release benchmark 事实、语言相对位置和热点结论。
2. 需要追二阶段 W1 runtime-body 优化证据时，看 `vm-performance-instruction-optimization-w1.md`。
3. 需要追 GC stress stabilization 的 A/B、rejected probes、accepted dense append probes、2026-05-03 撤回记录、full wall gate 状态和剩余缺口时，看 `vm-performance-gc-stabilization.md`。
4. 需要了解 suite 如何生成报表时，再看 `../testing-and-validation/ctest-performance-reporting.md`。
5. 需要追查 clang 断言恢复证据时，再看：
   - `../../tests/acceptance/2026-04-09-wsl-clang-performance-report-rebuild.md`
   - `../../tests/core/gdb_clang_execution_dispatch_assert.gdb`
6. 需要追 Java 接入和语言对比切片时，再看：
   - `../../tests/acceptance/2026-04-09-java-benchmark-integration.md`
   - `../../tests/acceptance/2026-04-09-language-benchmark-positioning.md`

## 相关生成产物

当前正式 benchmark 产物来自以下 Release 目录：

- `build/codex-wsl-current-clang-release-make/tests_generated/performance/`
- `build/codex-wsl-current-gcc-release-make/tests_generated/performance/`

每个目录下固定包含：

- `benchmark_report.md/json`
- `comparison_report.md/json`
- `instruction_report.md/json`
- `hotspot_report.md/json`

本目录负责把这些构建产物整理成可追踪、可审阅、可复用的长期文档。
