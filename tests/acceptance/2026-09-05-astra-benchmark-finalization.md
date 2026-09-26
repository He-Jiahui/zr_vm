# Astra Benchmark Finalization Repair

## Scope

The Task 4 report finalizer now preserves the benchmark runner's measurement
eligibility. Environment attachment may invalidate a report, but a complete
environment cannot upgrade an unstable, failed, noncomparable, undersampled,
or invalid-scope record. C-relative ratios require eligible records on both
sides and an identical measurement scope.

Affected layers: `benchmark_task4_contract.py`, schema-3 report finalization,
and the Python integration contract tests. No VM runtime or CMake source was
changed.

## Baseline

Before the repair, an isolated environment caused `_recompute_benchmark_ratios`
to derive eligibility from status, stability and positive medians only. It
could turn `gate_eligible=false` into true, accept missing qualification
fields, and compare a valid record against an unstable or differently scoped C
baseline. With no C row, it also cleared the valid record's own eligibility.

## Test Inventory

- Existing Task 4 environment, cache, report and publication tests.
- Rejected record matrix: undersampled, failed, unstable, noncomparable,
  missing qualification, empty/unknown scope.
- Baseline matrix: failed, unstable, noncomparable, ineligible, mismatched
  scope and invalid median C records.
- Missing C baseline and finalized on-disk report cases.
- Existing report-consumer and statistics suites are run as regression checks.

## Tooling Evidence

Windows PowerShell, Python 3.12 environment:

```text
python -B tests/benchmarks/test_benchmark_task4_integration.py -v
Ran 16 tests in 1.243s
OK
```

The new tests were first run before implementation and produced 18 expected
failures, all at the finalizer's eligibility overwrite. WSL tooling inventory
reported Python 3.10.12 and CMake 3.22.1; root owns the current CMake build and
controlled performance replay.

## Results

The finalizer now uses an explicit gate-eligibility predicate requiring PASS,
STABLE, `comparable=true`, `gate_eligible=true`, a recognized scope and a
positive finite median. It leaves independently valid rows eligible without a
C denominator and nulls only the unavailable ratio. All 16 focused tests pass.

## Acceptance Decision

Accepted for the bounded report-finalization repair. Full VM performance gates
remain open: stable comparable representative samples, process checksum
verification, algorithm/representation equivalence, interpreter geomeans and
AOT native coverage still require the follow-up work recorded in
`docs/plans/astra/benchmark/review.md`.
