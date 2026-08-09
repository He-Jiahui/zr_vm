---
plan_id: lsp-03-robustness
record_id: 2026-08-09-two-historical-semantic-snapshots
status: completed
completed_at: 2026-08-09 23:37 +08:00
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_semantic_snapshot_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_snapshot_cache_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/acceptance/2026-08-09-lsp-l6-two-historical-semantic-snapshots.md
doc_type: milestone-detail
---

# Two Historical Semantic Snapshots

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-09 23:37 +08:00 | 已完成 | 每个 URI 保留最近两份完整历史 semantic analyzer snapshot，并在 scoped cache 借用旧 AST 时保持单一所有权和安全滚动淘汰。 |

## 已实现契约

- `ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot` 按新到旧返回精确
  `version`、`contentGeneration` 和只读历史 analyzer；结果不允许脱离
  LSP context 保存或释放。
- 真实内容变更将前一 primary analyzer state 转为独立历史 analyzer，
  primary analyzer 外层指针、metrics 和缓存开关保持稳定。
- 每个 URI 最多两份历史状态。第四次版本更新后历史是 v3/v2，v1 被
  释放；删除 URI 和释放 context 均先释放其历史状态。
- scoped query cache 只借用历史 AST，不重复拥有它。借用 AST 被淘汰时，
  scoped cache 先失效并计入失效指标，避免 stale hit 和 double free。

## 验证

- RED：MSVC 在新增 public query 尚未定义时产生未解析外部符号；rollover
  断言随后暴露了无效查询会清零输出结构的测试缺陷，修正为独立 overflow
  输出后验证真实 v3/v2 顺序。
- RED：MSVC local semantic-query 在第二次快照转移后暴露 scoped analyzer
  双重持有；修复为保留路径中历史 state 放弃 scoped pointer，并让 live
  analyzer 单独借用旧 AST。
- GREEN：MSVC、GCC、Clang 的
  `zr_vm_language_server_lsp_interface_test` 与
  `zr_vm_language_server_local_semantic_query_test` 均真实 exit 0。
  新接口 case 覆盖 v1..v4、稳定 primary identity、完整历史 semantic
  context、容量为二和 oldest rollover；local suite 覆盖跨多次 body edit
  的 scoped cache preservation 与淘汰失效。

## 未完成边界

- 此 leaf 不实现 workspace semantic cache 的 256MiB LRU、recency victim
  selection 或全局内存上限。
- 此 leaf 不报告进程峰值内存，也不构成完整 L6 stdio/CLI/性能矩阵证据。
