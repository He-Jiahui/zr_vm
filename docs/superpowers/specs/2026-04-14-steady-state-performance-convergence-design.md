---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/CMakeLists.txt
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
  - scripts/benchmark/time_numeric_loops_interp_vs_aot.sh
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
implementation_files:
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - user: 请参照其他提供的示例语言继续压测，提供性能数据，优化性能
  - user: 全部
  - user: 先执行热路径
  - user: 暂时不考虑aot 以方案CBD顺序执行
tests:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
doc_type: module-detail
---

# Steady-State Performance Convergence Design

## Scope

This round optimizes `zr_vm` steady-state execution only.

In scope:

- `zr_interp`
- `zr_binary`
- cross-language comparison against the existing benchmark suite implementations:
  `C`, `Lua`, `QuickJS`, `Node.js`, `Python`, `.NET`, `Java`, and `Rust` where each case already registers them
- WSL/Linux benchmark evidence as the primary decision source
- shared runtime hot paths first, then mode-specific work only if a shared fix does not explain the gap

Out of scope for this round:

- `zr_aot_c`
- `zr_aot_llvm`
- CLI startup time
- one-shot compile time
- host compiler / dynamic-loader overhead
- Windows-first performance work

The core rule is that the optimization target is the hot execution path after the benchmark is already prepared and running. Reported numbers may still come from the existing suite harness, but decisions must be grounded in steady-state evidence, not startup noise.

## Execution Order

The user selected the `C -> B -> D` order from the earlier proposal. This design therefore executes in the following order:

1. Fix the tracked benchmark case set first.
2. Use the hot-path diagnosis and optimization loop second.
3. Accept or reject changes only through same-metric before/after evidence last.

That order is intentional. It prevents the work from drifting into broad, unfocused benchmarking or premature implementation before the benchmark surface and acceptance rules are stable.

## C. Fixed Benchmark Case Set

The initial tracked case set is:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`
- `gc_fragment_baseline`
- `gc_fragment_stress`

These cases are the current minimum surface because together they cover the dominant runtime families we care about:

- arithmetic and branch density
- dispatch and call overhead
- container/object/index traffic
- string construction
- recursive and layered call chains
- mixed service-style loops
- GC churn under both baseline and stress pressure

Cases outside this set are not banned, but they do not become optimization drivers unless one of the tracked cases points directly at the same lower shared hotspot.

Implementation filter for this round:

- always include `c`, `zr_interp`, and `zr_binary`
- include the existing example-language implementations for each case when the registry already provides them
- explicitly exclude `zr_aot_c` and `zr_aot_llvm`

## B. Diagnosis And Optimization Flow

### Step 1: Build and Fresh Baseline

Use WSL `gcc` Release as the primary diagnosis environment.

Required baseline outputs:

- `benchmark_report.md/json`
- `comparison_report.md/json`
- `instruction_report.md/json`
- `hotspot_report.md/json`
- CSV exports derived from the timing pass

The benchmark build must use a tree where `ZR_VM_REGISTER_PERFORMANCE_CTEST=ON`. The current `build/benchmark-gcc-release` cache already has that enabled, but the workflow must verify it instead of assuming that every benchmark tree does.

The baseline run must use the tracked case set and exclude AOT implementations.

### Step 2: Hot-Path Profiling

After the baseline timing pass, run a profile pass for the same tracked case set.

Primary evidence sources:

- `instruction_report` for opcode and helper distribution
- `hotspot_report` with Callgrind counting mode enabled
- per-case generated artifacts under `tests_generated/performance/` or `tests_generated/performance_profile_callgrind/`

The profiling pass exists to answer one question before any code change:

`Which lowest shared runtime layer explains the largest steady-state gap across zr_interp and zr_binary?`

Likely candidate layers include:

- dispatch loop and opcode dispatch helpers
- VM call preparation and frame setup
- stack/value copy helpers
- object/member/index access helpers
- container runtime helpers
- string runtime helpers
- GC-triggered runtime slow paths
- compiler quickening or lowering choices that still feed the same runtime hot helper

### Step 3: Support-First Optimization

Optimizations must follow the shared-layer rule:

- if `zr_interp` and `zr_binary` are both slow for the same reason, fix the shared runtime/helper/lowering layer first
- only optimize `zr_binary` specifically if the profile still shows a distinct binary-only hotspot after the shared fix
- do not special-case one benchmark project just to green a chart

Each optimization loop must be:

1. identify one hotspot hypothesis
2. change the smallest shared layer that explains it
3. rerun the focused benchmark subset that proves or disproves the hypothesis
4. rerun the broader tracked timing pass before accepting the change

The work stops being acceptable if a local speedup creates a broader regression in the tracked case set.

## D. Acceptance And Reporting

Acceptance is evidence-only and same-metric:

- one fresh pre-optimization baseline
- one fresh post-optimization rerun
- same tracked cases
- same implementation filter
- same tier and warmup/iteration settings

Required acceptance claims:

- which cases improved for `zr_interp`
- which cases improved for `zr_binary`
- whether the fix was shared-runtime or mode-specific
- whether any untargeted tracked case regressed
- updated relative-to-`C` and relative-to-peer-language ratios for the affected cases

Required safety gates:

- no correctness-contract drift in benchmark pass banners or checksums
- no new regressions in the default WSL validation matrix for touched shared code
- if shared runtime/parser/compiler code changes, rerun relevant WSL gcc/clang validation after the performance pass

Performance acceptance threshold for this round:

- at least one real, repeated steady-state improvement must be demonstrated in the targeted hotspot family
- no unexplained regression larger than 5% median wall time on other tracked non-GC core cases for the touched execution modes
- GC cases may move independently, but any regression there must be called out explicitly with before/after data

## Commands To Standardize

The implementation plan should standardize around these command families instead of inventing new benchmark worlds:

- Release benchmark build under WSL:
  `bash scripts/benchmark/build_benchmark_release.sh gcc`
- Fresh timing + profile reports:
  `bash scripts/benchmark/run_wsl_benchmarks_report_csv.sh build/benchmark-gcc-release`
- Focused diagnosis using existing filters:
  `ZR_VM_PERF_ONLY_CASES=...`
  `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=...`
  `ZR_VM_TEST_TIER=core|profile`
  `ctest -R '^performance_report$' --test-dir build/benchmark-gcc-release --output-on-failure`

If the benchmark tree does not register `performance_report`, use the already-supported fallback path through `run_performance_suite` or direct `run_performance_suite.cmake` invocation instead of inventing a parallel harness.

## Risks And Boundaries

- `gc_fragment_*` can dominate absolute ratios and must not be allowed to hide arithmetic/dispatch/container hot paths.
- `zr_binary` can inherit the same shared runtime costs as `zr_interp`, so apparent mode differences may still be rooted in common helpers.
- older benchmark artifacts already present under `build/benchmark-gcc-release/tests_generated/performance/` are historical context only; this round must produce fresh evidence before any claim.
- AOT remains excluded even if existing scripts mention it.

## Deliverables

This round should end with:

- fresh benchmark reports for the chosen tracked case set
- a concise hotspot summary grounded in `comparison_report`, `instruction_report`, and `hotspot_report`
- one or more shared-layer optimizations backed by repeated focused reruns
- a final before/after table for `zr_interp` and `zr_binary`
- WSL regression validation for any touched shared code paths
