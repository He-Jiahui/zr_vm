---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.h
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_nominal_alias_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.14: GcBridge Type-Value Alias Producer

## Goal

让`Gc<T>`/`GcBox<T>`内层通过compiler-state canonical type-value alias解析时，在canonical
prototype world校验后发布exact source alias，同时保持inner target与outer bridge TypeId分离。

## Contract

- `DocAlias -> Document`用于`Gc<DocAlias>`时，canonical文本为`Gc<Document>`，inner alias为
  `DocAlias`。
- `ResourceAlias -> BoxedResource`用于`GcBox<ResourceAlias>`时，canonical文本为
  `GcBox<BoxedResource>`，inner alias为`ResourceAlias`。
- `Gc`只接受ordinary class，`GcBox`只接受resource class；world校验失败不发布alias。
- alias必须在附加`gcBridgeKind`前绑定inner target TypeId，不得绑定outer wrapper TypeId。
- 不注册alias nominal identity，不改变compatibility、diagnostic或prototype relation。

## RED/GREEN

RED在compiler state中注册ordinary/resource class prototypes与两条structured type-value alias，
再推断真实parser AST中的`Gc<DocAlias>`与`GcBox<ResourceAlias>`。prototype gate与outer canonical
display均已正确，但第一个inner exact alias query返回`NULL`；GCC display真实exit 1，
`20 Tests / 1 Failure`。

GREEN在GcBridge target通过prototype world校验后、设置bridge kind前复用existing type-value alias
publisher。GCC/Clang display均为`20/20`、真实exit 0，两种bridge kind均覆盖。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `20/20`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity/source-contract真实exit 0、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 05:59 +08:00。
- 状态：Task 5.14 GcBridge type-value alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：Gc/GcBox alias RED/GREEN；ordinary/resource prototype world gate；inner/outer TypeId
  与source alias分离；GCC/Clang focused及expanded验证；graph/interface fixed-marker复核；模块、计划
  和子里程碑记录。
- 未完成项目：LSP alias consumer、owner全变体display gate、source receiver/member argument
  mapping、receiver `TypeId`、`.zro`/native callable mapping parity、Syntax05 imported
  property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
