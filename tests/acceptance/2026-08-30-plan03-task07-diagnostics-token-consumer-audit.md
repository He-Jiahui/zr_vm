---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-30-plan03-task07-diagnostics-token-consumer-audit.md
doc_type: acceptance-record
---

# Plan 03 Task 7.33 Acceptance: Diagnostics and Semantic Token Consumer Audit

## Acceptance boundary

本记录只验收静态 consumer ownership，不把尚未释放的 Syntax05 semantic-token producer 边界
伪装成通过。结构化 diagnostics 路径必须读取 parser semantic query facts；semantic tokens
仍必须标记为 pending。

## Evidence

- `semantic_analyzer_query_diagnostics.c` 调用 `MaterializeDiagnostics()` 与 `Diagnostics()`，
  没有用 LSP message/name/type text 重建 semantic diagnostic。
- `semantic_analyzer_diagnostic_projection.c` 从 structured diagnostic 复制协议字段和 snapshot
  owned payload，未创建第二套语义事实。
- 项目级 unresolved import/member 路径仍依赖 project graph/metadata provider 的 external
  producer contract，canonical module/member identity 尚未释放；该部分保持 pending。
- `lsp_semantic_tokens.c` 仍是 Syntax05 exact-owned，存在 raw symbol-table/metadata-chain
  fallback；本阶段不修改，不计 GREEN。

## 状态

- 状态：审计完成，consumer migration 部分完成并受 ownership 阻塞。
- 完成时间：2026-08-30 05:22 +08:00。
- Plan 03 Task 7 与 Task 8 仍为进行中；最终三工具链矩阵、stdio/CLI smoke 与 producer
  release gate 尚未由本记录宣称通过。
