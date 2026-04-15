# GC V0/V1 Closure Status And Stress Overhead

## Status
- Accepted for this slice.
- Scope:
  - keep the staged GC v0/v1 region / remembered-set / pin / stats surface intact
  - close a larger compile-time escape-analysis loop with runtime and binary-roundtrip callable-capture validation
  - produce a valid high-pressure GC overhead report for the paired `gc_fragment_baseline` / `gc_fragment_stress` cases
  - document what is still missing so the current GC is not mistaken for a fully complete concurrent production collector

## What Landed In This Slice
- Added runtime escape-pipeline regressions for callable-child captures:
  - returned closure capture closes over a heap object and the closed capture object keeps `CLOSURE_CAPTURE | RETURN`
  - global-bound closure capture closes over a heap object and the closed capture object keeps `CLOSURE_CAPTURE | GLOBAL_ROOT`
  - both cases now also pass after compile -> `.zro` -> runtime load roundtrip
- Added a reproducible GC-only stress benchmark entrypoint:
  - `scripts/benchmark/run_gc_overhead_stress.sh`
  - it runs the same `run_performance_suite.cmake` flow directly so stress-tier GC-only reporting is not truncated by the `performance_report` CTest timeout
- Updated benchmark docs to point stress-tier GC measurements at the direct helper instead of the default `ctest` wrapper when the suite would exceed the 1800-second test timeout

## Verified High-Pressure GC Overhead
- Command:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && export ZR_VM_TEST_TIER=stress && export ZR_VM_PERF_ONLY_CASES=gc_fragment_baseline,gc_fragment_stress && export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,zr_aot_c,zr_aot_llvm && cmake -DCLI_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_cli -DPERF_RUNNER_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_perf_runner -DNATIVE_BENCHMARK_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_native_benchmark_runner -DBENCHMARKS_DIR=/mnt/e/Git/zr_vm/tests/benchmarks -DGENERATED_DIR=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated -DHOST_BINARY_DIR=/mnt/e/Git/zr_vm/build/benchmark-gcc-release -DTIER=stress -P /mnt/e/Git/zr_vm/tests/cmake/run_performance_suite.cmake'`
- Report:
  - `build/benchmark-gcc-release/tests_generated/performance/gc_overhead_report.md`
- Stress-tier paired overhead (`2026-04-12T14:47:37Z`):
  - `C`: baseline `103.305 ms`, stress `112.425 ms`, ratio `1.088`, overhead `8.828%`
  - `ZR interp`: baseline `9130.751 ms`, stress `180834.434 ms`, ratio `19.805`, overhead `1880.499%`
  - `ZR binary`: baseline `12906.141 ms`, stress `173311.937 ms`, ratio `13.429`, overhead `1242.864%`
  - `ZR aot_c`: baseline `20414.624 ms`, stress `166079.294 ms`, ratio `8.135`, overhead `713.531%`
  - `ZR aot_llvm`: baseline `29404.928 ms`, stress `187638.282 ms`, ratio `6.381`, overhead `538.118%`
- Peak-memory deltas on the ZR paths stay clustered around `+145 MiB`, which indicates the paired fixtures are stressing the same survivor/fragmentation shape while the extra wall time is dominated by forced collection work rather than a different workload.

## Validation
- WSL gcc debug:
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_escape_pipeline_test`
  - result: `10 Tests 0 Failures 0 Ignored`
- WSL gcc debug:
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test`
  - result: `49 Tests 0 Failures 0 Ignored`
- WSL gcc benchmark release:
  - direct `run_performance_suite.cmake` invocation above
  - result: valid `benchmark_report`, `gc_overhead_report`, `instruction_report`, and `hotspot_report` under `build/benchmark-gcc-release/tests_generated/performance/`

## What Is Still Missing
- This is still not a fully complete concurrent generational collector:
  - `workerCount` and concurrent-major control-plane fields exist, but the collector is not yet a true background worker-driven `mark/refine/merge/sweep` pipeline with verified concurrent invariants
  - compact/selective-major policy is still a staged v0/v1 implementation, not the full region-fragmentation policy matrix described in the plan
  - the current public API / stats surface is substantially ahead of the concurrency backend
- PIC corruption root cause is still unresolved:
  - `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c` now sanitizes invalid callsite PIC state before mark/rewrite
  - this prevents full-GC scanning from treating garbage `picSlotCount` / `picSlots` contents as live heap pointers
  - the write source that corrupts PIC state is still a TODO and needs a dedicated debug / memory-checker session
- Separate unrelated red baseline remains outside this slice:
  - `zr_vm_container_runtime_test` currently fails early in this worktree on `ZrCore_Function_CheckStackAndAnchor` (`stackPointer != NULL`)
  - that issue was intentionally not folded into this GC closure pass

## Acceptance Decision
- Accepted for this slice because:
  - high-pressure GC overhead is now measured and reproducible from the repo
  - the compile-time escape metadata loop now has runtime/binary closure-capture evidence instead of metadata-only assertions
  - the current GC status is documented honestly as a staged v0/v1 system, not overstated as finished
