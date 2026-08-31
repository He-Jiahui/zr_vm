---
related_code:
  - tests/CMakeLists.txt
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/performance/perf_report.c
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
implementation_files:
  - tests/CMakeLists.txt
  - tests/benchmarks/registry.cmake
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/performance/perf_report.c
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
plan_sources:
  - E:/Git/ZirconEngine/docs/plans/optimize/benchmarks
tests:
  - tests/benchmarks/test_benchmark_statistics.py
  - tests/benchmarks/test_benchmark_execution_plan.py
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
  - tests/cmake/run_benchmark_task3_suite_contract_test.cmake
  - tests/cmake/run_benchmark_support_test.cmake
doc_type: testing-guide
---

# Benchmark Calibration, Sampling, And Randomization

## Scope

Task 3 makes benchmark ordering and statistical eligibility explicit. The suite
still performs the one-shot correctness check before measurement and preserves
the process and steady output roots. It now treats execution planning, adaptive
sampling, and ratio eligibility as reportable contracts rather than incidental
harness behavior.

## Execution Plan

The suite builds the flat ordered set of unique `{case, implementation}` jobs
from the registry, applies `ZR_VM_PERF_ONLY_CASES` and
`ZR_VM_PERF_ONLY_IMPLEMENTATIONS`, and only then invokes the frozen execution
planner. The planner uses versioned Fisher-Yates/SplitMix64 shuffling. The
default seed is `0`; `ZR_VM_PERF_SEED` accepts a canonical unsigned 64-bit
decimal integer.

`execution_plan.json` records the schema, algorithm, algorithm version, seed,
filters, job count, and exact job order. `benchmark_report.json` embeds this
plan verbatim. Measurements execute in that flat order. Case report objects are
assembled only after every planned job has finished, so a randomized order
cannot make a ratio depend on whether the baseline happened to execute first.

## Sampling Policy

Process scope preserves the tier's existing warmup and initial-sample defaults.
Steady scope defaults to five warmups and ten initial samples. Non-profile rows
use each case's positive `MIN_SAMPLE_MS`, at most ten adaptive samples, and a
hard total of twenty measured samples. An initial-count override reduces the
adaptive budget: twelve initial samples allow eight extras, and twenty allow
none. Values above twenty fail closed.

The runner calibrates a repetition count before measured sampling so one sample
meets the case minimum duration. It then adds samples while the coefficient of
variation is above `0.05`, subject to the adaptive and total limits. Statistics
are calculated from the structured runner JSON, never parsed from display
stdout.

Profile scope is deliberately different: zero warmups, one sample, one
repetition, no minimum-duration calibration, no adaptive samples, and the
runner's `--profile` flag. Profile rows are `NOT_COMPARABLE` and cannot
contribute ratios or gates.

## Statistics Contract

Each successful implementation record surfaces:

- requested iterations, actual sample count, and extra sample count;
- calibrated repetitions, minimum sample duration, and aggregate calibration
  duration;
- mean, median, minimum, maximum, standard deviation, MAD, and coefficient of
  variation;
- deterministic median bootstrap interval, seed, statistic, and resample count;
- stability, comparability, and gate eligibility;
- mean/median/min/max peak working set for process runs, or session peak working
  set for persistent runs.

Schema 3 adds the root `execution_plan` and `measurement_policy` objects and a
case-level `min_sample_ms`. `UNSTABLE` and profile rows keep their measurements
for diagnosis, but all ratios remain `null`. Aggregate and CSV consumers also
check Task 3 stability/comparability/gate fields when present, while preserving
schema 2 compatibility for older reports.

## Output Isolation

Process reports remain under `tests_generated/performance/`, with fixtures and
tool artifacts under `tests_generated/performance_suite/`. Steady reports use
`performance_steady/` and `performance_suite_steady/`. Task 3 does not merge or
rename those roots.

## Focused Validation

The fast contract is registered as `benchmark_task3_suite_contract`. It checks
policy boundaries, seed validation, filtered fixed-seed order, structured runner
JSON consumption, profile fail-closed behavior, suite wiring, and the required
CTest registrations. `benchmark_task3_report_consumers_python` verifies stable,
unstable, profile, gate-ineligible, and legacy report-consumer behavior.

