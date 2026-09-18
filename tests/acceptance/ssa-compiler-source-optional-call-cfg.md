---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
status: partial
---

# SSA 01.02: nullable optional-call SemanticIR CFG

## Scope

The source compiler now publishes a canonical present/absent diamond for
`receiver?.method(arguments)` when the receiver-guard fact is nullable and the
complete chain has `VOID_NOOP` result lift. The receiver ValueId owns the
conditional branch. Its ordered true edge enters the present block; its false
edge skips directly to the join. The present path alone owns argument and
suffix facts, then reaches the same join through a normal edge.

The lowering snapshots the compiler's semantic slot bridge before the present
path and restores it at the join. Result-producing ownership operations now
bind their defining ValueId to the result stack slot, so a nullable
`wake(weak)` result is a defined branch operand rather than a stack-only value.
No execution-bytecode jump offset is inspected to construct this graph.

## Focused fixtures

`test_source_optional_call_skips_argument_semantic_effects` compiles a known
`void` member call with `side = true` as its argument. It checks the exact
four-block topology, canonical present-true/absent-false ordering, two join
predecessors, and that the argument's only semantic `STORE` belongs to the
present block.

`test_unmodeled_optional_value_abandons_partial_source_cfg` first starts a
source `if` graph and then compiles a value-producing optional member chain.
It requires the producer to remove all synthetic branch instructions and
retain the legacy two-block graph instead of publishing a partially correct
optional CFG.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 debug build under `D:/zr-ssa-verify-871bc234` built the
  focused target and passed 25/25 direct Unity cases.
- WSL GCC 11.4.0 and Clang 14.0.0 debug builds under the matching `wsl-gcc`
  and `wsl-clang` roots rebuilt the same target and each passed 25/25 direct
  Unity cases.
- On MSVC, GCC, and Clang, the adjacent `ssa_builder_cfg`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`,
  `ssa_builder_fact_identity`, `ssa_place_eligibility`,
  `ssa_place_promotion`, and `ssa_value_validation` gate passed 7/7.
- The WSL GCC ASan+UBSan build under `wsl-gcc-asan` passed 25/25 with leak
  detection and both sanitizers configured to halt on the first error.
- The receiver-guard performance executable passed its one test after the
  change. The broader ownership-intrinsic executable retained the same four
  pre-existing failures both with and without the ownership result-slot bridge,
  so those failures are not attributed to this checkpoint.

## Boundary

This checkpoint covers nullable, value-discarded `void` optional calls. It does
not yet merge nullable values, model Weak guard wake/cleanup in the canonical
CFG, split exception or cleanup exits, or lower the call operation itself into
source-owned `CALL_*` facts. The complete ownership setup fixture also cannot
yet pass through the ExecIR builder because the earlier resource-construction
call still lacks a canonical result TypeId. These remaining items prevent a
complete 01.02 exit-gate claim.
