---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_receiver_completion_projection_cases.h
doc_type: module-detail
---

# LSP Receiver Completion Capability Boundary

`ZrLanguageServer_Lsp_TryCollectReceiverCompletions` no longer invokes either
of its recursive variable-prototype discovery and expression-type inference
fallbacks. Published receiver types still feed the existing member projectors.

## Canonical Receiver Projection

An ordinary receiver first uses the published reference type fact, then the
existing symbol and type-environment projections. Explicit type bindings and
imported native metadata retain their existing paths. Class and imported-type
completion remains available through the existing symbol and metadata
providers.

## No Completion-Local AST Reinference

The bounded receiver completion function no longer calls
`find_receiver_variable_prototype_recursive`,
`try_infer_receiver_type_text_from_ast`, or
`ZrParser_ExpressionType_Infer`. The missing native-local type regression now
returns no members through the exported completion query. The helper functions
remain available to other receiver consumers tracked by later Plan 03 slices.
Class constructor discovery, imported-type resolution, and name/type
environment lookup remain in this function and require their own canonical
identity migration. This slice does not establish the complete stale,
unresolved, source, binary, and native receiver matrix.

## Lifetime And Ownership

The analyzer, AST, and semantic facts are borrowed for the current snapshot.
Reference TypeIds are formatted only through that analyzer's semantic context.
The request owns the produced completion items and frees them after use; this
change adds no retained fact pointers or new semantic facts. Exactness and
snapshot validation in the existing query paths are unchanged.

## File Boundary

The production change removes more than 150 lines from the oversized support
file without introducing a new responsibility. Extraction of the remaining
receiver identity and member projection helpers is deferred until their
canonical query boundary is established. The new runtime regression is in a
separate receiver-completion test header; the existing runner only registers it.

## Validation

The source contract rejects those three AST reinference symbols inside
`TryCollectReceiverCompletions` and requires the reference-fact path. GCC,
Clang ASan/UBSan, and MSVC source-contract executions pass. GCC and a fresh
static MSVC build pass the structured receiver completion and new missing-type
regression; both complete interface runners retain exactly the eight failure
names present in the frozen GCC log.
Clang's complete runner stops earlier at the existing
`semantic_scope_facts.c:799` use-after-free. The new regression separately
passes under GDB with ASan access checks enabled and leak detection disabled
for ptrace. Broader runtime acceptance is tracked in the milestone record.
