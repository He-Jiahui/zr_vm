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

# Plan 03 Task 5.7: Primitive Type-Use Alias Producer

## Goal

让显式primitive type-use在parser snapshot中发布source spelling alias，同时保持canonical
`TypeId`与formatter文本独立；LSP不得从AST token或类型名称重建缺失alias。

## Contract

- primitive AST type先完成正常inference，再由同一inferred type生成canonical `TypeId`。
- source spelling以exact type-name `FileRange`发布到Task 5.6 alias fact，不参与TypeId identity。
- `i64`的canonical formatter结果保持`int`，exact alias query返回`i64`。
- 缺semantic context、无效TypeId或fact publication失败不改变推断结果，也不生成诊断。
- generic、nominal、qualified、binary/native alias producer与LSP consumer不在本子项内。

## RED/GREEN

RED解析`var index: i64 = 0;`并完成canonical inference；旧代码能格式化`int`，但exact
alias query返回`NULL`，GCC semantic display真实exit 1，`12 Tests / 1 Failure`。

GREEN将primitive type-use producer提取到独立type-inference模块。identifier type分支仅在
structured primitive resolution成功后调用producer；producer canonicalize同一inferred type并
发布exact range alias，不注册named type或改变兼容性。GCC/Clang semantic display均为
`12/12`、真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过display `12/12`、calls `26/26`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`，全部真实exit 0。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，`19 Tests / 1 Failure`，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个既有producer
  marker，delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 03:23 +08:00。
- 状态：Task 5.7 parser primitive alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：primitive type-use source alias producer；独立type-inference模块；canonical
  `i64 -> int` identity/display与exact `i64` alias TDD；GCC/Clang focused及expanded验证；
  graph/interface fixed-marker复核；模块、计划和子里程碑记录。
- 未完成项目：generic/nominal/qualified source alias producer、LSP alias consumer、owner全变体
  display gate、source receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable
  mapping parity、Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
