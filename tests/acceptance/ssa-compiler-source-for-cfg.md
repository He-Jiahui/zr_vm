---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_for.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc
status: partial
---

# SSA 01.02: source linear `for` CFG

## Scope

The compiler-owned source CFG now lowers a condition-bearing statement-form
`for` with supported linear components. After the initializer, the source
prefix jumps to a dedicated condition block. Its ordered true and false edges
select the body and join, the falling-through body jumps to a dedicated step
block, and the step closes the normal backedge to the condition.

The join restores the semantic slot snapshot taken after initialization.
Values created only by the condition, body, or step therefore cannot leak to
the loop's false continuation. Legacy ExecBC labels and jumps remain intact;
the source graph is not reconstructed from their instruction offsets.

The `for` compiler was extracted from the general 1,514-line statement-flow
unit into a focused source file. The remaining statement-flow unit is 1,414
lines and retains `foreach` and the other established statement paths.

## Focused fixture

`test_source_for_emits_typed_condition_body_step_cfg` compiles a typed local,
a raw linear assignment initializer, a linear boolean condition, one body
assignment, a linear step, and a resolved call after the loop. It requires the
exact entry-to-condition,
condition-to-body/join, body-to-step, and step-to-condition topology. It also
requires two predecessors on the condition, one predecessor on the join, the
later typed call, successful SemanticIR validation, and all nine source blocks
preserved by ExecIR construction.

The existing unmodeled-`for` fixtures continue to require persistent fallback
for terminal loop exits and verify both blocking later CFG startup and
abandoning an already active partial graph. The child-loop fixture continues
to require isolation from the entry body's published graph.

## TDD evidence

- The initial focused MSVC run passed the prior 56 cases and failed only the
  new fixture: the legacy fallback produced five blocks instead of the
  expected nine-block source graph.
- Adding the bounded `for` preflight and explicit condition/body/step/join
  topology made the focused suite pass 57/57 without weakening the existing
  fallback fixtures.
- Moving the `for` compiler into its own translation unit preserved the same
  57/57 result after CMake discovered and compiled the new source.
- Independent review identified that the first preflight accepted a variable
  declaration initializer but rejected the ordinary raw assignment form used
  with a predeclared counter. Converting the fixture reproduced the persistent
  startup barrier as the only failure. Accepting either a falling-through
  statement/declaration or a raw linear expression restored 57/57.

## Validation evidence (2026-09-18)

- MSVC 19.44, WSL GCC 11.4, and WSL Clang 14 each passed the focused
  pre-execution SemanticIR suite 57/57.
- All three toolchains passed the seven adjacent SSA builder, dominance,
  control-edge, fact-identity, eligibility, promotion, and value-validation
  executables, plus the receiver-guard performance smoke 1/1.
- WSL GCC with AddressSanitizer leak detection and UndefinedBehaviorSanitizer,
  both configured to halt on the first error, passed the focused suite 57/57
  without a sanitizer diagnostic.
- The MSVC finally-abrupt suite passed 7/7. The broader resource suite retained
  its existing 19/20 baseline; the only failure remained
  `test_resource_unique_moves_through_value_parameter_and_return`.
- `python scripts/validate_wiki.py` passed for 116 Markdown files, 115
  manifest pages, and 644 local links; `git diff --check` reported no patch
  errors.
- Independent review found the raw assignment-initializer gap captured in the
  TDD evidence above. After the fixture and preflight fix, re-review found no
  remaining Critical, Important, or Minor issue.

## Boundary

This checkpoint accepts only a statement `for` with a non-null linear
condition, an optional falling-through initializer, an optional linear step,
and a falling-through body. Infinite loops, `break`/`continue`, nonlinear
components, cleanup/finally transfers, and `foreach` remain conservative. It
does not claim the complete SSA 01.02 loop gate.
