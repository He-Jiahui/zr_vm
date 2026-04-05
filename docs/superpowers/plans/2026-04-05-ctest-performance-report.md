# CTest Performance Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated `ctest` performance suite that runs stable benchmark project fixtures and emits timing plus peak-memory reports.

**Architecture:** Keep functional regression suites unchanged and add one isolated `performance_report` suite. Use a small cross-platform benchmark runner executable to measure subprocess wall time and peak memory, then aggregate per-case results into Markdown and JSON from a dedicated CMake suite script.

**Tech Stack:** C, CMake/CTest, zr_vm CLI project fixtures, Windows process APIs, POSIX `fork/exec/wait4`.

---

### Task 1: Define The Benchmark Contract

**Files:**
- Create: `docs/testing-and-validation/ctest-performance-reporting.md`
- Modify: `tests/TEST_EXECUTION_ORDER.md`
- Modify: `tests/test_runner.c`

- [ ] **Step 1: Document the new suite contract**

Write the suite purpose, report paths, default iteration policy, and the stable benchmark case list.

- [ ] **Step 2: Expose the suite in user-facing test docs**

Add `performance_report` to the active suite list and runner help text.

### Task 2: Add Stable Benchmark Fixtures

**Files:**
- Create: `tests/fixtures/projects/benchmark_numeric_loops/benchmark_numeric_loops.zrp`
- Create: `tests/fixtures/projects/benchmark_numeric_loops/src/main.zr`
- Create: `tests/fixtures/projects/benchmark_dispatch_loops/benchmark_dispatch_loops.zrp`
- Create: `tests/fixtures/projects/benchmark_dispatch_loops/src/main.zr`
- Create: `tests/fixtures/projects/benchmark_container_pipeline/benchmark_container_pipeline.zrp`
- Create: `tests/fixtures/projects/benchmark_container_pipeline/src/main.zr`

- [ ] **Step 1: Add deterministic benchmark workloads**

Create three benchmark projects covering arithmetic loops, method dispatch, and container-heavy aggregation.

- [ ] **Step 2: Prove they return stable outputs**

Run each project once through `zr_vm_cli` and record the exact expected output shape for later suite assertions.

### Task 3: Build The Cross-Platform Perf Runner

**Files:**
- Create: `tests/performance/perf_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add a failing contract**

Use `ctest -N -R '^performance_report$'` to show the suite is currently missing.

- [ ] **Step 2: Implement subprocess timing and peak-memory sampling**

Use `CreateProcessA` + `GetProcessMemoryInfo` on Windows and `fork/execvp` + `wait4` on POSIX.

- [ ] **Step 3: Emit machine-readable per-case output**

Have the runner print a parseable summary line and also write a JSON file per benchmark case.

### Task 4: Wire The CTest Suite And Reports

**Files:**
- Create: `tests/cmake/run_performance_suite.cmake`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add the `performance_report` CTest entry**

Register a dedicated suite that runs serially for measurement stability.

- [ ] **Step 2: Aggregate Markdown and JSON reports**

Generate `tests_generated/performance/benchmark_report.md` and `tests_generated/performance/benchmark_report.json`.

- [ ] **Step 3: Support tier-based iteration counts**

Use `smoke/core/stress` to scale warmup and measured iterations without changing the benchmark case contract.

### Task 5: Verify On WSL And MSVC

**Files:**
- Test: `build/codex-wsl-gcc-debug`
- Test: `build/codex-msvc-debug`

- [ ] **Step 1: Run focused performance suite verification**

Run `ctest -R '^performance_report$' --output-on-failure` in WSL gcc and Windows MSVC builds.

- [ ] **Step 2: Re-run full relevant suites**

Run the normal CTest regression after the new suite is integrated.

- [ ] **Step 3: Inspect generated reports**

Confirm every benchmark case includes wall time and peak-memory statistics in both Markdown and JSON outputs.
