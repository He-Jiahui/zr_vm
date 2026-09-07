---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/language_server/test_lsp_canonical_completion_cases.h
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_type_use.c
doc_type: module-detail
---

# LSP Completion Capability Boundary

`ZrLanguageServer_LspSemanticQuery_CollectCompletionItems` projects completion
items from the current semantic snapshot and existing metadata providers. A
completion request does not create a second analyzer or publish new semantic
facts.

## Canonical Projection

Lexical completion uses parser `VisibleSymbols` through
`lsp_canonical_completion.c`. Receiver and imported-module completion may use
already-published canonical expression/reference facts and structured metadata.
The completion projector copies labels, details, and documentation into
request-owned items before the snapshot is released.

At a resolved type-use position, the projector queries `SymbolAt` and compares
its SymbolId with each visible candidate in the same snapshot. A matching
candidate retains its declaration detail and appends the formatted instantiated
TypeId when it differs from the declaration type. This carries const-generic
normalization through completion and resolve without matching display names.
The [type-use publication contract](../parser-and-semantics/semantic-type-use-publication.md)
defines the producer, unavailable-identity behavior, range and lifetime rules.

Explicit unresolved type-reference facts are retained in parser-published
callable signatures as `cannot infer exact type`. Completion and hover consume
the same signature for matching callable identities; the
[callable display contract](../parser-and-semantics/callable-display-resolution.md)
describes publication timing and the canonical-only contract boundary.

## No Request-Time Reanalysis

When the current snapshot cannot supply receiver or lexical completions, the
request returns the available result, including an empty result. It does not
create a scoped query analyzer, call `Analyze` or `AnalyzeScope`, locate an
analysis root, or re-run semantic production against the AST. Missing or
approximate receiver facts therefore fail closed instead of being recovered by
syntax/name/type-text reconstruction.

## Validation

The source contract bounds the completion consumer and rejects scoped analyzer
creation, request-time analysis, and the old fallback variable. Existing native
construct completion coverage keeps descriptor fields available with an exact
receiver fact and returns no members after that fact is made unavailable. GCC
source-contract and interface targets were rebuilt and run; the interface
runner retains its pre-existing container-matrix hover failure.
