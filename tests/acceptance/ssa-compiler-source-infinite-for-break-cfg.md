---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: conditionless `for` break CFG

## Scope

A statement-form `for` with no condition may now publish a source-owned CFG
when its body ends directly in an unvalued `break`, optionally after supported
linear statements. The loop header has one unconditional normal edge to the
body; the terminal break closes the body at the join. There is no false edge,
semantic step block, or semantic backedge because every body path exits.

The established ExecBC layout still emits the unreachable unconditional
backedge. The forward break target follows that backedge, so executing the
break cannot repeat the loop.

## Focused fixtures

`test_source_infinite_for_break_targets_join_cfg` starts CFG construction at
`for (;;) { break; }`, then compiles a resolved call after the loop. It
requires the exact header-to-body-to-join topology, one predecessor at each
region, successful SemanticIR validation, and preservation of all eight
blocks through ExecIR construction. It also requires exactly one forward
unconditional legacy jump and a negative backedge immediately before its
target.

`test_source_infinite_for_break_preserves_active_call_cfg` places resolved
calls before and after the loop and requires both to remain in the same active
11-block graph. `test_infinite_for_continue_keeps_fallback` proves that a
conditionless loop without a terminal break is not admitted by this slice and
continues to block detached suffix CFG startup.
`test_infinite_for_continue_abandons_active_call_cfg` starts a graph with a
resolved prefix call, then requires the unsupported loop to abandon that
partial graph, retain only the prefix's straight-line facts, and block the
trailing call from starting a detached suffix.

## TDD evidence

- The initial focused MSVC run passed all prior 61 cases and the new
  conditionless-continue boundary, while both converted conditionless-break
  positive fixtures failed at the conservative startup barrier.
- Admitting only conditionless loops classified as terminal-break loops and
  recording the unconditional header-to-body edge made the focused suite pass
  62/62.
- Independent review found that converting the previous active-prefix
  fallback fixture had left the unsupported-loop abandonment path implicit.
  Adding an explicit conditionless-continue active-prefix regression preserved
  the gate at 63/63.

## Validation evidence (2026-09-18)

- MSVC, WSL GCC, and WSL Clang each pass the complete pre-SemanticIR
  producer suite: 63/63.
- All three toolchains pass the adjacent SSA builder CFG, dominance,
  control-edge, fact-identity, place-eligibility, place-promotion, and value
  validation executables, plus the ownership receiver-guard performance
  smoke test.
- WSL GCC with ASan, UBSan, and leak detection passes the producer suite
  63/63 with no sanitizer diagnostics.
- The finally/abrupt regression suite passes 7/7. The resource regression
  suite remains at its established 19/20 baseline, with only the unrelated
  `test_resource_unique_moves_through_value_parameter_and_return` failure.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; `git diff --check` reports no patch errors.
- Independent review initially found the active-prefix fallback coverage gap
  described above. After the regression fixture was added, re-review found no
  remaining Critical, Important, or Minor issues.

## Boundary

This checkpoint does not accept conditionless falling-through or
continue-ended loops, valued/nonterminal exits, nonlinear loop components,
cleanup/finally transfers, or `foreach`. It does not claim the complete SSA
01.02 loop gate.
