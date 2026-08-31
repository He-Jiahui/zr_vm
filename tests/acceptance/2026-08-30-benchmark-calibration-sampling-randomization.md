---
title: Benchmark calibration sampling and randomization acceptance
date: 2026-08-30
status: complete
---

# Benchmark Calibration, Sampling, And Randomization Acceptance

Task 3 integrates the versioned execution planner, adaptive measurement policy,
structured statistics, and schema 3 reporting into the existing performance
suite. Process remains the default scope; steady keeps its isolated output root.

## TDD Evidence

- RED: the focused CMake contract failed before the Task 3 helper existed.
- GREEN: policy, seed boundaries, filtered fixed-seed ordering, runner JSON
  parsing, profile fail-closed behavior, suite markers, and CTest registration
  checks pass in `run_benchmark_task3_suite_contract_test.cmake`.
- RED: the policy contract caught `12 + 10` adaptive samples exceeding the
  runner's 20-sample limit.
- GREEN: policy now resolves `12 -> 8`, `20 -> 0`, and rejects values above 20.
- RED/GREEN: report-consumer tests first accepted unstable/profile records, then
  passed after aggregate and CSV comparison gates required Task 3 fields when
  present. Legacy records without those fields remain compatible.
- GREEN: the 413-line deferred case/report assembly was extracted to
  `tests/cmake/benchmark_task3_case_assembly.cmake`; the main suite is now 1,503
  lines and the exact post-extraction smokes pass.

## Focused Contract

```text
python tests/benchmarks/test_benchmark_task3_report_consumers.py
.....
Ran 5 tests in 0.001s
OK

cmake -DTASK3_MODULE=.../tests/cmake/benchmark_task3_suite.cmake \
  -DEXECUTION_PLAN_SCRIPT=.../scripts/benchmark/benchmark_execution_plan.py \
  -DPERFORMANCE_SUITE_SCRIPT=.../tests/cmake/run_performance_suite.cmake \
  -DTESTS_CMAKE=.../tests/CMakeLists.txt -DPYTHON_EXE=python \
  -DTEST_OUTPUT_DIR=.../build/task3-contract \
  -P tests/cmake/run_benchmark_task3_suite_contract_test.cmake
-- Benchmark Task 3 suite contract PASS
```

The generated MSVC CTest tree also passes all four registrations when the Task 3
contract is run alone. In a combined four-test CTest invocation, the nested
CMake process was rejected by CTest's Windows job-object assignment with
`AssignProcessToJobObject` error 87; direct CTest execution of the same test
passed, so this is recorded as a CTest launcher limitation rather than a suite
contract failure.

## Real Suite Smokes

Both runs used the rebuilt adaptive MSVC runner and native benchmark runner.

| Scope | Filter | Sampling | Result | Evidence |
| --- | --- | --- | --- | --- |
| process | `numeric_loops/c` | warmup 0, initial 1, seed 0 | PASS | `process_end_to_end`, calibration enabled (`MIN_SAMPLE_MS=750`, repetitions 2), schema 3, structured MAD/CV/bootstrap present; one sample is not gate eligible |
| steady | `numeric_loops/dotnet` | warmup 0, initial 10, seed 0 | PASS | `persistent_runtime`, one session PID, calibration enabled, `jit_state_reused=true`, 20 samples (`10 + 10`), `UNSTABLE`, comparable true, gate false, ratios null |
| profile | `numeric_loops/c` | conflicting overrides, seed 0 | PASS | forced warmup 0, iterations 1, repetitions 1, calibration disabled, `NOT_COMPARABLE`, gate false, ratio null; the runner invocation included `--profile` |

The steady run wrote only under `performance_steady`; the process run wrote only
under `performance`. The embedded execution plan records
`fisher_yates_splitmix64`, seed `0`, filters, and exact job order. Runner
statistics were read from the JSON report; stdout was retained only for
diagnostics.

## Acceptance Decision

Accepted for the Task 3 suite integration slice. The focused contract and both
real filtered smokes pass. The full cross-language matrix remains an operational
benchmark run, not a prerequisite for this orchestration/statistics acceptance.
