---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: source `while` loop-exit CFG edges

## Scope

The compiler-owned source CFG now lowers a direct terminal `break;` in a
supported `while` body to the loop join and a direct terminal `continue;` to
the condition header. The semantic targets travel with the existing loop
label, while legacy ExecBC labels and runtime behavior remain unchanged.

An explicit loop-exit edge invalidates the current semantic body block. The
ordinary `while` epilogue therefore emits its implicit backedge only for a
fall-through body. The resulting graph preserves predecessor occurrence
counts for ExecIR construction: `break` gives the join its header-false and
body-exit predecessors, while `continue` gives the header its entry and body
predecessors.

## Focused fixtures

`test_source_while_break_targets_loop_join` checks the exact five-block graph,
the body-to-join normal edge, the join's two predecessors, and successful
ExecIR construction. `test_source_while_continue_targets_loop_header` checks
the body-to-header edge, the header's two predecessors, the join's single
false-edge predecessor, and the same builder boundary.
`test_source_while_linear_prefix_can_continue` verifies that already-modeled
assignment facts remain in the body before its explicit header edge.

`test_unreachable_after_loop_exit_blocks_later_cfg_startup` fixes the
conservative case where syntax follows a loop exit in the same body.
`test_suppressed_loop_exit_blocks_later_call_cfg` fixes a loop exit nested
under an enclosing fallback shape. Both require the persistent CFG startup
barrier and verify that a later resolved call cannot create a detached source
graph.

The `for`/`foreach` fixtures cover the shared conservative path. They require
an invalid semantic loop target to block a later call, abandon an already
active call CFG, and apply the same rule to `foreach` rather than relying on a
temporary scoped-suppression latch.

`test_declared_child_loops_do_not_pollute_entry_cfg` compiles supported
`while` and fallback `for` exits inside a declared child function before an
entry-body call. Declared callables still lack independently published
SemanticIR functions, so each child loop is compiled against a disposable
isolated state. The fixture requires the entry startup barrier to remain clear
and the entry call to retain its own invoke graph plus final exit block.

## TDD evidence

- The first focused run failed both direct-edge cases because the old loop
  preflight rejected `break` and `continue`, leaving only the two-block legacy
  graph instead of the expected five-block graph.
- After direct edge lowering, both conservative fixtures initially failed
  because fallback suppression was restored before the later call and the
  function-level startup barrier remained clear.
- The implementation now promotes either unsupported loop-body shape to the
  persistent barrier, including a loop exit encountered while source CFG
  startup is scoped-suppressed.

## Validation evidence (2026-09-18)

- MSVC 19.44, WSL GCC 11.4, and WSL Clang 14 focused
  `zr_vm_pre_semantic_ir_test` runs each passed 49/49 direct Unity cases.
- The adjacent `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_builder_fact_identity`,
  `ssa_place_eligibility`, `ssa_place_promotion`, and
  `ssa_value_validation` gate passed 7/7 on all three toolchains.
- The receiver-guard performance smoke passed 1/1 on all three toolchains.
- The WSL GCC ASan+UBSan build passed the same 49/49 producer cases with leak
  detection and both sanitizers configured to halt on the first error.
- MSVC `zr_vm_cfg_finally_abrupt_test` passed 7/7, including break and
  continue routing through finally. The directly relevant resource cleanup
  case for return/break/continue also passed; that broader binary remains
  19/20 because of its existing unrelated Unique parameter/return fixture.
- Wiki validation passed 116 Markdown files, 115 manifest pages, and 644 local
  links. Final independent review reported no findings.

## Boundary

This checkpoint covers only an unvalued `break;` or `continue;` that is the
last non-null statement in a supported source-owned `while` body, optionally
after already-modeled linear statements. Nested exits, valued exits, exits
with trailing reachable syntax, `for`/`foreach`, and transfers crossing
`finally` or ownership cleanup remain on conservative legacy lowering. It does
not claim the complete 01.02 exit gate.
