---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.38: Local Reference Cross-Project Fail-Closed

## Goal

移除 local-symbol references consumer 中按 analyzer `Symbol->name` 扩展跨项目引用的
非 canonical 路径。local query 只消费 parser relation query 的结果；跨项目 imported
member identity 未由 producer 发布时保持 unavailable，等待 canonical relation/index
ownership 收口。

## Contract

- `ZrLanguageServer_LspSemanticQuery_AppendReferences` 对
  `ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL` 只委托
  `ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences`。
- local branch 不读取 `query->symbol->name`、symbol location、访问修饰符或 project
  module name 来推断 imported references。
- imported-member 与 external-metadata 分支继续消费各自已有的 resolved declaration
  identity；本任务不新增 name fallback，也不修改 Syntax05 producer 路径。
- parser relation query 未返回跨项目关系时，LSP 保持 fail-closed，不把同名成员当作
  同一 SymbolId。

## RED/GREEN

RED 由 source-contract regression 固定：在 `AppendReferences` 函数范围内禁止出现
`query->symbol->name`。旧实现先查询 parser relation，随后对 public local symbol 按
源文件 module 与 symbol name 调用 imported-member aggregation helper，违反 canonical
identity 边界。GREEN 删除该 local-only aggregation block，保留 parser relation query
返回值。

## Verification

- source-contract target 必须从当前工作树源码编译并以仓库根目录为工作目录执行；测试进程
  的真实退出码与 source-contract 结果必须一致。
- GCC、Clang、MSVC 的 source-contract focused processes 均真实 exit 0；生产
  `lsp_semantic_query.c` 的 GCC/Clang/MSVC 单文件语法检查也通过。三套 interface、
  stdio smoke 与 Plan 03 16-target matrix 仍需在最终门禁阶段使用同一基线重跑。
- 现有 member-write producer fact、class-member interface fixture、imported metadata
  及 `short_circuit_unreachable` producer warning 失败不在本任务中伪造或改写。

## 状态与产出记录

- 完成时间：2026-08-30 08:31 +08:00。
- 状态：Task 7.38 focused GREEN；Plan 03 Task 7/Task 8 仍未完成。
- 完成项目：local reference consumer 的 name-based cross-project aggregation 已移除；
  source-contract regression 已加入并通过；imported/external canonical 分支与 Syntax05
  producer ownership 已保持。
- 未完成项目：同一工作树基线上的 interface、stdio smoke 与最终 16-target matrix
  重放；既有 producer/metadata 阻塞项不在 LSP 侧增加兼容。
