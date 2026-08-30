---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/symbol_table.h
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_symbol_table.c
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.45: Symbol Reference Count Getter Removal

## Goal

删除 consumer 迁移后全仓无生产调用的 per-symbol reference count getter，继续缩小 LSP
自有 reference collection API。保留仍被 Syntax05 exact-owned interface test和生产
interface support读取的字段/数组，避免越权或制造编译断点。

## Contract

- `ZrLanguageServer_Symbol_GetReferenceCount` 不再提供第二套 reference-count query；
  CodeLens/reference consumers继续通过 parser relation query计算结果。
- symbol-table focused test直接验证 `SZrSymbol.references` 中保存的精确 range，不为零调用
  getter提供需求。
- `SZrSymbol.references`、`referenceCount`、`Symbol_AddReference`暂时保留：生产
  `lsp_interface_support.c`读取range数组，Syntax05 exact-owned `test_lsp_interface.c`读取count
  字段作为失败诊断。本任务不修改这两个已占用路径。
- 不增加 name、raw symbol或source-text reference fallback。

## RED/GREEN

source-contract 对 `symbol_table.c` 新增 getter禁止项，旧实现真实 exit 1并精确失败一项。
最初同时禁止 `referenceCount` 字段的审计草案暴露 Syntax05-owned interface test仍有字段
读取，因此按ownership边界收紧：只删除真正零调用的 getter，保留字段生命周期。GREEN
后 getter在production/test仅剩source-contract禁止文本。

## Verification

- 全仓扫描确认 `ZrLanguageServer_Symbol_GetReferenceCount` 无调用、声明或定义，仅剩
  source-contract禁止文本。
- WSL GCC/Clang对production `symbol_table.c` syntax checks真实 exit 0；GCC对focused
  symbol-table test syntax check真实 exit 0。
- 固定 `cdb214a + 4 code/test overlays` 的独立 GCC/Ninja 快照完成 symbol-table、
  reference-tracker、source-contract与Syntax05-owned interface test四目标重链，build exit 0。
- symbol table 4/4、reference tracker 5/5、source-contract全套均真实 exit 0；interface
  executable本任务只要求编译/链接验证，未将其既有runtime marker计作GREEN。
- `git diff --check`通过；本任务未重跑完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 15:06 +08:00。
- 状态：Task 7.45 focused GREEN；Plan 03 Task 7/Task 8总门禁仍进行中。
- 完成项目：getter零调用审计；source-contract RED/GREEN；public getter声明/实现删除；
  symbol-table range storage test更新；Syntax05 ownership边界复审；固定快照四目标重链；
  GCC/Clang syntax与focused runtime验证；计划状态记录。
- 未完成项目：`references/referenceCount`最终删除及其interface consumer迁移、Syntax05
  imported declaration producer、source/binary/native relation parity、三工具链完整16-target
  matrix、三套stdio smoke及其余active LSP symbol-table/typecheck consumers。
