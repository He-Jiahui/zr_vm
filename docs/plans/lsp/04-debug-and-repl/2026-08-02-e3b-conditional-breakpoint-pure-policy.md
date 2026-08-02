---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E3b conditional breakpoint pure policy
status: completed
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_condition.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_condition.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_evaluation_effect.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_condition.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_condition.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_breakpoint_condition_cases.h
doc_type: milestone-detail
---

# E3b Conditional Breakpoint Pure Policy

## Contract

Conditional breakpoint evaluation is formal-only. For a non-empty condition,
`zr_debug_breakpoint_condition_evaluate` calls
`zr_debug_evaluate_expression_with_capabilities` with the empty effect set and
with legacy compatibility disabled. It never routes through the compatibility
wrapper.

The policy rejects property getters, allocation, regular calls, native calls,
value mutation, and owner mutation. Failed evaluation leaves the condition
unsatisfied and returns the existing structured error to the breakpoint
orchestrator, which keeps its existing stderr output behavior. Empty conditions
remain unconditional breakpoints and do not enter the evaluator.

Truthiness is evaluated only from the resulting formal value. The policy does
not derive behavior from condition spelling, member names, AST shapes, or text
heuristics.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 10:22 +08:00 | `completed` | Conditional breakpoints now evaluate only through the zero-capability formal path, with compatibility disabled and unchanged unconditional-breakpoint semantics. |

## Validation

- RED: the new conditional-breakpoint test could not include the missing
  condition policy API before the implementation existed.
- GREEN: the test accepts pure `1 < 2` and rejects an array literal because it
  requires explicit allocation capability; the error contains the existing
  explicit-capability diagnostic.
- GCC 11.4.0 and Clang 14.0.0 each built and ran the static
  `zr_vm_debug_expression_diagnostics_test` from the isolated native snapshot.
  Each runner reported `53 Tests / 0 Failures / 0 Ignored` with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 rebuilt the static
  `zr_vm_debug_expression_diagnostics_test` target and ran the test executable
  as the final command with process exit 0.
