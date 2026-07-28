---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-29-e2b1-paused-frame-canonical-binding-integration
status: completed
completed_at: 2026-07-29 00:00 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/plans/lsp/semantic-inference/status-and-output.md
tests:
  - tests/debug/test_debug_expression_diagnostics.c
doc_type: milestone-record
---

# LSP 04 E2b1 Paused-Frame Canonical Binding Integration

## Status and Outcome

| Completed | Status | Scope | Evidence |
|---|---|---|---|
| 2026-07-29 00:00 +08:00 | Completed | Debug semantic fact generation now parses one formal expression and registers each active paused-frame binding through its generation-validated canonical metadata. The binding keeps its compiled `SymbolId`, `TypeId`, and declaration range rather than receiving a temporary identity. | The fresh MSVC shared-library target built successfully and `zr_vm_debug_expression_diagnostics_test.exe` reported `34 Tests 0 Failures 0 Ignored`; the new case compares the inferred reference fact with the exact paused-frame binding identity. |

## Implementation

- `zr_debug_semantic_register_frame_variables` obtains the read-only evaluation
  context, enumerates only its active bindings, and fails closed when context,
  metadata, canonical identity, or the exact typed-local row is unavailable.
- Each verified binding is registered with
  `ZrParser_TypeEnvironment_RegisterCanonicalVariable`; no name, AST, text, or
  raw-slot fallback may synthesize a semantic identity.
- Debug semantic facts now use `ZrParser_ParseExpressionWithState` instead of
  constructing a synthetic expression statement.
- The internal semantic binder is exported only through `debug_internal.h` so
  the shared-library regression target can inspect the production inference
  path. It remains absent from the public `zr_vm_lib_debug/debug.h` API.

## Boundaries

- This completes the exact paused-frame binding path, not all of E2b. The
  separate `PlaceId` carrier and shared debug evaluate/watch/conditional
  breakpoint/REPL consumer query remain follow-up work.
- Stale, trimmed, missing, or mismatched frame metadata fails closed; the
  implementation does not fall back to a temporary semantic identity.
