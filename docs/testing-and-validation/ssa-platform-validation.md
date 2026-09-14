---
related_code:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - tests/cmake/ssa-platform-matrix.cmake
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/03-platform-matrix.md
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_platform_matrix.c
  - tests/acceptance/ssa-platform-matrix.md
doc_type: module-detail
---

# SSA Platform Validation Contract

## Purpose

The 10.03 platform matrix needs to distinguish a build artifact from a real
runtime acceptance.  This contract records target identity, C ABI witnesses,
backend capability, runner provenance, semantic witnesses, and explicit
unsupported or unavailable results without storing a host pointer or an
executable address.

The contract is deliberately in `zr_vm_common`: parser, AOT, ExecBC, JIT, and
test runners can share the same value-only verifier.  Host-specific adapters
remain responsible for discovering the values and for running a real device,
emulator, browser, or WASM runtime.

## Data Model

`SZrSsaPlatformCapability` is the declared target profile.  It carries a
target triple, desktop/mobile/WASM target, x86-64/AArch64/WASM architecture,
pointer width, endianness, alignment, integer/float widths, struct-return
class, callback ABI hash, ABI/numeric/layout hashes, backend feature bits, and
the allowed switch/computed-goto dispatch forms.

`SZrSsaPlatformArtifactContract` repeats the target and contract hashes at the
artifact boundary.  A pointer-width, endianness, alignment, numeric, layout,
target-triple, or callback ABI mismatch is rejected before the artifact is
accepted.  Restricted iOS patches cannot add native imports.  Android, iOS,
and WASM profiles cannot publish or execute machine-code JIT.

`SZrSsaPlatformObservation` records what actually happened: compiler, device or
runtime, backend, runner (`cross-compile`, `emulator`, `real-device`, or
WASM/browser runtime), compiled/executed/semantic-passed stages, unsupported
feature bits, dispatch implementation, and result/exception/source-map
witnesses.  `ZrCommon_SsaPlatform_IsRuntimeAcceptance` requires all three
stages and a `passed` outcome; cross compilation alone is never sufficient.

## Failure Semantics

- `BACKEND_UNSUPPORTED` means the selected backend was not declared by the
  target capability profile.
- `REQUIRED_FEATURE_UNSUPPORTED` identifies missing threads, concurrent GC,
  PMU, or other required features.  WASM PMU is not inferred from the target.
- `RUNTIME_UNAVAILABLE` is used for a cross-compiled or otherwise unexecuted
  observation with no semantic result.  It is not a pass.
- A `passed` observation carrying any `unsupportedFeatures` bit is rejected as
  `OBSERVATION_INVALID`; the convenience acceptance predicate also fails
  closed on malformed schema, ABI, identity, dispatch, or feature fields.
- `MACHINE_CODE_JIT_FORBIDDEN` rejects mobile/WASM JIT attempts before a pass
  can be reported.
- `SEMANTIC_WITNESS_MISMATCH` keeps result, exception, and source-map failures
  distinct from target/ABI failures and carries the mismatching witness kind.

Diagnostics retain target, backend, runner, source/instruction context and
expected/actual values.  Callers should preserve these fields in their matrix
reports instead of collapsing them into a Boolean.

## ABI and Dispatch Rules

The header contains C11 static assertions for eight-bit bytes, fixed-width
integer/float widths, and 32/64-bit pointers.  `DetectHostAbi` fills the
runtime ABI witness, while `ComputeAbiHash` gives adapters a deterministic
hash input.  Struct-return classification remains an explicit witness because
it cannot be inferred portably from `sizeof` alone.

Switch and computed-goto implementations may differ when their semantic,
exception, and source-map hashes match.  The matrix still records the dispatch
kind and rejects a dispatch form that the declared profile did not allow.

## Test Coverage

`tests/core/test_ssa_platform_matrix.c` covers:

- desktop ExecBC compiled/executed/semantic-passed evidence and missing runtime;
- 32/64-bit artifact mismatch and numeric/layout hash mismatch;
- WASM thread/concurrent-GC/PMU unsupported reporting;
- mobile/WASM machine-code JIT rejection and WASM32 pointer-width validation;
- cross-compile versus real-device distinction;
- restricted iOS native-import rejection;
- x86-64/AArch64 host JIT target checks;
- target-triple and callback ABI drift;
- switch/computed-goto semantic witness parity and diagnostic source context;
- unavailable backend reporting rather than silent pass-through.

## Validation Evidence

The focused test was compiled directly because this subtask intentionally does
not edit shared CMake registration:

```text
gcc 4.8.3 (Windows PowerShell): strict C11 compile and executable: PASS
gcc 11.4.0 (WSL Ubuntu 22.04): strict C11 compile and executable: PASS
clang 14.0.0 (WSL Ubuntu 22.04): strict C11 compile and executable: PASS
```

The complete command lines and sanitizer results are recorded in
`tests/acceptance/ssa-platform-matrix.md`.  Strict MSVC, GCC/Clang extended
warning syntax, C++ header, WSL ASan+UBSan, and Valgrind checks all pass for the
focused fixture.  Android, iOS, and WASM real runtime/device rows remain
`unavailable` in this host environment; no cross-compile result is promoted to
runtime acceptance.

## Plan Sources and Scope

This module and `tests/cmake/ssa-platform-matrix.cmake` implement the
value-only contract and configure-time registration boundary of 10.03.  The
CMake helper can register a real target/command or a visible unavailable skip;
it does not configure cross compilers, allocate executable pages, or claim
device/browser coverage.
