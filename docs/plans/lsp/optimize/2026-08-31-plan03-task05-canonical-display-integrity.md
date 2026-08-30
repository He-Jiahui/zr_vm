---
related_code:
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.1: Canonical Display Integrity

## Goal

让 canonical type formatter 与 SymbolId callable display 只展示完整、有效的 snapshot contract；
损坏或未知字段必须返回 unavailable，LSP 不得从 AST、type text 或 member name 重建签名。

## Contract

- `ZrParser_CanonicalType_ValidateParameterContract` 是 intern、formatter、callable display 共用的
  read-only validator。
- validator 同时核对 TypeId、passing form、ref access、escape upper bound、entry/exit
  initialization、temporary acceptance 与 call-site marker。
- canonical ref access、receiver effect 与 callable effect flags 必须属于当前已发布枚举/bit set。
- malformed type format 清空输出并返回 false；malformed callable display 返回 NULL。
- unavailable display 不授权任何 LSP name/type-text fallback。

## RED/GREEN

RED 直接损坏已经成功 intern 的 function/ref snapshot：invalid receiver、未知 effect bit 与 invalid
ref access 在旧 formatter 中均可被展示；focused GCC 为 `5 Tests / 1 Failure`，首个失败为
`Expected FALSE Was TRUE`。

GREEN 发布现有 intern parameter validator，并让 intern、canonical formatter 与 callable display
共用它。测试继续覆盖非法 VALUE escape/temporary 与非法 passing form，保证不仅 enum surface，
完整 parameter contract 也 fail closed。GCC/Clang semantic display 均为 `6/6`。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `6/6`、calls `25/25`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`，全部真实 exit 0。
- canonical graph 在 GCC/Clang 均为 `18 Pass / 1` 个相同既有 marker：legacy `pair():`
  fixture 在 formatter 前被当前 syntax cutover 拒绝；该目标不计 GREEN。
- GCC/Clang interface 进程均真实 exit 1，失败集合严格保持 fixed parent 的同一8个 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 02:02 +08:00。
- 状态：Task 5.1 focused GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：共享 canonical parameter validator；ref/receiver/effect integrity；parameter snapshot
  integrity；formatter buffer fail-closed；callable display fail-closed；GCC/Clang focused 与
  interface marker 复核；模块、计划和子里程碑记录。
- 更正项目：隔离复核证明 source `in/ref/out` mapping 已有 producer 支持并通过临时 `26/26`；
  Task 4.23 的 producer-gap 结论已更正。
- 未完成项目：source receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable
  mapping parity、Task 5 全形态/consumer 收口、Syntax05 imported property/declaration producer、
  MSVC、完整16-target matrix、三套 stdio smoke、Plan 03 Task 8 总门禁。
