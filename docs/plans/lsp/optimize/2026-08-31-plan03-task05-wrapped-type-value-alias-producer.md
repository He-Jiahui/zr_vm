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

# Plan 03 Task 5.13: Wrapped Type-Value Alias Producer

## Goal

让ownership generic内层通过compiler-state canonical type-value alias解析时，在恢复semantic
context后发布exact source alias，同时保持outer owner与inner target TypeId分离。

## Contract

- alias table命中`Word -> int`后，`Unique<Word>`的canonical文本仍为`Unique<int>`。
- inner canonical `int` TypeId在exact `Word` annotation range的use-site alias为`Word`。
- producer只消费structured type-value alias binding与parsed inner identifier node。
- 不为outer `Unique<int>` TypeId发布`Word`，不注册`Word` nominal identity，不改变compatibility
  或diagnostic。
- Gc/GcBox还需class/resource prototype验证，继续作为独立producer子项。

## RED/GREEN

RED在compiler state中注册`Word -> inferred int`，再推断真实parser AST中的
`var handle: Unique<Word>`。owner inference与canonical formatter `Unique<int>`均已通过，但inner
`int` TypeId的exact alias query返回`NULL`；GCC display真实exit 1，`19 Tests / 1 Failure`。

GREEN复用同一个type-value alias binding查询，在ownership inner conversion恢复semantic context后
调用existing explicit alias publisher。GCC/Clang display均为`19/19`、真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `19/19`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity/source-contract真实exit 0、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 05:40 +08:00。
- 状态：Task 5.13 wrapped type-value alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：ownership wrapper alias RED/GREEN；outer/inner TypeId与source alias分离；structured
  alias binding gate；GCC/Clang focused及expanded验证；graph/interface fixed-marker复核；模块、计划
  和子里程碑记录。
- 未完成项目：GcBridge source alias producer、LSP alias consumer、owner全变体display gate、source
  receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
