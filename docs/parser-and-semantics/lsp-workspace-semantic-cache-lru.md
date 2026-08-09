# LSP Workspace Semantic Cache LRU

## Contract

Each `SZrLspContext` has a workspace-wide semantic cache-storage budget of
256MiB by default. The budget applies only to the exact storage reported by
`ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes`: an
`SZrAnalysisCache` plus the allocated capacity of its diagnostic and symbol
pointer arrays, recursively including a primary analyzer's scoped analyzer.

The LRU deliberately does not estimate AST, semantic-context, symbol-table,
diagnostic, text-block, or process allocator memory. Those objects remain
live when a cache entry is evicted and require separate peak-memory evidence.

## Ownership And Recency

- Every primary analyzer in the URI map and every historical semantic snapshot
  is scanned as one cache owner. A historical analyzer retains its immutable
  semantic state and AST even after its cache storage is released.
- Analyzer lookup, document analysis, historical-snapshot retrieval, and the
  completion scoped-analysis path refresh a monotonic access order.
- When exact cache storage exceeds the configured limit, the least recently
  accessed analyzer with nonzero cache storage is released. Ties are stable
  only within a scan and are not used as a semantic identity rule.
- `ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage` recursively frees
  only cache allocations. A later analysis recreates the cache storage without
  changing the primary analyzer address or historical snapshot ownership.

## Public Observability

`ZrLanguageServer_Lsp_SetSemanticCacheStorageLimit` applies a context-local
limit immediately. `ZrLanguageServer_Lsp_GetSemanticCacheStorageInfo` reports
the configured limit, current exact storage, high-water exact storage,
eviction count, and cumulative released exact storage. A limit of zero is a
strict no-cache-storage budget, not an unlimited sentinel.

The test configuration API exists to verify eviction deterministically. CI
must keep the 256MiB default unless an explicit configuration change is
reviewed; it must not silently relax the baseline.

## Boundaries

This feature does not measure process RSS or allocator peak memory, does not
make the LRU scan O(1), and does not constitute the complete L6 stdio/CLI
stress matrix. It only enforces the documented cache-storage subset with exact
accounting.
