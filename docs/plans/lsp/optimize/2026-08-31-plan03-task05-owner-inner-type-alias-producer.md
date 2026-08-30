---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.h
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.8: Ownership Wrapper Inner Type Alias Producer

## Goal

修复ownership generic在递归转换内层类型时临时关闭semantic context造成的alias producer缺口；
外层owner canonical identity与内层source spelling必须分别按各自TypeId/range查询。

## Contract

- `Unique<T>`、`Shared<T>`、`Weak<T>`继续通过structured ownership generic unwrap解析，不按名称推断。
- 内层类型正常转换后、附加owner qualifier前，仅对primitive identifier发布exact-range alias。
- `Unique<i64>`的outer TypeId格式化为`Unique<int>`；inner primitive TypeId在`i64` range返回`i64`。
- producer不把outer owner TypeId绑定到inner range，也不发布任意nominal source text。
- GcBridge wrapper、generic/nominal/qualified、binary/native alias producer与LSP consumer不在本子项内。

## RED/GREEN

RED解析并转换`var handle: Unique<i64> = null;`。outer canonical display已为`Unique<int>`，但
inner primitive TypeId的exact alias query返回`NULL`；GCC display真实exit 1，
`13 Tests / 1 Failure`，其余12项通过。

GREEN将primitive AST identifier验证移入独立alias producer模块，普通primitive与ownership
wrapper复用同一入口。ownership分支在恢复semantic context后、设置ownership qualifier前发布
inner fact，不改变inference、compatibility或diagnostics。GCC/Clang display均为`13/13`、
真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过display `13/13`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实exit 0。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个既有producer
  marker，delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 03:38 +08:00。
- 状态：Task 5.8 ownership wrapper inner primitive alias producer GREEN；Plan 03 Task 5/Task 7/
  Task 8总门禁仍进行中。
- 完成项目：ownership wrapper inner primitive alias TDD；outer/inner TypeId与range分离；
  primitive AST validation模块化；GCC/Clang focused及expanded验证；graph/interface fixed-marker
  复核；模块、计划和子里程碑记录。
- 未完成项目：GcBridge nested alias producer、generic/nominal/qualified source alias producer、
  LSP alias consumer、owner全变体display gate、source receiver/member argument mapping、receiver
  `TypeId`、`.zro`/native callable mapping parity、Syntax05 imported property/declaration producer、
  MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
