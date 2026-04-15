# Java Benchmark Integration

## Status
- Accepted for the Java benchmark slice on the WSL/gcc main validation chain.
- Java is now a first-class benchmark language in the report schema and no longer shows up as a blanket `SKIP` when the toolchain is provided.

## Root Causes Found
- The runtime probe in `tests/cmake/run_performance_suite.cmake` used `java --version`.
  - That fails on the current JDK 8 runtime even though `java -version` and `javac -version` succeed.
- The WSL `ctest -> cmake -P -> execute_process()` path does not apply the same automatic argument path translation that interactive WSL bash applies when launching Windows `.exe` tools.
  - As a result, Windows `javac.exe` received Linux source paths like `/mnt/e/...` and failed before any Java case could compile.

## Implemented Fix
- Kept the previously added Java registry/runner/case wiring.
- Completed the WSL execution chain by fixing `tests/cmake/run_performance_suite.cmake`:
  - Java runtime probe now uses `java -version`.
  - Added executable-aware path translation via `wslpath -w` for:
    - Java compile output directory
    - Java source file list
    - Java runtime classpath
- This makes the existing env-var contract work end-to-end:
  - `ZR_VM_JAVA_EXE`
  - `ZR_VM_JAVAC_EXE`

## Validation
- WSL gcc profile report:
  - command:
    - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-gcc-debug-make -R '^performance_report$' --output-on-failure"`
  - result:
    - passed
  - artifact timestamp:
    - `2026-04-09T12:01:42Z`
- WSL gcc Java checksum smoke for the 6 newly added cases:
  - `fib_recursive`: `BENCH_FIB_RECURSIVE_PASS / 79101464`
  - `call_chain_polymorphic`: `BENCH_CALL_CHAIN_POLYMORPHIC_PASS / 47250207`
  - `object_field_hot`: `BENCH_OBJECT_FIELD_HOT_PASS / 623146080`
  - `array_index_dense`: `BENCH_ARRAY_INDEX_DENSE_PASS / 175707665`
  - `branch_jump_dense`: `BENCH_BRANCH_JUMP_DENSE_PASS / 237632615`
  - `mixed_service_loop`: `BENCH_MIXED_SERVICE_LOOP_PASS / 408940136`
- WSL clang:
  - `benchmark_registry`: passed
  - `performance_report`: still hard-fails because current clang `ZR interp` correctness aborts in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:1752`
  - Java-specific outcome inside the clang report:
    - all 14 wired Java cases are `PASS`
    - this shows the Java harness path itself is working on clang too
- Windows MSVC smoke:
  - command:
    - `.\build\codex-msvc-cli-debug-current\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp`
  - result:
    - printed `hello world`

## Java Snapshot
- Source artifacts:
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/benchmark_report.md`
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/comparison_report.md`
- Java coverage in the current profile report:
  - `14 / 14` cases

| case | Java wall ms | `ZR interp vs Java` |
| --- | ---: | ---: |
| `numeric_loops` | 203.024 | 2.510x |
| `dispatch_loops` | 177.500 | 3.129x |
| `container_pipeline` | 170.182 | 1.216x |
| `sort_array` | 149.869 | 0.848x |
| `prime_trial_division` | 158.584 | 1.015x |
| `matrix_add_2d` | 156.748 | 0.872x |
| `string_build` | 155.434 | 0.884x |
| `map_object_access` | 180.600 | 1.108x |
| `fib_recursive` | 149.827 | 1.084x |
| `call_chain_polymorphic` | 158.618 | 0.834x |
| `object_field_hot` | 159.255 | 1.134x |
| `array_index_dense` | 147.548 | 0.945x |
| `branch_jump_dense` | 166.292 | 1.026x |
| `mixed_service_loop` | 159.447 | 1.053x |

## Positioning Read
- Aggregate `ZR interp vs Java` on the full 14-case set:
  - min ratio: `0.834x`
  - max ratio: `3.129x`
  - arithmetic mean: `1.261x`
  - geometric mean: `1.153x`
- `ZR interp` is already faster than Java on:
  - `call_chain_polymorphic`
  - `sort_array`
  - `matrix_add_2d`
  - `string_build`
  - `array_index_dense`
- `ZR interp` is still slower than Java on:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `prime_trial_division`
  - `map_object_access`
  - `fib_recursive`
  - `object_field_hot`
  - `branch_jump_dense`
  - `mixed_service_loop`

## Remaining Gap
- Java integration is now complete for the current 14-case benchmark set.
- The remaining gap is no longer wiring or coverage.
- The remaining gap is performance on the Java-losing workloads:
  - `dispatch_loops`
  - `numeric_loops`
  - `container_pipeline`
  - `map_object_access`
  - `object_field_hot`
  - `mixed_service_loop`

## Acceptance Decision
- Accepted for this slice.
- Reason:
  - the Java runner now compiles and executes through the canonical WSL `performance_report` path
  - the report schema emits real `Java` rows and real `vs Java` ratios for all 14 cases
  - the remaining clang failure is in current `ZR interp` correctness, not in the Java harness path
