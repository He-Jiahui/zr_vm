---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_nominal_alias_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.12: Type-Value Alias Producer

## Goal

让compiler-state canonical type-value alias在direct type-position use发布source alias，并保持target
TypeId、canonical display与alias identifier严格分离。

## Contract

- alias table命中`Word -> int`后，推断结果仍是canonical `int`。
- exact `Word` annotation range的use-site alias为`Word`。
- producer只消费成功的structured alias binding，不按identifier text查找named type。
- 不注册`Word` nominal TypeId，不改变compatibility、diagnostic或alias-target relation。
- wrapper内无semantic-context的alias use与GcBridge传播不在本子项内。

## RED/GREEN

RED在compiler state中注册`Word -> inferred int`，再推断真实parser AST中的`var value: Word`。
inference与canonical formatter `int`均已通过，但exact alias query返回`NULL`；GCC display真实
exit 1，`18 Tests / 1 Failure`。

GREEN在`compiler_lookup_type_value_alias`成功后复用existing explicit alias publisher，以target
canonical TypeId和exact identifier range发布`Word`。GCC/Clang display均为`18/18`、真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `18/18`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity/source-contract真实exit 0、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 05:29 +08:00。
- 状态：Task 5.12 type-value alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：direct type-value alias RED/GREEN；target TypeId/source alias分离；nominal registration
  fail-closed边界；GCC/Clang focused及expanded验证；graph/interface fixed-marker复核；模块、计划和
  子里程碑记录。
- 未完成项目：wrapped type-value alias propagation、GcBridge source alias producer、LSP alias
  consumer、owner全变体display gate、source receiver/member argument mapping、receiver `TypeId`、
  `.zro`/native callable mapping parity、Syntax05 imported property/declaration producer、MSVC、完整
  16-target matrix、三套stdio smoke、Plan 03 Task 8总门禁。
