---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b6d closure-capture formal consumer
status: completed
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug_evaluation_closure.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation_execute.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation_execute.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_canonical_binding_cases.h
  - tests/debug/test_debug_expression_diagnostics.c
doc_type: milestone-detail
---

# E2b6d Closure-Capture Formal Consumer

## Contract

- The formal evaluator dispatches `CLOSURE_CAPTURE` references to a dedicated
  reader. It requires a resolved fact with canonical SymbolId, TypeId,
  source-backed declaration range, nonzero origin token, runtime-root kind
  `NONE`, and invalid PlaceId.
- The reader reloads the paused evaluation context, requires the fact source to
  be the current function source, then asks the E2b6b API for exactly the fact
  `originIndex`. Index zero remains a valid first capture.
- It compares the returned capture SymbolId, TypeId, generation token, and
  whole declaration range against the fact before calling
  `ZrCore_Debug_EvaluationContext_ResolveClosureCapture`. The resolver repeats
  generation, activation, VM closure, and full binding validation.
- The consumer neither reads nor searches capture names, slots, AST nodes,
  display types, or text. Mismatched source/index/token/identity, stale or
  trimmed frames, and unavailable metadata remain unsupported formal execution.
- The capture value is read-only and remains subject to the existing formal
  effect policy. This stage adds no syntax fork, write path, or runtime-root
  substitution.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 06:25 +08:00 | `completed` | Enabled formal evaluation of a paused closure capture only through exact parser facts and the generation-checked resolver. |

## Validation

- RED: while the new canonical fact already existed, evaluating `seed` in the
  paused closure failed with `formal execution is unavailable for the authorized
  debug evaluation` because the consumer rejected `CLOSURE_CAPTURE`.
- GCC 11.4.0 passed the isolated command-graph compile, archive, link, and
  `zr_vm_debug_expression_diagnostics_test` execution at 52/52 with process
  exit 0.
- Clang 14.0.0 passed the corresponding isolated command-graph execution at
  52/52 with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 passed the static target and
  `zr_vm_debug_expression_diagnostics_test` at 52/52 with process exit 0.
