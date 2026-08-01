---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b6b generation-checked paused-frame closure resolver
status: completed
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_closure.c
  - zr_vm_core/src/zr_vm_core/function_closure_identity.c
tests:
  - tests/debug/test_debug_introspection.c
doc_type: milestone-detail
---

# E2b6b Generation-Checked Paused-Frame Closure Resolver

## Contract

- `ZrCore_Debug_EvaluationContext_GetClosureCapture` accepts only an active,
  generation-validated VM frame. It requires the exact active VM closure and
  exact closure-cell count for the compiled function before it asks the E2b6a
  typed sidecar for a capture identity.
- A published binding contains capture index, canonical TypeRef, SymbolId,
  TypeId, whole declaration range, and the paused frame generation token. A
  missing cell, missing sidecar row, incomplete identity, or invalid capture
  index clears the whole binding and returns `METADATA_UNAVAILABLE`.
- `ZrCore_Debug_EvaluationContext_ResolveClosureCapture` clears its output
  before validation; it repeats activation, function, generation, and PC
  validation, requires the exact generation token, regenerates the canonical
  binding, compares every identity field, and only then snapshots that closure
  cell.
- No resolver path calls a legacy upvalue name API or recovers identity from a
  capture name, slot, AST, display type, or text. Stale, retired, reused, or
  changed-PC frames return `STALE_FRAME`.
- This stage does not publish parser `CLOSURE_CAPTURE` reference origin/token
  facts and does not enable a Debug formal-expression consumer. Those remain
  E2b6c and E2b6d work.

## 状态与产出记录

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 05:20 +08:00 | `completed` | Published a fail-closed paused-frame closure-capture binding and resolver with exact VM closure, E2b6a identity, frame-generation token, declaration range, output clearing, and stale-frame rejection. |

## Validation

- RED: the new introspection test first failed to compile because the public
  closure-capture binding and resolver APIs did not exist.
- GCC 11.4.0 passed the isolated command-graph compile, archive, link, and
  `zr_vm_debug_introspection_test` execution at 3/3 with process exit 0.
- Clang 14.0.0 passed the corresponding isolated command-graph execution at
  3/3 with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 passed the static target and
  `zr_vm_debug_introspection_test` at 3/3 with process exit 0.
