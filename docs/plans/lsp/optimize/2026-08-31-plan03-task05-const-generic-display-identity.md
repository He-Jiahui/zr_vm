---
related_code:
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.2: Const Generic Display Identity

## Goal

让 const generic parameter 的 canonical 文本只依赖 TypeId identity，不受第一个 intern alias
影响；alias 作为展示 metadata 保留，但不再混入 canonical formatter。

## Contract

- const parameter identity 是 `(ownerSymbolId, ordinal)`，与现有 hash/equality 一致。
- `displayName` 不属于 TypeId identity，canonical formatter 不读取它。
- canonical 输出固定为 `$const(owner,ordinal)`；closed const integer仍输出值，type argument仍递归
  输出 canonical TypeId。
- 需要 source alias 的上层必须消费独立、use-site scoped display fact，不得从名称或源码重建。
- const integer/parameter 的局部 `snprintf` 同时检查负值与截断，失败时保持 formatter fail closed。

## RED/GREEN

RED 用 `Matrix<int, 4, N>` 首次 intern，再以同一 owner/ordinal 和 `OtherAlias` 重复 intern；两者
正确返回同一 TypeId，但旧 formatter 输出首个 alias `N`。GCC focused 真实 exit 1，
`7 Tests / 1 Failure`：expected `$const(77,2)`，actual `N`。

GREEN 不改变结构、hash、equality或 metadata storage，只让 canonical formatter忽略可选 alias并
输出结构化 owner/ordinal。GCC/Clang semantic display 均为 `7/7`。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `7/7`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实 exit 0。
- canonical graph 在 GCC/Clang 均为 `18 Pass / 1` 个相同既有 legacy `pair():` syntax marker；
  该目标不计 GREEN。
- GCC/Clang interface 进程均真实 exit 1，失败集合严格保持 fixed parent 同一8个 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 02:16 +08:00。
- 状态：Task 5.2 focused GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：const generic alias/identity 分离；混合 type/const-int/const-parameter display；
  insertion-order independent TypeId text；format truncation guard；GCC/Clang expanded、
  type-inference与interface marker复核；模块、计划和子里程碑记录。
- 未完成项目：source/use-site alias display fact、source receiver/member argument mapping、receiver
  `TypeId`、`.zro`/native callable mapping parity、Task 5 全形态/consumer收口、Syntax05 imported
  property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
