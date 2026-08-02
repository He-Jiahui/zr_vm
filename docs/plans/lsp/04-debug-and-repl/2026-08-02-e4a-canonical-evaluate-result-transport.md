---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E4a canonical evaluate result transport
status: completed
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
implementation_files:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_evaluate_result_transport_cases.h
doc_type: milestone-detail
---

# E4a Canonical Evaluate Result Transport

## Contract

`ZrDebugEvaluateResult` now carries the paused `state_id`,
`has_canonical_type`, and `canonical_type_id`. Formal evaluation asks
`ZrParser_SemanticQuery_CanonicalTypeAt` for the exact root expression range
inside the same prepared compiler context. It publishes that TypeId only when
the query succeeds.

`state_id` is the same stop-state generation used to validate value handles.
The evaluate protocol serializes it as `stateId`, always serializes
`hasCanonicalType`, and serializes `canonicalTypeId` only when the formal
identity is valid. If the caller explicitly permits a legacy compatibility
fallback, the internal evaluator clears the TypeId so the result fails closed
instead of deriving identity from display text or a runtime type tag.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 10:50 +08:00 | `completed` | Formal evaluate results and protocol JSON now publish the exact canonical TypeId and stop-state lifetime token when available. |

## Validation

- RED: the result transport test did not compile because
  `ZrDebugEvaluateResult` had no lifecycle or canonical TypeId fields.
- GREEN: a paused formal evaluation of `1 + 2` returns `state_id=73`, a
  nonzero canonical TypeId, and protocol JSON with `stateId`,
  `hasCanonicalType: true`, and `canonicalTypeId`.
- GCC 11.4.0 and Clang 14.0.0 each built and ran the static
  `zr_vm_debug_expression_diagnostics_test` from the isolated native snapshot.
  Each runner reported `55 Tests / 0 Failures / 0 Ignored` with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 rebuilt the static
  `zr_vm_debug_expression_diagnostics_test` target and ran the test executable
  as the final command: `55 Tests / 0 Failures / 0 Ignored`, process exit 0.
