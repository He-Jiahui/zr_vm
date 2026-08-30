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

# Plan 03 Task 5.9: Qualified Type-Use Alias Producer

## Goal

让structured qualified type resolution发布whole-use source alias，同时保持canonical module
identity为formatter唯一输入；缺少authoritative整体range的wrapped type必须fail closed。

## Contract

- qualified alias仅在`ast_type_try_resolve_qualified_inferred_type`成功后发布，不按name猜目标。
- alias文本由parsed `SZrType` chain结构化生成，range从root name起点延伸到terminal name终点。
- `declaration.Patch`通过真实compile-tool provider解析为canonical
  `zr.compile.declaration.Patch`，两种presentation不共享identity字段。
- arrays与ref/owner/readonly wrapped-qualified type当前没有authoritative whole range，不发布
  partial-range alias。
- generic/nominal/GcBridge、binary/native alias producer与LSP consumer不在本子项内。

## RED/GREEN

RED注册`zr.compile.declaration`真实provider并转换`declaration.Patch`。canonical formatter已输出
`zr.compile.declaration.Patch`，但exact whole-use alias query返回`NULL`；GCC display真实exit 1，
`14 Tests / 1 Failure`，其余13项通过。

GREEN新增通用explicit alias publisher，primitive入口继续先验证structured primitive AST。
qualified converter仅在resolver成功、最终wrapper/array result已形成后调用publisher；publisher
对无法由name chain完整覆盖的wrapper fail closed。GCC/Clang display均为`14/14`、真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过display `14/14`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实exit 0。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 03:55 +08:00。
- 状态：Task 5.9 qualified type-use alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁
  仍进行中。
- 完成项目：真实compile-tool qualified alias TDD；whole-use structured range；canonical/source
  presentation分离；partial wrapped range fail-closed；GCC/Clang focused及expanded验证；
  graph/interface fixed-marker复核；模块、计划和子里程碑记录。
- 未完成项目：wrapped-qualified authoritative range、generic/nominal/GcBridge source alias producer、
  LSP alias consumer、owner全变体display gate、source receiver/member argument mapping、receiver
  `TypeId`、`.zro`/native callable mapping parity、Syntax05 imported property/declaration producer、
  MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
