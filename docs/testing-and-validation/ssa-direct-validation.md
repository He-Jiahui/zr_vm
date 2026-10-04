---
related_code:
  - tests/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-analysis-fact-capacity-tests.cmake
  - tests/cmake/ssa-branch-range-null-tests.cmake
  - tests/cmake/ssa-branch-arithmetic-constraints-tests.cmake
  - tests/cmake/ssa-branch-loop-ranges-tests.cmake
  - tests/cmake/ssa-aot-scalar-arithmetic-tests.cmake
  - tests/cmake/ssa-aot-scalar-conditional-tests.cmake
  - tests/cmake/ssa-licm-scalar-legality.cmake
implementation_files:
  - tests/cmake/ssa-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-analysis-fact-capacity-tests.cmake
  - tests/cmake/ssa-branch-range-null-tests.cmake
  - tests/cmake/ssa-branch-arithmetic-constraints-tests.cmake
  - tests/cmake/ssa-branch-loop-ranges-tests.cmake
  - tests/cmake/ssa-aot-scalar-arithmetic-tests.cmake
  - tests/cmake/ssa-aot-scalar-conditional-tests.cmake
  - tests/cmake/ssa-licm-scalar-legality.cmake
plan_sources:
  - "user: 2026-10-04 ongoing SSA development with direct actual-checkout validation and no source snapshots"
tests:
  - tests/parser/test_ssa_loops_specialization.c
  - tests/parser/test_ssa_aot_scalar_text.c
  - tests/parser/test_ssa_analysis_fact_capacity.c
  - tests/parser/test_ssa_branch_range_null.c
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/parser/test_ssa_aot_scalar_conditional.c
  - tests/parser/test_ssa_licm_scalar_legality.c
doc_type: module-detail
---

# Direct checkout SSA validation

## Purpose and integration

The standalone CMake source directory at
`tests/cmake/ssa-direct-validation` registers nine finite SSA tests against
production inputs in the actual checkout. It permits a bounded native gate
without configuring the full module and third-party target graph. The seven
shared fragments remain usable from the repository's normal test registration.
`tests/CMakeLists.txt` and `tests/cmake/ssa-tests.cmake` provide that parent
context; this integration does not change those central files.

Each fragment derives `ZR_SSA_SOURCE_ROOT` from its own
`CMAKE_CURRENT_LIST_DIR/../..`. The direct driver derives it from
`CMAKE_CURRENT_LIST_DIR/../../..`. The built-in `CMAKE_SOURCE_DIR` continues to
identify the real configured CMake source directory. Normal `project()` compiler
discovery and executable ABI detection run; compiler ID, compiler WORKS and
try_compile success are not forced.

## Source closure and failure checks

The driver explicitly registers loop support, then the original scalar-text
consumer, then includes the seven shared fragments. LICM reuses the loop
support closure after removing the loop fixture and adds the interpreter
translation units. Scalar text starts with six sources; the conditional
fragment normalizes existing source paths and adds arithmetic and conditional
helpers only when absent. Both emitter fixtures include the complete helper
closure.

| CTest name | Configured translation units |
| --- | ---: |
| `ssa_loops_specialization` | 21 |
| `ssa_aot_scalar_text` | 8 |
| `ssa_analysis_fact_capacity` | 1 |
| `ssa_branch_range_null` | 22 |
| `ssa_branch_arithmetic_constraints` | 22 |
| `ssa_branch_loop_ranges` | 22 |
| `ssa_aot_scalar_arithmetic` | 8 |
| `ssa_aot_scalar_conditional` | 8 |
| `ssa_licm_scalar_legality` | 26 |
| Total | 138 |

Configuration resolves every target source and rejects missing files, paths
outside the checkout and duplicate translation units within a target. It writes
paths to `direct-target-sources.tsv` in the build directory. The capacity fixture
embeds the ranges implementation once: it belongs in compiler dependencies,
not as an additional translation unit. Ninja dependencies provide evidence for
headers and embedded C beyond the primary source graph.

## Assertions, sanitizers and local process limits

Assertions remain active through `/UNDEBUG` or `-UNDEBUG`. UBSan is enabled by
default and requires Clang. With an MSVC frontend, both sanitizer driver options
must appear before `/link` in the toolchain link rule; the driver checks the
positions of `-fsanitize=undefined` and `-fno-sanitize-recover=all`. Instrumented
compile options and frame pointers are applied to all nine targets. CTest uses
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, SSA/direct-validation labels
and a 30-second timeout per test.

Root runs the native tools in its owned Job wrapper with child-only environment
settings. Configuration, build and CTest have finite limits of 180, 900 and
330 seconds within an overall 1500-second gate. An accepted operation must exit
naturally with zero status, an empty aggregate Job and no termination actions
or close errors. The build uses `--parallel 1`. LLVM, VC and SDK inputs stay
read only; build, temporary and report outputs remain under the permitted
`E:/cargo-targets/zr_vm` task directories.

Production source snapshots, copied source trees and captured source contents
are prohibited. The driver reads source and header files directly from
`E:/Git/zr_vm`. Generated CMake/Ninja controls, path graphs, dependency records,
hashes and execution logs are validation artifacts. They do not replace the
checkout as compiler input. No network, runtime security or unrelated external
process test is part of this finite gate.

## Completed V31 evidence

Root accepted native Clang + LLD UBSan configuration, 138 actual compile edges,
the nine-target build and nine passing CTests with zero failures. The exact
source directory was `E:/Git/zr_vm/tests/cmake/ssa-direct-validation`; the build
directory was
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31`, using
`E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/direct-llvm-toolchain-v31.cmake`
and its companion `direct-llvm-rules-v31.cmake`. The driver's
[README](../../tests/cmake/ssa-direct-validation/README.md) records configure
arguments and toolchain construction instructions.

The receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31/Root-receipt.json`,
258995 bytes, SHA256
`17c71e728b1d84f679bfa7d2d60f2f46793a3c76ab6430782509d244b39e88e2`.
It records 525.4708234 seconds overall. Configure, build, CTest, Ninja
dependencies and Ninja commands each ended naturally with zero exit status,
zero active Job processes, no actions and no close errors. The source graph
and compiler dependency records identify real checkout inputs.

Independent adoption passed all 40 checks. The report is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-direct-cmake-v31-current-v1.json`,
12198 bytes, SHA256
`506ad7a314b4b37af3a75af6b746e23298742f73b9079d8d9da9b1a4bb78802e`.
It verified current source/tool/resource/product pins, all 138 compile edges,
UBSan and `UNDEBUG` settings, complete scalar-text helpers and nine passing
CTest results. The actual dependency union contains 257 real input files:
109 checkout files (67 Ninja header/embedded inputs plus 42 primary sources)
and 148 read-only system dependencies.
The independent report accepted the finite gate without source snapshots.

## Acceptance limits

V31 establishes this finite integration on the recorded native Clang + LLD
UBSan configuration. Full47, the full repository gate, typed AOT producer and
complete SSA milestones remain open. Linux, the MSVC compiler, ASan and native32
acceptance are not established by this run or its independent adoption.
