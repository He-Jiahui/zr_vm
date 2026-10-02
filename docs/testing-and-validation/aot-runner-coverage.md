---
related_code:
  - tests/benchmarks/aot_runner/aot_coverage.c
  - tests/benchmarks/aot_runner/aot_coverage.h
  - tests/benchmarks/aot_runner/aot_runner.c
  - tests/benchmarks/aot_runner/aot_runner.h
  - tests/benchmarks/aot_runner/main.c
  - tests/performance/perf_report.c
  - tests/performance/perf_report.h
implementation_files:
  - tests/benchmarks/aot_runner/aot_coverage.c
  - tests/benchmarks/aot_runner/aot_runner.c
  - tests/benchmarks/aot_runner/main.c
  - tests/performance/perf_report.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/04-aot-runner-coverage.md
tests:
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/2026-10-02-ssa-coverage-sampling-merge.md
doc_type: testing-guide
---

# AOT runner and semantic coverage contract

The SSA 07.04 runner is a standalone, callback-driven consumer of generated
artifacts. It never relabels the interpreter or zr_binary as AOT. A generated
module registers an entry as aot_c or aot_llvm, together with a stable non-zero
entry token and a checksum callback:

    register(compiled backend, entry token, name, invoke)
    run(requested backend, entry token, expected checksum)
            -> requested backend, actual backend, status, checksum, coverage

ZrTests_AotRunner_Run reports RAN, FALLBACK, UNAVAILABLE, or FAILED. The
requested and actual backends are retained independently. An interpreter entry
can satisfy a request only when the caller explicitly enables fallback; the
result remains FALLBACK and is not a pure-AOT result. Missing entries,
invocation errors, malformed coverage, and checksum mismatches each retain a
specific failure code.

When a registered C or LLVM entry reports sampled interpreter sites, the
runner also reports FALLBACK and sets `coverage.mixedExecution`, while keeping
the compiled backend as `actualBackend`. Native-helper sites alone do not cause
fallback. Counts without a non-zero sampling rate leave coverage unavailable
and do not invent a fallback status.

## Coverage denominator

SZrAotCoverageCounts counts semantic work, not emitted machine instructions.
Native code, runtime helpers, interpreter fallback, and deoptimizations are
separate counters. semanticSites is the static denominator and
executedSemanticSites is the dynamic denominator. A fused operation does not
remove its original semantic sites. Sampling metadata is explicit: without a
non-zero sample rate, ratios are UNAVAILABLE; an empty denominator is never
rendered as 100%. Native-helper and interpreter shares remain visible in mixed
execution.

ZrTests_AotCoverage_Record and ZrTests_AotCoverage_Merge are transactional and
overflow checked. A failed increment or merge leaves the input unchanged. The
validator rejects a declared denominator smaller than observed semantic work
and rejects fallback time greater than total time.

Sampling rates describe the work contributed by each input. A merge adopts the
source rate only when the destination has no declared or observed semantic
sites. An empty source leaves a nonempty destination's rate intact, even when
the source carries a different rate. When both inputs contribute work, a
missing rate or different rates makes the aggregate rate unknown (`0`). That
unknown aggregate stays unavailable through later merges with exact samples;
additional samples cannot reconstruct lost sampling information. Declared
sites without observed hits count as nonempty work for this rule. Equal known
rates remain known; only the full sampling rate produces `exact=true`.

The regression and direct GCC/Clang/MSVC evidence are recorded in
[the sampling merge acceptance](../../tests/acceptance/2026-10-02-ssa-coverage-sampling-merge.md).

## Phase and artifact report

ZrPerfReport_WriteAotJson writes the optional AOT phase report used by release
automation. Compile, link, load, startup, and run costs are independent
fields; -1 is serialized as null, so an unavailable phase is not confused with
a zero-cost phase. The report also records requested/actual backend, entry
token, artifact hash, toolchain, checksum/failure status, RSS/code size
availability, and all semantic coverage counters. It is valid only when the
structural identity and counter invariants pass
ZrPerfReport_ValidateAotPhase.

The benchmark registry declares aot_c and aot_llvm in
ZR_VM_BENCHMARK_AOT_IMPLEMENTATION_ORDER, with availability and runner target
fields defaulting to false/empty until a generated artifact provider is
configured. This prevents an unavailable native backend from being silently
substituted during the existing cross-language suite.

## Focused verification

The focused CTest target is
zr_vm_ssa_aot_runner_coverage_test (ssa_aot_runner_coverage). It covers:

- native/helper/interpreter denominator accounting and fusion-safe static
  coverage;
- zero-data UNAVAILABLE reporting;
- overflow and malformed denominator rejection;
- requested/actual backend and checksum preservation;
- explicit fallback, missing backend, invocation failure, and matrix
  diagnostics.

Use:

    cmake --build build/ssa-gcc-debug --target zr_vm_ssa_aot_runner_coverage_test -j 4
    ctest --test-dir build/ssa-gcc-debug -R '^ssa_aot_runner_coverage$' --output-on-failure --no-tests=error

The test is a contract fixture, not a claim that a host currently has a C or
LLVM generated artifact. Representative workload coverage and the 90% gate
remain UNAVAILABLE until each artifact provider supplies reproducible
compile/link/load evidence and per-workload counters.
