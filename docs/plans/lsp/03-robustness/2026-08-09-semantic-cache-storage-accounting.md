---
plan_id: lsp-03-robustness
record_id: 2026-08-09-semantic-cache-storage-accounting
status: completed
completed_at: 2026-08-09 22:31 +08:00
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/acceptance/2026-08-09-lsp-l6-semantic-cache-storage-accounting.md
doc_type: milestone-detail
---

# Semantic Cache Storage Accounting

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-09 22:31 +08:00 | 已完成 | 发布语义 cache 的精确 capacity-storage 计量、递归释放和按需重新初始化支持。 |

## 已实现契约

- `ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes` 精确计量 primary 与 scoped analyzer 的 `SZrAnalysisCache` 结构和两个 pointer-array capacity；未拥有的 diagnostic/symbol payload 不计入，算术溢出饱和为 `ZR_MAX_SIZE`。
- `ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage` 递归释放 primary/scoped 的 cache 结构和数组，保留 analyzer、scoped analyzer 与既有 semantic/AST state，不把未计量状态伪称为 cache storage。
- 缓存开启且 cache 已释放时，`AnalyzeScope` 会在 hash/cache 路径前重新建立 primary cache；后续分析不会因释放而失效。

## 验证

- RED：MSVC 在收紧 scoped-state 保留断言后失败，证明旧实现销毁了整个 scoped analyzer。
- GREEN：MSVC、GCC、Clang 的 `zr_vm_language_server_semantic_analyzer_test` 均真实 exit 0。新 case 覆盖 primary/scoped 递归计量、释放后零字节、scoped identity 保留和后续分析重新分配。

## 未完成边界

- 此记录不是 workspace LRU，也没有应用默认 256MiB semantic cache 上限。
- 此记录不保留、淘汰或查询两份历史 semantic snapshot；当前 two-history 契约只覆盖 text block。
- 此记录不报告 semantic analyzer、workspace 或进程的峰值内存，且不构成 L6 完成证据。
