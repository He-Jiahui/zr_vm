---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/symbol_table.h
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
doc_type: milestone-record
---

# Plan 03 Task 7.47: Symbol Table Query Surface Removal

## Goal

删除 canonical parser symbol query 接管后已无调用的 LSP symbol-table 可见符号与范围扫描
API，继续收敛 LSP 自有 scope enumeration 语义。保留仍被 exact lookup、definition 与
analyzer snapshot 使用的 symbol/scope storage，不把旧扫描迁移到新的兼容入口。

## Contract

- `ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition` 不再是公开或生产 API；lexical
  completion 继续只消费 `ZrParser_SemanticQuery_VisibleSymbols`。
- `ZrLanguageServer_SymbolTable_GetSymbolsInRange` 不再是公开或生产 API；LSP consumer
  不得重新遍历 scope stack/global scope 生成语义符号集合。
- 仅被范围扫描使用的 `is_symbol_in_range` 随入口删除；活跃的 position lookup、scope
  depth 与 definition selection helper 保持不变。
- `SZrSymbolTable`、scope/symbol storage、`LookupAtPosition` 与 `FindDefinition` 本阶段不变，
  后续删除必须等待对应 canonical consumer 与 Syntax05 ownership 释放。

## RED/GREEN

canonical completion source-contract 增加 symbol-table header/source 禁止项。旧代码固定快照
真实 exit 1，精确报告两个 API 的声明/实现共四项；删除后同一测试转 GREEN，全仓名称只剩
source-contract 禁止文本，未增加 name/token/type-text fallback。

## Verification

- 固定 `64b8cff + 3 code/test overlays` 的 WSL GCC/Ninja 快照完成 LSP static library、
  source-contract、symbol-table、semantic parity 与 interface 五目标重链。
- GCC source-contract 69/69、symbol-table 4/4、semantic parity 15/15，均真实 exit 0。
- 独立 Clang/Ninja 静态缓存完成同一五目标重链；source-contract 69/69、symbol-table 4/4、
  semantic parity 15/15，均真实 exit 0。
- GCC interface parent/overlay 均真实 exit 1，失败集合精确保持同一 8 个已登记 producer
  marker，delta 0；该目标不计本任务 GREEN。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链 16-target matrix 或三套
  stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 16:06 +08:00。
- 状态：Task 7.47 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：零调用审计；source-contract RED/GREEN；两个公开/生产 API 与独占 helper
  删除；失效命名映射清理；GCC/Clang 固定快照重链和 focused runtime 验证；GCC interface
  parent/overlay marker delta 复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、source/binary/native 完整
  relation parity、其余 symbol-table/analyzer 第二套语义删除、MSVC 与完整三工具链
  16-target matrix、三套 stdio smoke 和 Plan 03 Task 8 总门禁。
