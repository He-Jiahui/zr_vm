---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - tests/parser/test_ssa_source_script_entry_tokens.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_script_entry_tokens.c
  - docs/parser-and-semantics/ssa-source-script-entry-tokens.md
doc_type: acceptance
status: accepted-focused-green-validation
---

# Source Script Entry Tokens Acceptance

## Finite scope

This record covers existing metadata token publication for an actual pure,
versionless, zero-export SCRIPT entry with a proven no-argument i64 result.
The focused public-API fixture contains eight cases and twelve compile calls:
four LT/GT positive cases and four fallthrough/mixed/operandless/implicit-return
guards. It checks existing `MODULE`, `MEMBER_DEF`, paired `SIGNATURE` records,
the `METHOD_SIG` blob, lookup consistency, module signature hash and lifetime
across a second compile. It does not execute compiled source or metadata
tokens.

## Frozen RED V2 evidence

The accepted diagnostic receipt is
`E:\cargo-targets\zr_vm\reports\ssa-20261004-01a0fe2b\source-script-entry-tokens-red-v2\receipt.json`.
Its SHA-256 is
`e09e6e21d7d50fc6999ceeab92dbcb8615ffc6cfa6bd6f53b49ec68ceb48734b`.
The frozen test source SHA-256 is
`14265beff7553612ed853100dae4731083c69b81a3c2f3fcfb98464783a9be71`.
Configure and build exited zero; focused CTest exited 8 with four exact
missing-`MODULE` failures and four guards passing. The receipt confirms the
owned inputs remained unchanged and no source copy was created. This is the
intended pre-fix failure, not a passing acceptance result. Earlier V1 controller
space-parser evidence remains preserved but is not the accepted RED baseline.

## GREEN evidence

Focused GREEN V8 rebuilt the direct target and ran exactly
`ssa_source_script_entry_tokens`. Configure/build/CTest completed with native
exit codes 0/0/0; CTest reported 8 tests, 0 failures and 0 ignored. The log is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\script-entry-green-v8\ctest.log`.
The test ran with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and emitted
no sanitizer diagnostic. The executable was built from the current checkout
in `E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2`;
no source snapshot was created. This focused result is accepted for the finite
token gate and does not imply the broader SSA plan is complete.

The current direct build directory is
`E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2`.
Validation must use the current source checkout directly without generating a
source snapshot. The previous callable-return gate has its separate accepted
receipt; it does not by itself accept the new token publication gate.

## Explicit limits

No new Core field or ABI, token execution, ZRP bridge, retained IR, frame
metadata, AOT artifact, OOM injection, global rollback guarantee or full
47-item SSA acceptance is claimed. Linux, native 32-bit, ASan, LLVM/AOT and
ordinary full top-level CMake routes are not established by this focused
record. Allowed validation is local parser compilation and metadata inspection.
Do not invoke network, FFI, provider, capability, hotpatch or external services,
and do not probe their security boundaries.
