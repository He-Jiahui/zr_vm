---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.51: Symbol Reference Wrapper Removal

## Goal

删除 local-reference consumer 从 `SZrSymbol` 转换到 SymbolId 的零调用公开 wrapper，继续收敛
reference tracker 迁移后遗留的 LSP symbol surface。保留活跃 canonical relation projector、
snapshot source binding、cancellation 和 location 去重行为。

## Contract

- `ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol` 不再声明或实现。
- 活跃 `ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences` 只消费
  `SZrLspSemanticQuery.canonicalSymbol.symbolId`，并继续拒绝 raw LSP symbol identity mismatch。
- declaration/reference locations 继续来自 parser `DeclarationOf/ReferencesOf` facts；不增加
  symbol name、token text、source scan 或 reference-tracker fallback。
- document highlights 和 imported/external relation adapters保持不变。

## RED/GREEN

现有 local-reference source-contract 同时增加 header/source wrapper 禁止项；固定旧生产代码
真实 exit 1并精确报告两项。删除声明与 wrapper 后同一测试转 GREEN；全仓该名称只剩两个
source-contract 禁止文本。

## Verification

- 固定 `c20a968 + 3 code/test overlays` 的 WSL GCC/Ninja 快照完成 source-contract、local
  semantic query、semantic parity 与 interface 四目标重链。
- GCC source-contract 70/70、semantic parity 15/15，均真实 exit 0；local semantic query
  真实 exit 1，仅保留已登记的 unresolved member-write producer marker。
- GCC interface 真实 exit 1，失败测试名称与固定 parent 的 8 个已登记 producer marker完全
  一致，delta 0；该目标不计本任务 GREEN。
- 独立 Clang/Ninja 缓存完成 source-contract、local semantic query 与 semantic parity重链；
  source-contract 70/70、parity 15/15真实 exit 0，local-query 保持同一 producer marker。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链 16-target matrix 或三套 stdio
  smoke。

## 状态与产出记录

- 完成时间：2026-08-30 16:55 +08:00。
- 状态：Task 7.51 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：无调用 API 审计；source-contract RED/GREEN；Symbol-to-SymbolId reference wrapper
  声明/实现删除；GCC/Clang fixed snapshot 重链；local-query/parity/interface marker 复核；计划
  状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、member-write resolved identity
  producer、source/binary/native 完整 relation parity、其余 analyzer/symbol-table 第二套语义删除、
  MSVC 与完整三工具链 16-target matrix、三套 stdio smoke 和 Plan 03 Task 8 总门禁。
