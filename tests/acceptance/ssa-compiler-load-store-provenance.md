# SSA 01.02: compiler-owned load/store value provenance

## Scope and baseline

The source compiler's pre-execution Semantic IR is the producer. This slice
keeps `GET_STACK` and `SET_STACK` execution selection unchanged while recording
the actual `LOAD` result in a temporary Place and using that ValueId for a
later local `STORE`. No ExecBC reverse projection or claimed full SSA build is
involved. The input fixture is `var value: int = 1; var copy: int = value;
value = copy;` in `tests/parser/test_pre_semantic_ir.c`.

Before the edit the MSVC standalone fixture exited 1 at the added provenance
assertion: `Expected 4 Was 5`. The final `LOAD` defined ValueId 4; `STORE`
allocated ValueId 5 without a producer, so a future SSA consumer could not
establish a dominating definition. The prior baseline without that assertion
was 11/11 passing Unity tests.

## Test inventory and boundaries

- The source-level opcode golden now includes each `LOAD` result's temporary
  `PLACE_BASE` and `INITIALIZE`; initialization references the actual load
  result and the same Place.
- The assignment `STORE.valueId` equals the last source `LOAD.resultValueId`;
  `SemanticIr_Value.definitionInstructionId` points to that load, not the store.
- The standalone `test_pre_semantic_ir.c` suite also covers ownership loans,
  constructed values, field initialization, and CFG flow behavior.
- `test_span_core.c` checks structured contiguous-view facts and runtime Span
  use; source-to-Place propagation must not silently detach its view value.
- Literal initialization and computed RHS values still have missing producer
  definitions; promotion, phis, exceptional blocks, Oracle and full four-
  backend parity remain outside this decision.

## Validation evidence (2026-09-18)

- MSVC 19.44 in `D:/zr-ssa-verify-871bc234` built
  `zr_vm_pre_semantic_ir_test`; direct run after the change reported
  `11 Tests 0 Failures 0 Ignored` and exited 0.
- WSL GCC in `D:/zr-ssa-verify-871bc234/wsl-gcc` built the same standalone
  target; direct run reported `11 Tests 0 Failures 0 Ignored` and exited 0.
- WSL Clang 14 in `D:/zr-ssa-verify-871bc234/wsl-clang` built the same
  target; direct run reported `11 Tests 0 Failures 0 Ignored` and exited 0.
- The adjacent MSVC `zr_vm_span_core_test.exe` reported 16 tests, 2 runtime
  `GET_MEMBER` failures in `span_array_runtime.zr:7` and
  `span_constant_bounds.zr:7`; its focused structured SemIR facts test passed.
  An older September 6 MSVC build passed 16/16, but many unrelated workspace
  edits separate the two binaries; whether these failures are caused by this
  slice has not been established. The whole Span target is **not** accepted
  by this evidence.
- The adjacent MSVC compiler integration target did not link: unresolved
  `ZrVmLibNetwork_Register` from `test_compiler_regressions.c.obj`; its
  current full-suite result is unknown.

## Acceptance decision

The focused producer fact is demonstrated on MSVC, WSL GCC, and WSL Clang.
Classification of the adjacent Span runtime failures and the unavailable
compiler-integration target remains open; this does not accept 01.02 as a
whole or claim general constant/computed expression SSA provenance.
