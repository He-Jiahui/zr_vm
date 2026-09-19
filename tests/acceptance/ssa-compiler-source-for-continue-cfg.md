---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: source `for` continue CFG

## Scope

A condition-bearing statement-form `for` may now end its body with a direct,
unvalued `continue`, optionally after supported linear statements. SemanticIR
closes that body at the step block, then preserves the step-to-condition
backedge. The join remains reachable only from the condition's false edge.

The legacy ExecBC compiler now maintains separate condition and `continue`
labels. A `continue` resolves immediately before the step expression, while
the step's backedge targets the earlier condition label. This preserves the
language-level requirement that a `for` continue executes the step before the
next condition evaluation.

## Focused fixture

`test_source_for_continue_targets_step_cfg` compiles an assignment
initializer, linear condition, body assignment followed by `continue`, linear
step, and resolved call after the loop. It requires the exact nine-block
source graph, a body-to-step edge, a step-to-condition edge, one predecessor
on the step, and only the condition-false predecessor on the join. It also
requires successful SemanticIR validation and preservation of all nine blocks
through ExecIR construction.

The fixture additionally requires exactly one forward unconditional legacy
jump, verifies that its target starts the step's `GET_CONSTANT`/`SET_STACK`
sequence, and verifies the immediately following jump is a backedge to an
earlier label. Resolving `continue` at the condition would instead produce an
already-resolved backward jump, so these assertions guard the ExecBC
step-before-condition ordering independently of the source CFG.

## TDD evidence

- The initial focused MSVC run passed the prior 57 cases and failed only the
  new fixture because the conservative startup barrier still rejected the
  terminal `continue` body.
- Admitting only an unvalued terminal `continue`, closing its semantic edge at
  the step, and splitting the legacy condition and `continue` labels made the
  focused suite pass 58/58.
- The shared loop-body preflight remains parameterized so established `while`
  support still admits both direct `break` and `continue`, while this `for`
  slice admits only `continue`.
- Isolated condition-bearing fixtures require both direct `break` and a
  `continue` followed by reachable syntax to retain the persistent startup
  barrier and prevent a later call from publishing a detached source graph.

## Validation evidence (2026-09-18)

- MSVC 19.44, WSL GCC 11.4, and WSL Clang 14 each passed the focused
  pre-execution SemanticIR suite 60/60, including the positive continue case
  and both isolated condition-bearing fallback boundaries.
- All three toolchains passed the seven adjacent SSA builder, dominance,
  control-edge, fact-identity, eligibility, promotion, and value-validation
  executables, plus the receiver-guard performance smoke 1/1.
- WSL GCC with AddressSanitizer leak detection and UndefinedBehaviorSanitizer,
  both configured to halt on the first error, passed the focused suite 60/60
  without a sanitizer diagnostic.
- The MSVC finally-abrupt suite passed 7/7. The broader resource suite retained
  its existing 19/20 baseline; the only failure remained
  `test_resource_unique_moves_through_value_parameter_and_return`.
- `python scripts/validate_wiki.py` passed for 116 Markdown files, 115
  manifest pages, and 644 local links; `git diff --check` reported no patch
  errors.
- Independent review first identified the weak legacy-jump assertion and the
  masked fallback boundaries captured above. After tightening those tests,
  re-review found no remaining Critical, Important, or Minor issue.

## Boundary

This checkpoint accepts only a direct, unvalued terminal `continue`, with an
optional supported linear prefix, in a condition-bearing linear statement
`for`. Missing conditions, `break`, valued or nonterminal `continue`, nonlinear
components, cleanup/finally transfers, and `foreach` remain conservative. It
does not claim the complete SSA 01.02 loop gate.
