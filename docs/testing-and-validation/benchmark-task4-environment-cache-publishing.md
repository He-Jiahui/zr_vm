---
related_code:
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_task3_case_assembly.cmake
  - tests/cmake/benchmark_task4_environment.cmake
  - scripts/benchmark/benchmark_environment_contract.py
  - scripts/benchmark/benchmark_environment_schema.py
  - scripts/benchmark/benchmark_source_identity.py
  - scripts/benchmark/benchmark_task4_contract.py
  - scripts/benchmark/benchmark_report_publisher.py
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
implementation_files:
  - tests/cmake/benchmark_task4_environment.cmake
  - scripts/benchmark/benchmark_task4_contract.py
  - scripts/benchmark/benchmark_report_publisher.py
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
plan_sources:
  - user: 2026-08-30 optimize benchmark environment, cache, baseline comparison, and report publishing
  - docs/plans/benchmark/optimize/01-measurement-and-gates.md
tests:
  - tests/benchmarks/test_benchmark_task4_integration.py
  - tests/cmake/run_benchmark_task4_environment_contract_test.cmake
  - tests/acceptance/2026-08-30-benchmark-task4-integration.md
doc_type: workflow-detail
---

# Benchmark Task 4 Environment, Cache, And Publishing

## Purpose

Task 4 makes a benchmark result auditable before it can be compared. Linux and
WSL runs execute through `capture_benchmark_environment.sh`, which records the
build contract, source identity, one-CPU isolation evidence, and runtime
versions. The child suite can emit a provisional report while capture is
`IN_PROGRESS`; the wrapper finalizes the environment and the Task 4 postprocessor
then attaches the stable fingerprint to every report.

## Comparison Contract

Schema-3 reports carry an `environment` reference containing the finalized
fingerprint, isolation status, selected CPU, and explicit comparability status.
`NON_ISOLATED`, source changes during the run, profile records, missing or
invalid environments, unstable samples, non-gate-eligible records, duplicate
identities, mismatched checksums, scopes, build contracts, runtime versions,
measurement policies, or statistical algorithms produce `INCOMPARABLE` output.
The comparison helper recomputes ratios from raw median wall times and ignores
pre-existing speedup fields.

## WSL Cache

`build_benchmark_release.sh` performs a bootstrap configure with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`, derives the public cache identity from the
actual CMake cache and compile database, then removes the probe tree. On a cache
miss it configures and builds directly in
`${HOME}/.cache/zr-vm-benchmark/<full source key>/<toolchain key>`; it never moves
an already configured CMake tree. An existing cache is reused without rebuilding
only when its stored identity matches; source or build-contract changes fail closed.

## Publishing

`benchmark_report_publisher.py` copies report formats only (`json`, `md`, `csv`,
`html`, and text) into a destination-local staging directory, writes a SHA256
manifest, atomically renames the immutable run directory, and atomically updates
`LATEST`. Build directories and binaries never enter the published bundle.

## Test Coverage

The focused Python contract suite covers complete and incomplete environment
attachment, all baseline mismatch classes, median recomputation, cache-key
derivation, and immutable publication. The CMake contract test covers Linux
missing/provisional evidence and Windows diagnostic-only behavior. Shell syntax
is checked in WSL before running the minimal integration smoke.
