---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
doc_type: audit-record
---

# Plan 03 Task 7.33: Diagnostics and Semantic Token Consumer Audit

## Scope

审计 Task 7 consumer migration 的 diagnostics 与 semantic-token 边界，确认当前 LSP 是否仍在
独立重建 parser/compiler 已发布的语义。审计遵守 Syntax05 exact ownership，不修改
`lsp_semantic_tokens.c` 或其 producer/metadata 依赖。

## Findings

- Semantic diagnostics 已由 `ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments()`、
  `ZrParser_SemanticFacts_ResolveControlFlowOwnership()`、
  `ZrParser_SemanticQuery_MaterializeDiagnostics()` 和
  `ZrParser_SemanticQuery_Diagnostics()` 取得结构化 facts。
- `ZrLanguageServer_Diagnostic_FromStructured()` 只复制 severity、primary range、code、cause、
  suggestion、descriptor、related information 与 machine fixes。既有诊断的合并只使用 exact
  source/range/code，未按 message、类型名、成员名或 AST 配对产生语义。
- 项目级 unresolved import/member diagnostics 仍是 project import graph 与 metadata provider
  的恢复路径。它的 module/member 聚合等待 producer/metadata ownership 提供 canonical external
  identity，当前不在 LSP 侧添加名称 fallback 或伪造 SymbolId。
- Semantic tokens 当前仍读取 analyzer symbol table，并保留 parameter lookup 与 metadata-chain
  fallback。该文件是 Syntax05 Task4 exact-owned，故本记录只声明阻塞，不声明 token migration
  GREEN。

## Verification

本次为静态 ownership/consumer audit；既有 source-contract 与 diagnostic golden parity 入口仍
是后续最终门禁的一部分。审计没有产生代码 RED/GREEN，也没有改动 marker 或测试基线。

## 状态与产出记录

- 完成时间：2026-08-30 05:22 +08:00。
- 状态：审计完成；structured diagnostics consumer 已确认符合 canonical projection，semantic
  tokens migration 与跨项目 unresolved-member identity 仍阻塞，Task 7/Task 8 未完成。
- 完成项目：诊断 query/projection ownership 核对、exact dedupe 边界核对、Syntax05 semantic-token
  ownership 记录、project metadata producer 边界记录。
- 后续项目：等待 Syntax05 producer/metadata paths 释放后迁移 semantic tokens；完成跨项目
  external identity 后再运行 Plan 03 全量门禁。
