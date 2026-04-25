---
title: W2 Load Arithmetic Probe And Array Items Cache Acceptance
date: 2026-04-24
status: accepted
builds:
  - build/codex-wsl-gcc-debug
  - build/benchmark-gcc-release
---

# W2 Load Arithmetic Probe And Array Items Cache Acceptance

## Scope

- Added W2 compiler tests for post-quickening load/arithmetic probe coverage and `Array<int>` cached-items loop quickening.
- Added internal opcodes for loop-local Array items binding and cached get/set.
- Added runtime profile `quickening_probes` counters for dynamic `GET_STACK/GET_CONSTANT -> signed arithmetic` candidates.
- Raw contiguous `Array<int>` storage remains a documented next-stage design item.

## Validation

- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_integration_test zr_vm_compiler_w2_performance_quickening_test -j 8"`
  - PASS
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_w2_performance_quickening_test"`
  - PASS: 2 tests, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test"`
  - PASS: 94 tests, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_cli /tmp/zr_w2_matrix/benchmark_matrix_add_2d.zrp"`
  - PASS: `BENCH_MATRIX_ADD_2D_PASS`

## Profile Probe

Profile artifact: `build/benchmark-gcc-release/tests_generated/performance_profile_w2_items_cache_probe_20260424`

| case | total opcodes | load/arithmetic probes | probe % | GET_STACK probes | GET_CONSTANT probes |
| --- | ---: | ---: | ---: | ---: | ---: |
| numeric_loops | 6,944,154 | 773,141 | 11.134 | 0 | 773,141 |
| branch_jump_dense | 1,580,886 | 211,814 | 13.398 | 4,182 | 207,632 |
| dispatch_loops | 7,341,617 | 461,222 | 6.282 | 115,200 | 346,022 |
| matrix_add_2d | 35,009 | 2 | 0.006 | 0 | 2 |
| array_index_dense | 147,369 | 2 | 0.001 | 0 | 2 |
| sort_array | 16,442 | 1,274 | 7.748 | 124 | 1,150 |

Interpretation: numeric/branch/dispatch/sort exceed the 2% gate for a follow-up load+arithmetic superinstruction family. Matrix and dense-array workloads do not; their next useful stage is array storage/cache work.

## Array Items Cache Counts

| case | bind | get items | get items plain | set items |
| --- | ---: | ---: | ---: | ---: |
| matrix_add_2d | 99 | 2,304 | 1,536 | 3,072 |
| array_index_dense | 49 | 18,240 | 0 | 6,048 |
| sort_array | 133 | 1,286 | 9 | 1,197 |

The new opcodes replace the old typed array get/set opcodes in these hot loops while preserving boxed array item semantics.

## Cross-Language Core Timing

Core artifact: `build/benchmark-gcc-release/tests_generated/performance_core_w2_items_cache_probe_20260424`

| case | ZR interp vs C | ZR binary vs C | ZR interp vs Lua | ZR interp vs QuickJS | ZR interp vs Node | ZR interp vs Python | ZR interp vs Rust |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_loops | 4.389 | 3.481 | 1.964 | 1.373 | 1.063 | 0.568 | 6.072 |
| dispatch_loops | 11.659 | 11.643 | 0.319 | 0.891 | 0.389 | 0.505 | 10.426 |
| sort_array | 4.577 | 3.963 | 2.593 | 2.778 | 0.565 | 0.865 | 3.580 |
| matrix_add_2d | 2.125 | 1.781 | 2.889 | 2.896 | 0.643 | 1.079 | 3.535 |
| array_index_dense | 4.996 | - | - | - | 0.580 | 0.966 | - |
| branch_jump_dense | 5.823 | - | - | - | 0.571 | 1.041 | - |

Single-iteration core timing is noisy; use the profile counts above for pass/fail decisions on opcode-level work.
