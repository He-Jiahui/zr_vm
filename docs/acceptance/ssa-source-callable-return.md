---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/include/zr_vm_parser/core_function.h
  - tests/parser/test_ssa_source_callable_return.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_callable_return.c
  - docs/parser-and-semantics/ssa-source-callable-return.md
doc_type: acceptance
status: accepted-direct-source-green
---

# Source Script Callable Return Metadata Acceptance

## Scope

This is one finite gate in the broader SSA/source-to-AOT plan. It checks that
ordinary public source compilation publishes a narrowly proven SCRIPT entry
return type through the existing `SZrFunction` callable-return fields. No new
Core field or ABI, canonical child identity, retained IR, token/frame data, or
AOT artifact is part of this gate.

The focused fixture has eight Unity cases and makes twelve ordinary public
source-compile calls. It does not execute compiled source. Four eligible
positive cases cover `1 < 2`, `2 < 1`, `1 > 2`, and `2 > 1`; the function
returned by the first compile remains rooted while a second compile runs, so
the test can inspect its metadata after the temporary compilation state is
released. Four refusal guards cover a fallthrough branch, mixed i64/bool
returns, an operandless return, and an implicit-return script. Every guard
must still compile ordinarily and must not advertise the i64 callable result.

## Frozen red evidence

The pre-fix run is captured by `source-callable-return-red-v1` at
`E:\cargo-targets\zr_vm\reports\ssa-20261004-01a0fe2b\source-callable-return-red-v1\receipt.json`.
The receipt SHA-256 is
`1d9013bd6f684ca815a551b8ed8de4a670362ce2345f5448955f6e2be5acacd5`.
It records the expected red: all four positive assertions failed on missing
script-entry callable return metadata, all four guards passed, and no
precondition or UBSan failure occurred. The frozen test source SHA-256 is
`5ed8fd4f3cc0027a1890b8bcd87d2d2864d2e78a438acfe9a993649141ef9299`.

This red receipt is diagnostic evidence only; it is not a passing acceptance
result.

## Green evidence

The reviewed green receipt is
`E:\\cargo-targets\\zr_vm\\reports\\ssa-20261004-01a0fe2b\\source-callable-return-green-v1\\receipt.json`.
The direct build directory is
`E:\\cargo-targets\\zr_vm\\build\\ssa-20261004-01a0fe2b\\metadata-guards-direct-v2`.
Windows clang-cl 19 built Debug UBSan binaries with an 8 MiB stack. The
focused fixture passed 8/8 cases. The companion CTest run passed all eight
groups: 4 comparisons, 13 regressions, 6 loops, 7 scalar guards, 63 metadata
guards with 346 preconditions, 35 straight-line cases, and 8 callable-return
cases; the standalone scratch eligibility check also passed. Failures and
ignored cases were zero, and no UBSan diagnostic was emitted. The final CTest
log SHA-256 is
`968cc6600f796ca2cb81a04fb6f54a3d56846b54dd9f51c22b2ca36b89bdb8bd`.
The immutable red receipt remains preserved.

The focused eight-case fixture was run alongside the current direct
source-SSA comparison, regression, loop, scalar-guard, metadata-guard, and
straight-line groups, plus the standalone scratch-eligibility check. Report
The acceptance is limited to this direct route and does not claim failure
rollback after module-summary finalization, OOM injection, or broader native
coverage. Results come from the CTest log, not a build-only pass.
The ordinary top-level CMake route, Linux route, native 32-bit route, ASan,
LLVM/AOT backends, full parser-leaf suite, and full 47-item SSA plan are not
claimed by this bounded acceptance record.

## Safety boundary

The allowed evidence is local compilation and test execution over parser/Core
VM code. Do not run network, FFI, provider, capability, hotpatch, or external
service functionality, and do not probe those security boundaries. No external
calls are required for this metadata contract.
