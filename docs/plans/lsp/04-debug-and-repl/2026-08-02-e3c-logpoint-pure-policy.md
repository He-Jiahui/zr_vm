---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E3c logpoint pure policy
status: completed
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_logpoint.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_logpoint.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_evaluation_effect.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_logpoint.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_breakpoint_logpoint.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/cli-and-tooling/zr-debugger-v1-launch-workflow.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_breakpoint_logpoint_cases.h
doc_type: milestone-detail
---

# E3c Logpoint Pure Policy

## Contract

`zr_debug_breakpoint_logpoint_format` expands every logpoint
`{expression}` through `zr_debug_evaluate_expression_with_capabilities` with
the empty effect set and legacy compatibility disabled. It does not use the
compatibility evaluator.

Pure values are formatted into the console template. Evaluation failure retains
the established `<error:...>` interpolation text, so a rejected expression is
visible without becoming an execution capability. `debug.c` only emits the
completed template and cannot select another evaluator.

The policy rejects property getters, allocation, calls, native calls, value
mutation, and owner mutation. It does not infer a policy from template text,
member names, or AST shape.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 10:34 +08:00 | `completed` | Logpoint interpolation now uses the zero-capability formal evaluator and preserves existing console error interpolation for rejected effects. |

## Validation

- RED: the diagnostic runner could not include the missing logpoint policy API
  before the module was implemented.
- GREEN: `sum = {1 + 2}` produces `sum = 3`; an array literal interpolation
  returns the existing explicit-capability error inside `<error:...>`.
- GCC 11.4.0 and Clang 14.0.0 each built and ran the static
  `zr_vm_debug_expression_diagnostics_test` from the isolated native snapshot.
  Each runner reported `54 Tests / 0 Failures / 0 Ignored` with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 rebuilt the static
  `zr_vm_debug_expression_diagnostics_test` target and ran the test executable
  as the final command with process exit 0.
