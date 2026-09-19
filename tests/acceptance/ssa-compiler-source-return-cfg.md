---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
status: partial
---

# SSA 01.02: explicit source return CFG

## Scope

An explicit return in the compiler-owned entry body now consumes a canonical
ValueId and emits a one-operand SemanticIR `RETURN`. The current block closes
with `ZR_PARSER_CFG_TERMINATOR_RETURN`, has no successors, and becomes the CFG
exit. Bare `return;` uses the language's existing null result convention and
materializes that value through the semantic literal bridge first.

A return can establish the first source CFG block or terminate the normal
continuation of an existing typed-call `INVOKE` graph. Later unreachable source
still compiles to legacy ExecBC, but cannot add SemanticIR instructions or
start a detached CFG.

## Focused fixtures

`test_source_return_terminates_cfg_and_blocks_unreachable_restart` returns a
loaded local, then compiles unreachable call, branch, store, loop, and logical
source. It requires one payload-bearing SemanticIR and ExecIR `RETURN`, no
semantic call, and one zero-successor entry/exit block.

`test_source_void_return_uses_null_value_terminator` fixes the language-level
bare-return convention: the compiler emits a typed null constant and uses its
ValueId as the single return operand. The fixture checks the null constant-pool
type, ValueId identity, and one-operand ExecIR `RETURN`; a trailing call remains
semantic-dead.

`test_source_return_terminates_existing_invoke_cfg` first emits a resolved
typed call with ordered normal and exception successors. The return must close
the normal continuation, become the CFG exit, and lower alongside exactly one
ExecIR `INVOKE`; the trailing unreachable call cannot add a second call fact.

`test_conditional_source_return_blocks_later_cfg_startup` keeps a return inside
an unsupported conditional shape. Seeing the return while scoped startup is
suppressed promotes a function-level barrier, so a later call cannot publish a
graph that omits the conditional exit.

`test_unmodeled_return_payload_blocks_later_cfg_startup` uses an arithmetic
return value without a canonical producer ValueId. It remains legacy-only and
blocks the same detached restart.

`test_finally_return_preserves_precleanup_value` verifies the bounded
single-outcome `try/finally` path: the return operand is loaded before cleanup,
cleanup reaches a dedicated SemanticIR `RETURN` block, and unreachable later
calls remain absent. Nonlinear returns and multiple completion kinds remain
fail-closed.

`test_declared_child_return_does_not_pollute_entry_cfg` compiles the ownership
expression `return own Value()` in a declared child before the entry return.
The child lowers through a disposable SemanticIR state and must leave the
entry-body instruction, Value, Place, loan, slot, receiver-loan, and CFG-block
counts unchanged; only the later entry return may publish its typed constant
and terminator.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 each rebuilt and
  passed the focused pre-execution SemanticIR suite 40/40.
- GCC 11.4.0 with AddressSanitizer and UndefinedBehaviorSanitizer passed the
  same 40/40 suite with leak detection enabled and halt-on-error behavior.
- The adjacent SSA builder, dominance, control-edge, fact-identity,
  Place-eligibility, Place-promotion, and ValueId-validation gate passed 7/7
  under MSVC, WSL GCC, and WSL Clang.
- The receiver-guard performance contract passed 1/1 under all three
  toolchains.
- Wiki validation passed for 116 Markdown files, 115 manifest pages, and 644
  local links.

## Boundary

This checkpoint does not yet model return edges from both arms of a branch,
loop-local abrupt control, finally cleanup, ownership cleanup, or independently
published pre-execution functions for declared child callables. Those paths
remain on legacy lowering or block later CFG startup. The existing zero-operand
synthetic `RETURN` used to finish a naturally falling-through graph remains
distinct from the source return, whose runtime convention carries one value.
