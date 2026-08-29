---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/acceptance/2026-08-30-plan03-task07-unresolved-value-exactness.md
doc_type: milestone-detail
---

# Plan 03 Task 7.28 Unresolved Value Exactness

## 目标

- parser 已发布 reference fact 时，该 fact 的 resolved identity 是 source value/symbol query 的唯一
  有效性判据。
- non-TYPE reference 为 unresolved 或 SymbolId invalid 时，LSP 必须 fail closed，不得继续用
  `FindSymbolAtUsageOrDefinition` 把同名 LSP symbol 提升为 semantic target。
- import chain、binary/native metadata adapter 与当前 unresolved TYPE producer 边界保持不变，不把本片
  扩展为名称兼容或跨层 producer 修复。

## 执行

1. RED 创建局部变量 source fixture，确认 parser reference fact 和 LSP symbol table 同时存在，再临时把
   canonical reference 标记为 unresolved。旧实现仍返回 local target，parity 新增第 14 项唯一失败，
   真实 exit 1。
2. GREEN 在 `SymbolAt` 前读取同一 range 的 `FactsAt`。`SymbolAt` 失败且 non-TYPE reference 已明确
   unresolved/invalid 时立即返回 false，不进入 local symbol fallback。
3. A/B 审计曾尝试让所有缺失 canonical identity 都 fail closed；fixed interface 新增 closed-generic
   type marker。定位证明该 type annotation 已有 unresolved TYPE fact，其 producer 仍在 Syntax05 当前
   ownership 内，因此撤销过宽实现，不把新增 marker 计作 GREEN。
4. 最终保留 TYPE 边界原行为；本片只关闭有完整 RED/GREEN 证据的 value/symbol exactness 缺口。

## 状态与产出记录

- 完成时间：2026-08-30 04:09 +08:00。
- 状态：已完成。
- 完成项目：non-TYPE unresolved reference RED/GREEN、canonical `FactsAt` exactness fence、
  parent/overlay interface A/B、三工具链 parity/source-contract/interface/project marker 审计、模块合同更新。
- 后续边界：closed-generic type annotation 的 unresolved TYPE fact 仍等待 canonical producer 修复；
  Task 7.25 imported source declaration identity 与 source-local rename 仍受 Syntax05 ownership 阻塞。
