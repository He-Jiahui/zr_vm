---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/04-editor-feature-correctness.md
tests:
  - tests/language_server/stdio_navigation_capabilities_smoke.js
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/stdio_workspace_folders_smoke.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: module-detail
---

# Navigation Capability Boundary

## Distinct Semantic Targets

Declaration, definition, type definition and implementation are distinct queries.
An alias to another operation is insufficient to publish a protocol capability.
The current native declaration/typeDefinition adapters are withdrawn from
initialize, the registry and dispatch. Their explicit methods return JSON-RPC
`-32601 Method not found`. Browser declaration/typeDefinition remain absent.
The internal core aliases still require replacement in Plan 04; they do not
authorize publishing those capabilities again.

Definition and implementation remain native capabilities. Definition projects
canonical query results. Implementation resolves a local canonical symbol, then
queries `ImplementationsOf(SymbolId)` within the owning module. The query result
is borrowed from its semantic snapshot; locations copy the range while their
URI pointers remain borrowed from the context/analyzer storage through synchronous
JSON serialization. Neither adapter may infer a target from a
matching name or a displayed type. Missing or unsupported relation facts cannot
be replaced by an arbitrary same-name declaration.

The focused protocol fixture proves a precise Device definition token and the
complete Device/Sensor implementation set for Readable, using full class ranges.
An unrelated class with a same-name method is excluded. Existing local parity
tests also detach the analyzer symbol table while querying implementations.
These tests cover local canonical implementation, not all external/binary/native
provider origins or definition-flow cases.

## Canonical Symbol Projection

The public position-symbol bridge keeps the exact property contract as its first
provider and delegates ordinary symbols to parser `SymbolAt`. A failed canonical
lookup, missing semantic context, or invalid `SymbolId` returns no symbol. The
bridge does not walk the LSP symbol table's scopes or retained declaration/reference
ranges to reconstruct identity. The returned symbol is a presentation projection
looked up by stable `SymbolId` and is borrowed only for the synchronous request.

`test_lsp_symbol_projection_cases.h` covers both missing semantic context and a
mismatched projected identity. The focused record is
[Plan 03 Task 7.63](../plans/lsp/optimize/2026-09-07-plan03-task07-canonical-symbol-projection.md).

## Existing Workspace Contract

Native `workspace/didChangeWorkspaceFolders` updates the actual workspace root
set. Its capability is retained and verified by the dedicated folder smoke,
including multi-root removal and open-overlay retention. The historical request
to disable ignored folder notifications no longer describes this implementation.
Full browser workspace parity remains a separate gate.

## Remaining Semantic Gate

At the ce04018c baseline, the existing reaching-definition C tests fail: the
simple and branch-write fixtures return zero definition locations and the
missing-source fixture cannot prepare its expected query. The matching stdio
read also returns an empty array. Those are recorded defects in Plan 03/04;
withdrawing the two aliases does not repair or waive them.

This capability change removes obsolete declarations and wrappers. The large
existing stdio smoke remains a protocol orchestration file; its former alias
array-shape assertion is removed and new behavior lives in the focused fixture.
Broader smoke responsibility extraction remains Plan 06 work.
