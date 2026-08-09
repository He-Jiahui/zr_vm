---
plan_id: lsp-03-robustness
record_id: 2026-08-10-workspace-semantic-cache-lru
status: completed
completed_at: 2026-08-10 00:20 +08:00
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_cache_lru.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/acceptance/2026-08-10-lsp-l6-workspace-semantic-cache-lru.md
doc_type: milestone-detail
---

# Workspace Semantic Cache LRU

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 00:20 +08:00 | 已完成 | 每个 LSP context 以默认 256MiB exact cache-storage budget 扫描 primary、scoped 和历史 semantic analyzer，按 access order 释放 LRU cache storage，同时保持 AST、semantic context 与 analyzer identity。 |

## 已实现契约

- `SZrLspSemanticCacheLru` 只汇总已发布的 `SZrAnalysisCache` capacity
  storage，不估算 AST、semantic context、symbol table 或进程内存。
- primary URI analyzer 和两份历史 semantic snapshot 都是 LRU owner；其
  scoped cache 通过 primary 的递归计量与释放一并处理。
- `ZrLanguageServer_Lsp_SetSemanticCacheStorageLimit` 立即强制 context
  budget；`GetSemanticCacheStorageInfo` 输出 current/peak exact storage、
  eviction count 和 cumulative released storage。
- 访问 analyzer、完成 document analysis、读取历史 snapshot 和 completion
  scoped re-analysis 都更新 recency。淘汰只调用 cache-only release，不释放
  semantic state 或历史 AST。

## 验证

- RED：GCC interface test 因新 storage-info 类型和 limit API 缺失而编译
  失败，随后由最小公开合同实现恢复。
- GREEN：两个 URI 的测试将预算收紧为一个 analyzer 的实测 storage，先
  淘汰 oldest primary，再重建并淘汰另一个 URI；累计 released bytes 与
  两次 exact storage 相等。
- GREEN：单 URI v1/v2/v3 测试将预算收紧为 current storage，确认两份
  historical analyzer 的 cache storage 被淘汰而 semantic context 保留。
- GCC、Clang、MSVC 的 `zr_vm_language_server_lsp_interface_test` 与
  `zr_vm_language_server_local_semantic_query_test` 均真实 exit 0；MSVC
  使用全新 static build 目录完成编译和执行。

## 未完成边界

- 此 leaf 不测量 process RSS、allocator peak 或完整 semantic snapshot
  object graph 的内存。
- 此 leaf 不构成完整 L6 stdio/CLI/latency matrix；剩余最终矩阵和峰值内存
  报告继续单独验收。
