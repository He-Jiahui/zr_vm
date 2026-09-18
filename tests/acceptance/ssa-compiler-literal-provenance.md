# SSA 01.02: source compiler literal and local-initializer provenance

## Scope and failing baseline

The source compiler already emitted ExecBC `GET_CONSTANT`, but a literal
initializer's front-end Semantic IR local value had no defining instruction.
A new source-backed assertion in `test_pre_semantic_ir.c` failed on MSVC:
`Expected Not-Equal` for `definitionInstructionId` (zero), with the other
ten tests passing. The input is `var value: int = 1; var copy: int = value;
value = copy;`.

The producer now records `CONSTANT` with a real result ValueId and a marked
compiler constant-pool index before choosing `GET_CONSTANT`. Literal stack
slots and previously unmaterialized normalized destinations preserve the same
source value. The local
initializer uses the existing `CONVERT` to define its typed value from that
source; the test checks the entire definition chain and the unchanged
LOAD-to-STORE identity. The opcode golden explicitly captures each new
temporary Place, initialization, and conversion.

## Validation evidence (2026-09-18)

- MSVC 19.44 from `D:/zr-ssa-verify-871bc234`: fresh target rebuild,
  direct `zr_vm_pre_semantic_ir_test.exe` 11/11 passing.
- WSL Clang 14 from `D:/zr-ssa-verify-871bc234/wsl-clang`: fresh target
  rebuild, direct `zr_vm_pre_semantic_ir_test` 11/11 passing.
- WSL GCC from `D:/zr-ssa-verify-871bc234/wsl-gcc`: fresh target rebuild,
  direct `zr_vm_pre_semantic_ir_test` 11/11 passing.
- MSVC adjacent SSA builder fact identity, dominance, and CFG standalone
  targets rebuilt against the enlarged SemIR instruction and each passed.
- Adjacent MSVC `zr_vm_span_core_test.exe`: fresh rebuild, 14/16 passing;
  the same two runtime `GET_MEMBER` failures at `span_array_runtime.zr:7`
  and `span_constant_bounds.zr:7` observed before this literal change.
  Structured contiguous-view fact coverage passes; whole Span suite is
  **not** accepted.

## Boundary

This establishes an index into the compiler's constant pool, not an embedded
standalone literal payload. No claim of general computed-expression producers,
already-materialized destination overwrites, pruned phi construction, Oracle
equivalence, or complete 01.02 acceptance is
made. Upper-layer regression classification remains pending; no full-suite
or sanitizer result is inferred from the focused passes.
