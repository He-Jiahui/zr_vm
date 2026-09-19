---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_branch_exit_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
status: partial
---

# SSA 01.02: total conditional abrupt paths

## Scope

Statement-form conditionals whose two arms both terminate now publish their
source CFG without fabricating a reachable join. Each `return` or unhandled
`throw` block is a zero-successor sink. A direct total conditional sets the
function termination latch so unreachable later syntax cannot start a detached
graph. Because the current CFG schema retains one required `exitBlockId`, the
last source-order abrupt sink is its representative exit; this does not add an
edge between terminal blocks.

A nested total conditional is classified as a terminating arm. If its outer
sibling falls through, only that sibling reaches the outer join and subsequent
source, including a resolved call, stays in the same graph. This removes the
inner join without treating the whole enclosing conditional as terminated.

## Focused fixtures

`test_source_if_two_abrupt_arms_terminate_cfg` covers a direct return/throw
pair. It requires three blocks, one value return, one throw, no later call,
two zero-successor sinks, a set function termination latch, and successful
ExecIR construction.

`test_nested_two_return_arms_preserve_outer_fallthrough_cfg` and
`test_nested_two_throw_arms_preserve_outer_fallthrough_cfg` place a total
conditional in one arm of a conditional with an implicit fall-through sibling.
Each requires no inner join, one single-predecessor outer join, a reachable
later typed call, exact explicit and synthetic terminator counts, and
successful ExecIR construction. The throw fixture distinguishes its two
source throw blocks from the later call's zero-instruction exception sink.

`test_nested_total_exit_with_trailing_statement_remains_conservative` fixes
the current boundary: source syntax after a no-fall-through nested conditional
keeps the persistent startup barrier and legacy two-block graph.

## TDD evidence

- The initial MSVC run passed the prior 52 cases and failed all three converted
  positive fixtures because the arm-flow preflight still rejected two
  terminating arms.
- The first implementation reached the new lowering and exposed a misplaced
  join guard that skipped the second arm. Moving that guard to final join entry
  preserved sibling compilation and made the total shape structurally valid.
- The focused MSVC suite passed 55/55 after distinguishing the existing invoke
  exception sink, then 56/56 after adding the trailing-syntax fallback fixture.
- Review then identified that declared-child returns intentionally do not close
  the disposable SemanticIR blocks. Strengthening the child isolation fixture
  to two returning arms reproduced `Failed to record if true branch` as the
  only MSVC failure. Limiting join omission to the published entry body kept a
  valid internal child join; the focused suite returned to 56/56.

## Validation evidence (2026-09-18)

- MSVC 19.44 passed the focused pre-execution SemanticIR suite 56/56.
- WSL GCC and Clang each passed the same focused suite 56/56.
- WSL GCC with AddressSanitizer, leak detection, and UndefinedBehaviorSanitizer
  passed the focused suite 56/56 without a sanitizer diagnostic.
- MSVC, WSL GCC, and WSL Clang each passed the seven adjacent SSA builder,
  dominance, control-edge, fact-identity, eligibility, promotion, and value
  validation executables, plus the receiver-guard performance test 1/1.
- The MSVC finally-abrupt regression suite passed 7/7. The broader resource
  suite retained its existing 19/20 baseline: the only failure remained
  `test_resource_unique_moves_through_value_parameter_and_return`.
- `python scripts/validate_wiki.py` passed for 116 Markdown files, 115
  manifest pages, and 644 local links; `git diff --check` reported no patch
  errors.
- Independent review found the child total-return invalid-join regression
  described above. After its test and fix, re-review found no remaining
  critical, important, or minor issue.

## Boundary

This checkpoint covers statement-form `if` only. Non-linear abrupt payloads,
reachable syntax after an abrupt or total nested conditional, expression-form
nested `if`, handled exception dispatch, and transfers across `finally` or
ownership cleanup remain conservative. It does not claim the complete SSA
01.02 exit gate.
