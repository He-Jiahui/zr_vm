---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.43: Dead Project Navigation Fallback Removal

## Goal

删除 project navigation 中已经没有调用者、但仍按 AST import hit、module/member name、
LSP global symbol 和 raw `SZrSymbol *` 重建 definition/reference/highlight 的旧入口。主接口
只保留已生效的 `LspSemanticQuery` canonical consumer。

## Contract

- `ZrLanguageServer_Lsp_GetDefinition/FindReferences/GetDocumentHighlights` 不再存在可回退
  到 `ProjectTry*` 第二套语义的内部 API。
- project navigation 不按 imported member name 查找目标 analyzer 的 global symbol，也不
  将 raw symbol name 用作跨项目 reference relation key。
- 删除 raw `SZrLspProjectResolvedSymbol` transport；SymbolId/relation query 的主路径位于
  `LspSemanticQuery`/`LspSemanticReferenceQuery`。
- active binary/native external metadata declaration/reference/highlight adapters 保持不变；
  本任务不把 external metadata name adapter冒充 source canonical identity。
- Syntax05 exact-owned parser import metadata、`lsp_interface.c` 和
  `test_lsp_interface.c` 均不在本任务写集。

## RED/GREEN

RED 在 source-contract 中禁止三个无调用者的 `ProjectTry*` API、
`project_resolve_symbol_at_position`、`find_global_symbol_by_name`、raw-symbol reference
bridge 和 transport type。旧代码精确产生 9 项失败。GREEN 删除这些 API 及独占 helper；
Clang 随后暴露同一死链中的 external imported-member resolver，继续删除后无
`unused-function` 告警。旧测试对死 bridge 和死 source-position resolver 的正向要求同步
移除，canonical relation 主路径断言保持不变。

## Verification

- 仓库生产源码全局扫描确认 9 个旧 API/helper/type 名称计数均为 0。
- WSL GCC 与 Clang 对 `lsp_project_navigation.c` 的 `-fsyntax-only` 均真实 exit 0，
  Clang 无新增 warning。
- 当前工作树重新编译并运行 `test_lsp_source_contracts.c`，真实 exit 0，全部 source
  contracts 通过。
- `git diff --check` 通过。production 三个文件净删 519 行；测试增加 fail-closed
  contract 并移除两条只为死路径存在的过时正向断言。
- 本任务未重跑完整三工具链 16-target matrix 或三套 stdio smoke，不将旧 binary 或
  既有 marker 计作本阶段证据。

## 状态与产出记录

- 完成时间：2026-08-30 14:22 +08:00。
- 状态：Task 7.43 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：dead project semantic API 调用图审计；source-contract RED/GREEN；
  global-name/raw-symbol/import-member fallback 删除；内部声明与 transport type 删除；
  GCC/Clang syntax verification；计划状态记录。
- 未完成项目：Syntax05 imported declaration producer、source/binary/native relation parity、
  三工具链完整 16-target matrix、三套 stdio smoke，以及其余 active LSP symbol-table/
  typecheck consumers；不得用恢复死 API 或名称 fallback 替代这些 producer 工作。
