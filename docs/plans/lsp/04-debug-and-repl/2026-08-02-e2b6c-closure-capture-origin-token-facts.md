---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b6c closure-capture origin and token facts
status: completed
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug_evaluation_closure.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_branch_assignment_join.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_branch_refinement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_loop_assignment_scope.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_canonical_binding_cases.h
  - tests/debug/test_debug_expression_diagnostics.c
doc_type: milestone-detail
---

# E2b6c Closure-Capture Origin And Token Facts

## Contract

- `ZrParser_TypeEnvironment_RegisterClosureCapture` accepts only the complete
  identity returned by the generation-checked E2b6b resolver: canonical TypeRef,
  SymbolId, TypeId, declaration range, capture index, and nonzero token. It
  does not allocate replacement semantic identities or invent a local Place.
- `SZrTypeBinding` and `SZrSemanticReferenceFact` carry `originIndex`. For a
  closure capture it is the exact capture index, independent from `placeId`;
  index zero is valid and `placeId` remains invalid.
- Debug obtains identity by exact capture index before it consults the capture
  name. The name is only the surface binding key for a formal expression. A
  source declaration with that key remains authoritative, and any other
  duplicate binding fails closed as metadata unavailable.
- Identifier inference copies origin kind, runtime-root kind, token, index,
  SymbolId, TypeId, and declaration range into the canonical reference fact.
  Missing, stale, trimmed, incomplete, or duplicate identity therefore cannot
  become a synthetic local, name-derived fact, or runtime-root fact.
- This stage only publishes semantic facts. The separate E2b6d consumer now
  resolves a closure value only by consuming this fact and the resolver, with
  no name, slot, AST, display-type, or text fallback.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 06:13 +08:00 | `completed` | Published and exercised canonical closure-capture parser bindings and reference facts with exact origin index, token, IDs, range, and invalid PlaceId. |

## Validation

- RED: the new canonical identity assertion initially failed to compile because
  `originIndex` was not present in `SZrSemanticReferenceFact`.
- A stale GCC cache then exposed an old `semantic_facts.c` array element size;
  rebuilding that header consumer made the new field copy correctly. This was
  a cache ABI observation, not a product-code fallback or a relaxed assertion.
- GCC 11.4.0 passed the isolated command-graph compile, archive, link, and
  `zr_vm_debug_expression_diagnostics_test` execution at 52/52 with process
  exit 0.
- Clang 14.0.0 passed the corresponding isolated command-graph execution at
  52/52 with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 passed the static target and
  `zr_vm_debug_expression_diagnostics_test` at 52/52 with process exit 0.
