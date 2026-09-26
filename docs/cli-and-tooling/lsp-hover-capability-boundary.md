---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - .codex/plans/20260926-lsp-experience-repair.md
tests:
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/parser/test_semantic_declaration_binding_cases.h
doc_type: module-detail
---

# LSP Hover Capability Boundary

The public `Lsp_GetHover` entry point now uses canonical semantic and local
query projections before formatting protocol output. It does not invoke the
legacy semantic analyzer hover builder when those queries cannot produce a
result.

## Canonical Projection

Native receiver, external callable, imported metadata, local symbol, and local
expression hover paths are handled by their structured query providers. A
canonical hover copies the parser symbol identity, signature or canonical type
display, documentation fact, and request range into LSP-owned output.

A declaration can be resolved even when its exact type is unavailable. Such a
canonical symbol has a valid identity and invalid type identity; hover then
displays `cannot infer exact type`, and completion retains the same declaration
with an explicit unknown-type detail. An undeclared name still has no canonical
symbol and does not acquire one from a source-text search.

For a resolved symbol that has no richer query result, the existing content
snapshot markdown documentation path remains available. It consumes the
projected symbol and current document content without running another semantic
analysis.

Canonical call targets use the call fact's `SymbolId` to recover the declaration
metadata. This keeps method-call hover on the canonical member even when the
receiver is inferred through a local or imported type. Declaration-leading
comments are read from that canonical declaration and are included in both the
ordinary hover and `zr/richHover` documentation section; a failed local pointer
lookup must not discard the comment.

## No Analyzer Hover Fallback

`Lsp_GetHover` no longer calls `SemanticAnalyzer_GetHoverInfo`, which traversed
the AST and reconstructed declared or inferred types while servicing a request.
Unavailable or stale canonical facts therefore remain unavailable. The
metadata provider still has a separate source-hover integration boundary for
external declaration records; that path is not changed by this public-entry
point slice.

## Validation

The source contract bounds the public hover function and rejects the analyzer
hover call. Interface regressions cover canonical native receiver hover, native
construct completion/signature, canonical call hover and the explicit exact-type
failure message. Parser and analyzer identity regressions separately check that
an unavailable type retains the correct source declaration through lexical
shadowing and does not become a weak `object` type.
