---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.4: Callable Effect Display Integrity

## Goal

让 SymbolId callable signature 完整投影 canonical function `TypeId` 的 effect 与 passing
contract；不得校验 effect 后又静默丢弃，或由 LSP 从声明文本补写。

## Contract

- `async`、`generator` 位于 receiver prefix 与 callable name 之前，顺序与 canonical type
  formatter一致。
- `throws` 位于返回类型之后。
- value、`in`、`ref`、`ref readonly`、`out` 继续从有序 canonical parameter contract投影。
- receiver effect、parameter legality与未知 effect bits继续 fail closed；不读取 AST effect
  spelling、member name 或 display text重建语义。

## RED/GREEN

RED 注册一个携带 value/in/ref/ref readonly/out 五种 passing form，并同时带 async、generator、
throws bits 的 canonical function SymbolId。旧实现正确输出五个参数，但只得到
`execute(...): int`，缺失全部 effect；GCC focused真实 exit 1、`9 Tests / 1 Failure`。

GREEN 在 `ZrParser_SemanticDisplay_CreateCallableSignature` 中直接消费已校验的 canonical
effect bits，输出 `async generator execute(...): int throws`。GCC/Clang semantic display均为
`9/9`、真实 exit 0。

## Verification

- GCC/Clang 固定 snapshot 均通过 display `9/9`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实 exit 0。
- GCC/Clang interface进程均真实 exit 1，失败集合严格保持 fixed parent同一8个既有 producer
  marker，delta 0且不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 02:41 +08:00。
- 状态：Task 5.4 focused GREEN；Plan 03 Task 4/Task 5/Task 7/Task 8 总门禁仍进行中。
- 完成项目：SymbolId callable async/generator/throws display；value/in/ref/ref readonly/out
  passing display gate；GCC/Clang focused与expanded验证；interface fixed-marker复核；模块、计划和
  子里程碑记录。
- 未完成项目：owner全变体与use-site alias display fact、source receiver/member argument mapping、
  receiver `TypeId`、`.zro`/native callable mapping parity、Task 5 consumer收口、Syntax05 imported
  property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
