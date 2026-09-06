---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
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

For a resolved symbol that has no richer query result, the existing content
snapshot markdown documentation path remains available. It consumes the
projected symbol and current document content without running another semantic
analysis.

## No Analyzer Hover Fallback

`Lsp_GetHover` no longer calls `SemanticAnalyzer_GetHoverInfo`, which traversed
the AST and reconstructed declared or inferred types while servicing a request.
Unavailable or stale canonical facts therefore remain unavailable. The
metadata provider still has a separate source-hover integration boundary for
external declaration records; that path is not changed by this public-entry
point slice.

## Validation

The source contract bounds the public hover function and rejects the analyzer
hover call. GCC source-contract execution passes. The GCC interface target
keeps canonical native receiver hover, native construct completion/signature,
and canonical call hover cases passing; the existing container-matrix hover
failure remains outside this slice.
