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
  的真实退出码与 source-contract 结果必须一致。2026-08-30 09:50 +08:00 的 GCC、Clang、
  MSVC source-contract 均为 `70/70`、真实 exit 0；MSVC 临时验证快照的 LF 行尾已与源码
  contract 的多行片段一致。
- 同一验证基线的 Plan 03 16-target matrix 三套均为 `10 PASS / 6 FAIL`。失败只来自
  canonical tuple producer、semantic analyzer producer markers、interface imported/class
  fixture、local member-write producer fact 与 imported-type matrix；Task 7.38 相关
  local reference cases通过。三套 stdio smoke 均在 `stdio_smoke.js:2003` 因
  `short_circuit_unreachable` producer warning 缺失真实 exit 1，三套 CLI `--version`
  均真实 exit 0。
- 现有 member-write producer fact、class-member interface fixture、imported metadata
  及 `short_circuit_unreachable` producer warning 失败不在本任务中伪造或改写。

## 状态与产出记录

- 完成时间：2026-08-30 09:50 +08:00。
- 状态：Task 7.38 focused GREEN；post-commit overall gate 未通过。三工具链 source-contract
  `70/70` 通过，但最终 16-target 仅 `10/16`，Plan 03 Task 7/Task 8 仍未完成。
- 完成项目：local reference consumer 的 name-based cross-project aggregation 已移除；
  source-contract regression 与三工具链真实退出复核已通过；同基线 16-target、stdio smoke
  和 CLI 回放完成；imported/external canonical 分支与 Syntax05 producer ownership 保持。
- 未完成项目：canonical tuple、member-write、imported-type、interface fixture、analyzer
  producer 与 `short_circuit_unreachable` fixture 缺口；不在 LSP 侧增加兼容，待 producer
  归属路径收口后重跑完整矩阵与三套 smoke。

- 支持提交后复核时间：2026-08-30 10:07 +08:00。Windows semantic-fact helper 导出
  已在 VSDevCmd `17.14.38` 下重链；三工具链 source-contract 仍为 `70/70`，16-target
  仍为 `10/16`，未引入新的 LSP fallback。
