---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/symbol_table.h
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_no_local_reference_collection_cases.h
plan_sources:
  - docs/plans/lsp/optimize/index.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.62: Canonical SymbolAt Reference Resolution

## Goal

删除 LSP 自有的 AST use-site reference collection，让位置符号解析由 parser semantic snapshot 的 canonical `SymbolAt` 和稳定 `SymbolId` 决定。LSP symbol table 只作为 canonical symbol 的展示投影，不再作为位置语义的事实来源。

## Contract

- `SemanticAnalyzer_GetSymbolAt` 先绑定 query source，再只调用 `ZrParser_SemanticQuery_SymbolAt`。
- canonical query 失败、semantic context 缺失或 `SymbolId` 无效时 fail closed。
- LSP symbol 只能通过 `ZrLanguageServer_SymbolTable_FindBySemanticId` 按稳定 `SymbolId` 查找。
- `GetSymbolAt` 不调用 `ReferenceTracker_FindReferenceAt`，不使用名称、AST 文本或位置范围猜测 canonical identity。
- 删除 `CollectReferencesFromAst` 的 analysis 调用、内部声明和实现文件。
- declaration/property bookkeeping 使用的 `ReferenceTracker_AddReference`、`SZrSymbol.references` 与 `referenceCount` 保留，不属于本子里程碑删除范围。

## Implementation

- `semantic_analyzer_references.c` 的旧 AST reference collector 已删除。
- `semantic_analyzer_analysis.c` 不再在 scope analysis 中调用该 collector。
- `semantic_analyzer_internal.h` 不再暴露 collector 声明。
- `symbol_table.h/c` 新增按 `TZrSymbolId` 查找已投影 `SZrSymbol` 的 bridge。
- local parameter/local variable 测试携带同一 `sourceName`，并直接比较 parser `SymbolAt` 返回的 `SymbolId` 与 LSP symbol identity。
- source-contract test 新增 canonical `SymbolAt`/`FindBySemanticId` 要求和旧 reference collector 禁止项。

## Verification

本子里程碑应执行并记录以下聚焦验证：

- `zr_vm_language_server_symbol_table_test`
- `zr_vm_language_server_reference_tracker_test`
- `zr_vm_language_server_semantic_analyzer_test`
- `zr_vm_language_server_lsp_source_contracts_test`
- `zr_vm_language_server_semantic_query_parity_test`
- `zr_vm_language_server_local_semantic_query_test`
- LSP interface 回归，并与 parent baseline 对账失败集合和 delta。

source-contract 必须继续阻止 `CollectReferencesFromAst`、`ReferenceTracker_FindReferenceAt` 位置回退和 name/range semantic reconstruction。若完整 analyzer 或 interface 仍有既有 producer failure，必须记录真实测试名称与 exit code，不得将 focused GREEN 扩大为全局 GREEN。

## 状态与产出记录

- 完成时间：2026-09-02。
- 状态：Plan 03 Task 7.62 focused 子里程碑完成；Plan 03 Task 7 与 Task 8 仍进行中。
- 完成项目：删除 LSP AST use-site reference collector；`SemanticAnalyzer_GetSymbolAt` 改为 parser canonical `SymbolAt` + stable `SymbolId` projection；补充 source identity 与 canonical identity regression；加入 source-contract 防回归。
- 保留边界：definition/property bookkeeping、`ReferenceTracker_AddReference`、`SZrSymbol.references` 和 `referenceCount` 仍被其他路径使用，本次不删除。
- 未完成项目：上层 `Lsp_FindSymbolAtUsageOrDefinition` 的 allScopes/range fallback；source/binary/native sourceless relation matrix；virtual declaration URI；真实 multi-provider nonzero generation；其余 Task 7 consumer；完整 16-target matrix；三套 stdio/CLI smoke；Task 8 总门禁。
