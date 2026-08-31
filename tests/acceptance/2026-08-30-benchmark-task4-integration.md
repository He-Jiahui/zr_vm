# Benchmark Task 4 Integration

## Scope

- Integrate the finalized schema-2 environment contract with schema-3 benchmark reports.
- Keep Linux/WSL comparisons isolated to one logical CPU and complete environment evidence.
- Add fail-closed baseline comparison, source/build keyed WSL caches, and report-only atomic publishing.
- Windows remains diagnostic and non-comparable until native affinity capture is available.

## Baseline

Task 3 produced schema-3 measurement reports and report consumers, but had no
post-capture environment attachment, baseline summary comparison, keyed WSL
cache lifecycle, or immutable report publisher. Existing environment-contract
tests passed 41 cases on Windows (8 platform skips) and 41 cases on WSL before
this integration.

## Test Inventory

- `tests/benchmarks/test_benchmark_task4_integration.py`: 11 focused Python tests.
- `tests/cmake/run_benchmark_task4_environment_contract_test.cmake`: Linux provisional/missing evidence and Windows diagnostic contract.
- Existing `tests/benchmarks/test_benchmark_task3_report_consumers.py`: 5 regression tests.
- Boundary cases: invalid/in-progress/missing environment, non-isolated capture, source changed during run, profile, unstable and ineligible rows, duplicate case/implementation identities, checksum/scope/build/runtime/policy/statistical mismatches, stale speedup fields, cache identity drift, duplicate published run, and binary exclusion.

## Tooling Evidence

- Windows PowerShell: `python -B tests/benchmarks/test_benchmark_task4_integration.py -v` -> 11 passed.
- Windows PowerShell: `python -B tests/benchmarks/test_benchmark_task3_report_consumers.py -v` -> 5 passed.
- Windows PowerShell: CMake Task 4 contract script -> passed.
- WSL: `bash -n scripts/benchmark/*.sh` -> passed after normalizing touched shell files to LF.
- WSL: CMake Task 4 contract script -> passed.
- WSL: configure/build/CTest smoke was attempted with GCC Debug and timed out after
  430 seconds before producing output; this remains an execution-environment
  limitation, not a passing full benchmark run.

## Results

- COMPLETE isolated environments attach stable fingerprint and preserve recomputed median ratios.
- Incomplete, invalid, non-isolated, source-changed, profile, unstable, or otherwise non-comparable inputs null ratios and gates.
- Baseline comparison emits explicit reasons and never trusts existing speedup fields.
- Cache derivation includes the actual compile database target evidence and rejects identity drift.
- Publisher writes a manifest, immutable run directory, atomic `LATEST`, and no binaries.

## Acceptance Decision

Accepted for the focused Task 4 orchestration contract. Full cross-language
benchmark execution remains a separate WSL smoke because it depends on the
available external runtimes and a configured release build. No C runner,
protocol, or core runtime files were changed.
