---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.h
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_generic_alias_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.11: Const-Generic Expression Alias

## Goal

让generic use-site alias保留const expression的structured source presentation，同时继续以求值后的
const value构造canonical TypeId，禁止alias文本反向污染identity。

## Contract

- `Matrix<i64, 2 + 2>` canonical display为`Matrix<int, 4>`。
- 同一TypeId与exact whole-use range的source alias为`Matrix<i64, 2 + 2>`。
- alias renderer只消费parsed type/identifier/integer/unary/binary AST，不切source文本。
- binary operator spacing与generic separators按稳定AST presentation规范化。
- compiler通用type-name/const evaluator、generic compatibility与canonical const identity保持不变。
- unsupported AST、wrapper、incomplete generic、invalid range或buffer overflow时alias fail closed，
  type inference不受影响。

## RED/GREEN

RED解析并推断`Matrix<i64, 2 + 2>`。canonical formatter已正确返回`Matrix<int, 4>`，但旧alias
复用const evaluator而返回`Matrix<i64, 4>`；GCC display真实exit 1，`17 Tests / 1 Failure`，
唯一失败为source expression alias文本不一致。

GREEN在focused alias模块增加structured generic presentation builder和窄publisher；generic inference
完成canonical result后调用该publisher，不再把canonical argument text当source alias。GCC/Clang
display均为`17/17`、真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `17/17`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity真实exit 0、source-contract真实exit 0、facts
  `15/15`、canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 05:20 +08:00。
- 状态：Task 5.11 const-generic expression alias GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：const expression source alias RED/GREEN；alias/canonical evaluator分离；structured AST
  presentation；unsupported-shape fail-closed；GCC/Clang focused及expanded验证；graph/interface
  fixed-marker复核；模块、计划和子里程碑记录。
- 未完成项目：nominal/GcBridge source alias producer、LSP alias consumer、owner全变体display gate、
  source receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、
  Plan 03 Task 8总门禁。
