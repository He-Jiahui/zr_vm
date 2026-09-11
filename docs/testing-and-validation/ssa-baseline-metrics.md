# SSA baseline metrics contract

This document records the first executable slice of the SSA measurement plan
(`docs/plans/ssa/00-measurement-contracts/01-baseline-metrics.md`).  It keeps
the existing persistent runner as the source of raw timings and provides a
small, deterministic boundary model for later report/runner integration.

## Normalized sample

`SZrPerfBackendMetrics` (in `tests/performance/perf_backend_metrics.h`) keeps
the requested backend separate from the backend actually used.  It also
records the measurement phase, checksum/environment/workload identity, raw
samples, optional phase costs, and counters.  Optional counters are valid
only when their bit is present in `availableMetrics`; an unavailable value is
not represented as an invented zero.

Validation rejects malformed text, non-zero process exits, checksum or
environment failures, non-finite timings, unknown availability bits, and
out-of-range native coverage.  A structurally valid requested/actual mismatch
is retained as `FALLBACK_VISIBLE` so a fallback cannot be reported as a pure
backend result.

## Paired comparison

`ZrTests_Perf_ComparePaired` reuses `perf_statistics.c` for the median, sample
variation, and deterministic bootstrap interval.  Comparisons require the
same workload/checksum/environment, paired samples, and at least three
samples.  Excessive coefficient of variation is `INCONCLUSIVE`; mismatched
identity is `INCOMPARABLE`; fallback is visible and never gate eligible.

The performance gate is conservative: the lower bootstrap improvement bound
must be at least 3% before a result is `gateEligible`.  A point estimate above
3% with a lower bound below 3% is reported as `NO_GAIN`, not promoted.

## Focused verification

The focused executable is registered by `tests/cmake/ssa-tests.cmake` as
`zr_vm_ssa_baseline_metrics_test`, with CTest name
`ssa_baseline_metrics`.  It covers valid/unavailable fields, crash and
checksum rejection, visible fallback, stable improvement, confidence-bound
rejection, environment mismatch, noisy samples, and malformed optional data.

The first implementation was developed test-first: compiling the test before
the new header produced the expected missing-header failure, then the same
fixture passed after the contract and implementation were added.  The full
WSL GCC/Clang and Windows MSVC commands are recorded in the corresponding
acceptance entry once those configurations have been freshly configured.

This is a focused M0 slice, not the complete SSA baseline gate.  Persistent
runner phase/counter collection and representative workload capture remain
explicit follow-up batches; no historical benchmark result is reclassified by
this contract.
