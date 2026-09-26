---
related_code:
  - scripts/benchmark/benchmark_task4_contract.py
  - scripts/benchmark/aggregate_benchmark_summary.py
  - tests/performance/perf_runner.c
  - tests/performance/perf_process.c
  - tests/performance/perf_statistics.c
  - tests/performance/persistent_protocol.c
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/zr_runner/benchmark_server.c
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_persistent_commands.cmake
implementation_files:
  - scripts/benchmark/benchmark_task4_contract.py
plan_sources:
  - user: 2026-09-05 review plans and code, fix functionality before performance, retain original gates
  - docs/plans/astra/index.md
  - docs/plans/benchmark/optimize/00-audit-and-baseline.md
  - docs/plans/benchmark/optimize/01-measurement-and-gates.md
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
  - docs/plans/benchmark/optimize/04-aot-jit-parity.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
tests:
  - tests/benchmarks/test_benchmark_task4_integration.py
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
  - tests/benchmarks/test_benchmark_environment_contract.py
  - tests/acceptance/2026-09-05-astra-benchmark-finalization.md
doc_type: milestone-detail
---

# Benchmark Review And Repair Plan

## Scope And Evidence

Review base: `c95e5387` on `main`, with the initial September 1 runtime, frame
tests and benchmark-plan edits preserved. This review covers optimize plans
00-05 and the benchmark execution, statistics, environment and report chain.
Runtime optimization code is reviewed by its owning workers. No historical
performance result is promoted to current acceptance.

The existing Windows Task 4 integration suite passed all 11 tests before new
regressions on September 5. It exercises synthetic structured reports and
cache/publication behavior; it does not establish VM performance. WSL provides
Python 3.10.12 and CMake 3.22.1. Current full build discovery and controlled
benchmark scheduling belong to the root worker.

## Findings

### P1: Environment Finalization Changes Measurement Eligibility

Source: `scripts/benchmark/benchmark_task4_contract.py:80`,
`_recompute_benchmark_ratios`; producer:
`tests/performance/perf_runner.c:600`.

An isolated finalized environment triggers recomputation based only on PASS,
STABLE and positive medians. The postprocessor ignores the incoming
`comparable` and `gate_eligible` flags and the C baseline's status, stability and
scope. It can upgrade a one-sample record rejected by the runner, revive a
noncomparable record, or produce a ratio against an unstable or differently
scoped C sample. Conversely, without a C row it clears the eligibility of
otherwise valid steady-state ZR/Lua/.NET records. C has no persistent command,
so that converse affects the supported steady-state workflow directly.

Expected behavior: environment validity is an additional condition; it cannot
establish sample validity. Preserve independently valid record eligibility.
Only emit `relative_to_c` when both records are eligible and their recognized
measurement scopes match.

Owner: benchmark worker. Status: **planned**. This is the first bounded repair.

### P1: Process Samples Discard Their Results After Preflight

Source: `tests/performance/perf_process.c:295` redirects stdout/stderr to the
null device, while `tests/cmake/run_performance_suite.cmake:698` constructs the
preflight expected output. Measured process samples subsequently check exit
status only (`tests/performance/perf_runner.c:543`). A successful preflight does
not prove that every later fresh process returned the same checksum. A
state-dependent or nondeterministic computation can therefore produce a
seemingly stable, successful timing report with wrong measured results.

Persistent requests do check every DONE index/checksum and reject malformed,
timed-out, prematurely terminated and nonzero-STOP sessions. Process mode needs
its own bounded output/checksum contract, without moving parsing or compiler
preparation into the wrong measurement scope. This is a separate repair, not
part of the environment-finalization patch.

### P2: Representative Steady-State Coverage Is Incomplete

Source: `tests/cmake/benchmark_persistent_commands.cmake:18` and
`tests/benchmarks/zr_runner/benchmark_server.c:146` restrict persistent commands
to numeric/dispatch and ZR, Lua, QuickJS and .NET. Other implementations/cases
are explicitly skipped. That behavior is preferable to a false scope claim,
but the roadmap's full steady-state numeric/control/call and memory/object/GC
gates cannot currently execute. In particular, branch/call cases and the C
steady-state denominator are absent.

Add coverage incrementally with shared algorithm/checksum verification and
repeat-call state/lifecycle tests before collecting parity samples.

### P2: Equivalence And Statistical Acceptance Remain Incomplete

The registry records checksums and workload scales but does not implement
Task 5's algorithm version, representation class or operation counters. Equal
checksums alone do not establish equal complexity or allocation policy.
`compare_benchmark_summaries` reports ratios of medians; it does not calculate
a confidence interval for the before/after difference or enforce the original
3% improvement criterion. Per-run median bootstrap intervals must not be
misrepresented as that missing effect interval. These are explicit pending
gates, not proof that current optimizations are rejected or accepted.

## Current Capability Map

| Plan | Current capability | Remaining acceptance |
|---|---|---|
| 00 baseline | Historical process/Callgrind numbers identify frame/address costs. | Rebuild source identity and replay current evidence. |
| 01 Task 1 | Process/persistent scope and preparation/reuse fields exist. | Repair finalization and maintain fail-closed consumer behavior. |
| 01 Task 2 | Persistent pipes, deadlines, matching DONE/checksum, STOP and session RSS exist. | Expand beyond numeric/dispatch; replay runtime integration. |
| 01 Task 3 | Repetition calibration, 10+10 samples, MAD/CV/bootstrap and seeded ordering exist. | Preserve their eligibility decisions through publication; measure effect CI. |
| 01 Task 4 | Environment capture, source/build identity, affinity, cache and immutable publishing exist. | Repair finalization and replay full current integration. |
| 01 Task 5 | Per-case checksum and scale registry exist. | Algorithm/representation/operation-count equivalence is unfinished. |
| 02 interpreter | Dense/direct/packed frame and call optimizations have historical focused evidence. | Stable wall-clock and representative parity gates remain open. |
| 03 memory | Canonical arrays, shape/PIC, builder and GC slices are documented. | Full builder binding, representative throughput/RSS/pause evidence remain open. |
| 04 AOT | Main parser compiles archived AOT backend sources and runs backend contract tests. | No registered `zr_aot_c` benchmark runner or native/deopt coverage reporting yet. |
| 05 roadmap | Staged gates are explicit and performance gaps are already acknowledged. | Full current functional matrix and valid comparable representative samples. |

The archive directory name does not mean AOT source is unused. Backend
capability and a usable independently identified benchmark implementation are
different facts; the AOT worker owns that implementation inventory.

## Repair Design And Sequence

The selected repair keeps the existing Task 4 module and schema. A shared
eligibility predicate checks PASS, STABLE, explicit incoming comparability and
gate flags, recognized measurement scope, and a finite positive median. C
ratio eligibility additionally requires the C record to satisfy the same
predicate and match scope. An absent or rejected C baseline leaves each
record's own sampling eligibility intact and only suppresses its C ratio.

1. Add regressions to the existing Task 4 integration suite for low sample
   rejection, explicit noncomparability, missing qualification flags, invalid
   or mismatched scope, unstable/failed/ineligible baseline and absent C.
2. Run these tests before implementation and record the expected failures.
3. Implement the bounded finalizer fix with no runtime or CMake changes.
4. Run Task 4, environment, Task 3 report-consumer and statistics tests on WSL
   and Windows; verify an on-disk finalization/aggregate path.
5. Update the environment/publishing module document and acceptance record;
   report explicit paths and evidence to root for independent review/commit.

## Original Gates Preserved

- PR micro: same machine/scope, at least 10 stable samples, >3% regression with
  a 95% interval excluding zero fails. CV below 5% remains required.
- Optimization: target improvement >=3%, representative geomean regression
  <=1%, all correctness and memory checks passing.
- Nightly: full core/stress scope; >5% regression fails.
- Interpreter intermediate: numeric/control/call geomean `ZR/Lua <=3.0`.
- Interpreter final: full representative geomean `ZR/Lua <=2.0`, each case
  `<=5.0`, no unexplained RSS growth >5%.
- Original local targets remain: scalar 30%; call/dispatch 15%; array/object
  20%; mixed service 10%; string allocation -70% and wall time +20%; GC
  baseline overhead <10%, stress throughput +15%, p99 regression <=10%.
- Native: coverage >90%; geomeans `ZR/Lua <=1.25`, `ZR/QuickJS <=1.25`,
  `ZR/.NET <=1.5`; numeric/branch native execution <=2x C; original per-case
  guards stay in force. Compile time and artifact size are separate reports.
- Callgrind reductions explain hotspots; they do not substitute for missing
  or unstable wall-clock acceptance. JIT decision gates remain closed.

## Historical Artifact Leads

Recent acceptance records cite `/home/hejiahui/.cache/codex/` for Callgrind
and profile files, including `callgrind.numeric-packed-signed-slot-final.candidate.out`
and `profile.numeric-packed-signed-slot-final.json`. Task 4's intended reusable
build root is `${HOME}/.cache/zr-vm-benchmark/<source-key>/<toolchain-key>`.
Older `build/benchmark-gcc-release` and `build/codex-wsl-gcc-debug` examples are
not verified current paths. Root must confirm existence and provenance before
reusing any artifact.
