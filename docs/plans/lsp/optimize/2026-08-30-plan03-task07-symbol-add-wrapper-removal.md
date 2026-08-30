---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/symbol_table.h
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_symbol_table.c
  - tests/language_server/test_reference_tracker.c
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.48: Symbol Add Wrapper Removal

## Goal

删除全仓无生产调用的 `ZrLanguageServer_SymbolTable_AddSymbol` 兼容 wrapper，让生产与测试
统一通过能返回 canonical symbol identity 的 `AddSymbolEx` 入口构建仍需保留的 LSP symbol
storage。本阶段不扩大 symbol table 职责，也不改变 parser canonical query consumer。

## Contract

- `ZrLanguageServer_SymbolTable_AddSymbol` 不再是公开或生产 API。
- 活跃生产入口保持为 `ZrLanguageServer_SymbolTable_AddSymbolEx`；调用方可通过
  `outSymbol` 取得已登记的 exact symbol identity。
- legacy symbol-table/reference-tracker tests 只把 setup 改为
  `AddSymbolEx(..., ZR_NULL)`，不新增另一层 helper，也不改变被测 lookup/reference 行为。
- symbol/scope storage 仍服务尚未迁移的 analyzer 与 Syntax05-owned consumers；本阶段不删除
  `AddSymbolEx`、`LookupAtPosition`、`FindDefinition` 或 raw symbol fields。

## RED/GREEN

reference identity source-contract 增加 symbol-table header/source 禁止项。旧代码固定快照真实
exit 1，精确报告 wrapper 声明/实现两项；测试 setup 迁移并删除 wrapper 后转 GREEN，全仓
名称只剩 source-contract 禁止文本。

## Verification

- 固定 `5826e0a + 5 code/test overlays` 的 WSL GCC/Ninja 快照完成 LSP static library、
  source-contract、symbol-table、reference-tracker、semantic parity 与 interface 六目标重链。
- GCC source-contract 69/69、symbol-table 4/4、reference-tracker 5/5、semantic parity 15/15，
  均真实 exit 0。
- 独立 Clang/Ninja 静态缓存完成同一六目标重链；四个 focused 运行目标同为 69/69、4/4、
  5/5、15/15，均真实 exit 0。
- GCC interface 真实 exit 1；失败测试名称与 Task 7.47 parent 的 8 个已登记 producer marker
  完全一致，delta 0，因此不计本任务 GREEN。
- `git diff --check` 通过；本任务未执行 MSVC、完整三工具链 16-target matrix 或三套
  stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 16:18 +08:00。
- 状态：Task 7.48 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：零生产调用审计；source-contract RED/GREEN；五处 legacy test setup 迁移；
  wrapper 声明/实现和失效命名映射删除；GCC/Clang 固定快照重链与 focused runtime；GCC
  interface parent/overlay marker delta 复核；计划状态记录。
- 未完成项目：Syntax05 imported declaration/property producer、source/binary/native 完整
  relation parity、其余 symbol-table/analyzer 第二套语义删除、MSVC 与完整三工具链
  16-target matrix、三套 stdio smoke 和 Plan 03 Task 8 总门禁。
