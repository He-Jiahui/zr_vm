---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
plan_sources:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/acceptance/2026-08-09-lsp-l6-two-historical-semantic-snapshots.md
doc_type: module-detail
---

# LSP Historical Semantic Snapshots

## Purpose

An LSP document retains the current analyzer plus at most two earlier complete
semantic analyzer states for the same URI. The retained state includes the AST,
semantic context, symbol table, references, diagnostics, compiler state, and
semantic cache owned by that analyzer. This lets a later consumer inspect a
consistent historical version without reconstructing semantics from source text.

## Public Contract

`ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot` returns snapshots in
newest-to-oldest order. A successful result contains the captured document
`version`, its text `contentGeneration`, and a read-only analyzer pointer. The
result belongs to the LSP context: callers must not free it, retain it after the
context is destroyed, or mutate its analyzer.

Only real successful document changes capture a semantic snapshot. The current
analyzer object remains the URI's stable analyzer identity, so existing metrics
and callers that cache the primary pointer stay valid.

## Request Dependency Fence

The current request snapshot captures a canonical primary URI and records its
resolved project imports as canonical dependency URIs. Dependency discovery
recurses only through a project-index record and its parsed import bindings; it
does not infer dependencies from module names or source text. The primary URI
and every discovered dependency form one visited set, so a cyclic import graph
cannot retain the same document twice or recurse indefinitely.

`Validate` compares each captured document generation with the live file
version. An update to an unrelated URI leaves a request snapshot valid, while
an update to a direct or transitive imported document invalidates the primary
snapshot and makes the request publication fence report `ContentModified`.
The same identity is consumed by diagnostics, semantic tokens, and workspace
edits; those consumers do not construct a separate dependency hash.

## Ownership And Rollover

The snapshot cache owns each historical analyzer, which in turn owns its
retained AST. It keeps at most two entries per URI. Capturing a third previous
version removes the oldest entry before the new one becomes visible. Removing a
URI or freeing its context releases its historical entries before its live
analyzer is freed.

A preserved scoped-query analyzer may read an older AST after an unaffected
body edit. In that case it records a borrowed AST rather than taking a second
AST owner. When the owning historical snapshot is evicted, the live analyzer
invalidates that scoped cache before releasing the historical AST and records a
scoped-cache invalidation. This prevents both stale cache hits and double
release across repeated edits.

## Boundaries

This feature bounds semantic snapshot count per URI. It does not implement the
workspace-wide 256MiB semantic-cache LRU, cache victim recency, a process peak
memory report, or the full L6 stdio stress matrix. The later LRU must consume
the exact cache-storage accounting API rather than estimate AST or analyzer
memory from source length.
