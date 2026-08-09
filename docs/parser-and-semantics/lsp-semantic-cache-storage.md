---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
plan_sources:
  - user: follow docs/plans/lsp and docs/plans/syntax, with syntax precedence on conflict
  - docs/plans/lsp/03-lsp-robustness-and-position.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/acceptance/2026-08-09-lsp-l6-semantic-cache-storage-accounting.md
doc_type: module-detail
---

# LSP Semantic Cache Storage

## Purpose

`SZrSemanticAnalyzer` keeps an `SZrAnalysisCache` for its main analysis and may own a scoped-query analyzer with a second cache. This module publishes a narrow accounting and release contract for those cache allocations. It gives a later workspace LRU a real storage unit instead of an estimate derived from source length or diagnostic count.

## Accounted Storage

`ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes` reports exactly the allocations owned by every reachable `SZrAnalysisCache`:

- `sizeof(SZrAnalysisCache)`;
- `cachedDiagnostics.capacity * sizeof(SZrDiagnostic *)`;
- `cachedSymbols.capacity * sizeof(SZrSymbol *)`;
- the same three allocations for the scoped-query analyzer, recursively.

Capacity, rather than current length, is used because it is the allocated pointer-array storage. The cached diagnostic and symbol pointers are non-owning, so their payloads are intentionally not charged to this API. Arithmetic saturates at `ZR_MAX_SIZE` before an overflow can wrap to a smaller apparent cache size.

The result is not a process-memory measurement. It excludes analyzer object state, ASTs, symbol tables, reference trackers, diagnostics, compiler state, native metadata, document text blocks, allocator overhead, and all other workspace memory.

## Release And Rehydration

`ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage` recursively frees the main and scoped `SZrAnalysisCache` blocks and their pointer arrays. It deliberately retains both analyzer objects and their semantic/AST state, so releasing the counted storage does not invalidate a scoped analyzer merely because its cache storage was reclaimed.

When caching is enabled, `ZrLanguageServer_SemanticAnalyzer_AnalyzeScope` recreates a missing main cache before it calculates cache hashes or stores cache entries. A later analysis therefore continues normally after release. Scoped cache recreation remains owned by its existing scoped-query path.

## Consumer Boundary

This API is a lower-level primitive. It does not select document victims, track recency, apply a workspace-wide 256MiB budget, retain two historical semantic snapshots, or publish a peak-memory report. Those L6 responsibilities require context-level ownership in the LSP interface and must consume this measured cache storage without substituting source-length estimates or name/text heuristics.

## Test Coverage

`test_semantic_analyzer.c` creates a primary and scoped analyzer, verifies that recursive storage is larger than primary storage, releases cache storage, verifies both analyzer identities remain valid while all counted bytes are zero, then analyzes a parsed source again and verifies primary cache rehydration. The focused executable passes with GCC, Clang, and MSVC.
