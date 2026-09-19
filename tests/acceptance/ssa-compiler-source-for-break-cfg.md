---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: source `for` break CFG

## Scope

A condition-bearing statement-form `for` may now end its body with a direct,
unvalued `break`, optionally after supported linear statements. SemanticIR
closes that body directly at the join. Because every true/body path exits the
loop, the source graph omits the unreachable step block and condition
backedge; the join has exactly the condition-false and body-break
predecessors.

The compiler still emits the unreachable step expression and backedge needed
by the established ExecBC layout, under temporary semantic-emission
suppression. The legacy break remains a forward jump to the label after both,
so executing it cannot evaluate the step.

Loop preflight and terminal-exit classification moved from the 993-line
general semantic-CFG module into the focused 90-line
`compiler_semantic_cfg_loop.c` module. The main unit is now 931 lines.

## Focused fixtures

`test_source_for_break_targets_join_cfg` compiles an assignment initializer,
linear condition, linear body prefix followed by `break`, linear step, and a
resolved call after the loop. It requires the exact eight-block source graph,
a body-to-join edge, one condition predecessor, two join predecessors, no
source step block, successful SemanticIR validation, and preservation of all
eight blocks through ExecIR construction.

The fixture also requires exactly one forward unconditional legacy jump and
verifies that its target follows the step's `GET_CONSTANT`/`SET_STACK`
sequence and negative backedge jump. This independently proves that legacy
`break` skips the step. A separate condition-bearing fixture keeps `break`
followed by reachable syntax on the persistent fallback path.

## TDD evidence

- The initial focused MSVC run passed the prior 59 cases and failed only the
  converted conditioned-break fixture because the conservative startup
  barrier still rejected direct `for` break.
- Allowing terminal break, omitting its unreachable semantic step/backedge,
  and suppressing semantic emission while retaining the legacy step made the
  focused suite pass 60/60.
- Adding the isolated nonterminal-break fallback and stronger legacy ordering
  assertions preserved the gate at 61/61.

## Validation evidence (2026-09-18)

- MSVC, WSL GCC, and WSL Clang each pass the complete pre-SemanticIR
  producer suite: 61/61.
- All three toolchains pass the adjacent SSA builder CFG, dominance,
  control-edge, fact-identity, place-eligibility, place-promotion, and value
  validation executables, plus the ownership receiver-guard performance
  smoke test.
- WSL GCC with ASan, UBSan, and leak detection passes the producer suite
  61/61 with no sanitizer diagnostics.
- The finally/abrupt regression suite passes 7/7. The resource regression
  suite remains at its established 19/20 baseline, with only the unrelated
  `test_resource_unique_moves_through_value_parameter_and_return` failure.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; `git diff --check` reports no patch errors.
- Independent review found no Critical, Important, or Minor issues in the
  loop classification, semantic topology, temporary termination-latch
  handling, legacy jump layout, boundary tests, or documentation.

## Boundary

This checkpoint accepts only a direct, unvalued terminal `break`, with an
optional supported linear prefix, in a condition-bearing linear statement
`for`. Missing conditions, valued or nonterminal exits, nonlinear components,
cleanup/finally transfers, and `foreach` remain conservative. It does not
claim the complete SSA 01.02 loop gate.
