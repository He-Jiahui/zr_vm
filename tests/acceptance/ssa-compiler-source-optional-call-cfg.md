---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
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
suffix facts. A supported known member call then occupies a dedicated invoke
block and publishes its typed `CALL_*` instruction, resolved symbol, ordered
callable/receiver and argument operands, and result ValueId. Its normal
continuation reaches the join; its exception edge enters a propagation sink.
The ExecIR builder therefore emits one `INVOKE` with ordered normal/exception
successors instead of assigning the exceptional transfer to an earlier store.

The propagation sink deliberately has no instruction. ExecIR does not yet
represent an exception value defined by an incoming edge, so emitting `THROW`
would require a fabricated normal-entry payload. The sink's CFG terminator fact
keeps the exceptional boundary explicit until that payload contract exists.

The lowering snapshots the compiler's semantic slot bridge before the present
path and restores it at the join. Result-producing ownership operations now
bind their defining ValueId to the result stack slot, so a nullable
`wake(weak)` result is a defined branch operand rather than a stack-only value.
No execution-bytecode jump offset is inspected to construct this graph.

## Focused fixtures

`test_source_optional_call_skips_argument_semantic_effects` compiles a known
`void` member call with `side = true` as its argument. It checks the exact
seven-block topology, canonical present-true/absent-false ordering, the
present-to-invoke edge, ordered invoke normal/exception edges, a normal-to-join
edge, two join predecessors, and the empty exceptional sink. It also proves
that the argument's only semantic `STORE` belongs to the present block, the
typed call is the invoke block's tail with its receiver/callee and explicit
argument operands, and the
resulting ExecIR contains exactly one `INVOKE` and one exception block.

Value-producing nullable optional calls are covered by the follow-up
[nullable optional-call value merge](ssa-compiler-source-optional-value-cfg.md)
record.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 debug build under `D:/zr-ssa-verify-871bc234` built the
  focused target and passed 26/26 direct Unity cases.
- WSL GCC 11.4.0 and Clang 14.0.0 debug builds under the matching `wsl-gcc`
  and `wsl-clang` roots rebuilt the same target and each passed 26/26 direct
  Unity cases.
- On MSVC, GCC, and Clang, the adjacent `ssa_builder_cfg`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`,
  `ssa_builder_fact_identity`, `ssa_place_eligibility`,
  `ssa_place_promotion`, and `ssa_value_validation` gate passed 7/7.
- The WSL GCC ASan+UBSan build under `wsl-gcc-asan` passed 26/26 with leak
  detection and both sanitizers configured to halt on the first error.
- The receiver-guard performance executable passed its one test after the
  change on all three toolchains. The broader ownership-intrinsic executable
  retained the same four pre-existing failures and passed the nullable optional
  argument-skipping regression, so those failures are not attributed to this
  checkpoint.

## Boundary

This checkpoint covers nullable, value-discarded `void` optional calls with a
known member symbol and complete canonical operand/type facts. The follow-up
checkpoint merges nullable call values; Weak guard wake/cleanup, exception
payloads and enclosing handlers, cleanup exits, and source CFG startup for
general calls remain open.
The earlier untyped resource-construction boundary is closed by
[SSA 01.02: ownership facts through ExecIR](ssa-compiler-ownership-execir.md),
but the remaining control-flow items still prevent a complete 01.02 exit-gate
claim.
