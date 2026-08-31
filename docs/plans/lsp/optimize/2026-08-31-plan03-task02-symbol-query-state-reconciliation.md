---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-24-plan03-task02-symbol-at.md
  - docs/plans/lsp/optimize/2026-08-24-plan03-task02-visible-symbols.md
  - docs/plans/lsp/optimize/2026-08-29-plan03-task07-canonical-visible-symbol-completion.md
doc_type: milestone-record
---

# Plan 03 Task 2.3: Symbol Query State Reconciliation

## Goal

对齐主计划Task2 checkbox与既有`SymbolAt`、`VisibleSymbols` producer/query/consumer完成记录，
避免把已完成合同误判为新实现缺口。

## Audit Result

- `SZrParserSemanticSymbolQuery`、`SZrParserSemanticVisibleSymbolOptions`、`SymbolAt`与
  `VisibleSymbols`已由parser public API发布；display/declaration pointers的lifetime绑定semantic
  snapshot。
- source/native/import/type/generic/receiver candidates、lexical shadowing、overload、visibility、
  static context和稳定排序均由既有parser tests覆盖。
- canonical lexical completion只消费`ZrParser_SemanticQuery_VisibleSymbols`；source-contract禁止
  恢复analyzer completion、symbol-table visible-symbol或range scope walk。
- 因此Task2剩余的API与compiler-owned scope reference-check两项属于状态漂移。

## Verification

- 当前固定GCC/Clang snapshot的parser symbols均`21/21`、真实exit 0。
- 当前GCC/Clang semantic-query parity均`15/15`、LSP source contracts均`70/70`，真实exit 0。
- 历史MSVC evidence保留在Task2原records；本对账项未重跑MSVC。
- 未运行完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 08:09 +08:00。
- 状态：Task 2状态对账完成；Plan 03总体仍进行中。
- 完成项目：public symbol query API；compiler-owned visible scope facts；stable order；canonical
  completion consumer；禁止LSP scope/name扫描的source-contract；主计划checkbox对齐。
- 未完成项目：Task3 external relation producer；Task4 receiver/member和binary/native parity；
  Task7其余consumer迁移；Task8 MSVC/完整16-target matrix/stdio smoke及总验收。
