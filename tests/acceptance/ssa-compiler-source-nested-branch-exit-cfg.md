---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_branch_exit_cfg.inc
  - tests/parser/test_pre_semantic_ir_return_cfg.inc
  - tests/parser/test_pre_semantic_ir_throw_cfg.inc
status: partial
---

# SSA 01.02: nested conditional abrupt paths

## Scope

The conditional arm-flow preflight now composes the one-abrupt-arm rule
recursively for statement-form `if`. An inner conditional with one direct
`return` or unhandled `throw` arm and one fall-through arm remains a reachable
continuation for its enclosing branch. The inner abrupt block has no
successor, the inner join receives only the surviving sibling, and subsequent
linear statements in that enclosing arm execute before its edge to the outer
join.

An inner conditional whose two arms both terminate remains conservative. It
has no fall-through continuation under the current single-active-block
compiler bridge, so preflight rejects the enclosing source graph before it can
publish partial blocks or let a later call start a detached CFG.

## Focused fixtures

`test_nested_if_return_preserves_outer_fallthrough_cfg` places a return in the
then arm of an inner conditional, followed by an assignment on the surviving
inner path. It requires an 11-block graph, a zero-successor return block, one
inner-join predecessor, two outer-join predecessors, one later typed call, and
successful ExecIR construction.

`test_nested_if_throw_preserves_outer_fallthrough_cfg` nests the conditional
inside the outer else arm and places a throw in the inner else arm. This fixes
the symmetric compilation order: the inner then path reaches its one-input
join, the throw cannot reach either join, the outer then and surviving else
paths both reach the outer join, and the later call remains in the graph.

The existing conditional return/throw fallback fixtures now give the inner
conditional two abrupt arms. They require the persistent startup barrier and
legacy two-block graph, preserving the no-fall-through boundary after the
recursive case became supported.

## TDD evidence

- The initial MSVC run passed the existing 53 cases and failed both new nested
  fixtures because the outer preflight treated any nested abrupt path as
  unsupported.
- Recursive statement-`if` classification changed only the compositional
  case: at least one nested arm must fall through, while two terminating arms
  remain unsupported.
- The focused MSVC suite then passed 55/55 with exact inner and outer join
  predecessor counts and successful ExecIR construction.

## Validation evidence (2026-09-18)

- MSVC 19.44 passed the focused pre-execution SemanticIR suite 55/55.
- WSL GCC and Clang each passed the same focused suite 55/55.
- WSL GCC with AddressSanitizer, leak detection, and UndefinedBehaviorSanitizer
  passed the focused suite 55/55 without a sanitizer diagnostic.
- MSVC, WSL GCC, and WSL Clang each passed the seven adjacent SSA builder,
  dominance, control-edge, fact-identity, eligibility, promotion, and value
  validation executables, plus the receiver-guard performance test 1/1.
- The MSVC finally-abrupt regression suite passed 7/7. The broader resource
  suite retained its existing 19/20 baseline: the only failure remained
  `test_resource_unique_moves_through_value_parameter_and_return`.
- `python scripts/validate_wiki.py` passed for 116 Markdown files, 115
  manifest pages, and 644 local links; `git diff --check` reported no patch
  errors.
- Independent code review found no critical, important, or minor issue in
  recursive arm classification, local abrupt-mode composition, slot-state
  restoration, join topology, or conservative fallback behavior.

## Boundary

This checkpoint supports recursively nested statement-form conditionals only
while every nested level retains a fall-through path. A nested level whose two
arms terminate, expression-form nested `if`, non-linear payload, direct
transfer followed by reachable syntax, handled throw, or transfer crossing
`finally` or ownership cleanup remains conservative. It does not claim the
complete SSA 01.02 exit gate.
