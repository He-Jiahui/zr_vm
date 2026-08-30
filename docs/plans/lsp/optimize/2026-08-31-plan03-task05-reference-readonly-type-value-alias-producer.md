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

# Plan 03 Task 5.15: Reference/Readonly Type-Value Alias Producer

## Goal

让direct type-value alias位于`readonly`、`ref`或`ref readonly`source wrapper中时发布inner
target的exact source alias，同时保持最终wrapper TypeId与canonical display不变。

## Contract

- `Word -> Document`用于`readonly Word`时，canonical文本为`readonly Document`，inner alias为
  `Word`。
- `ref Word`与`ref readonly Word`分别保持canonical `ref Document`与
  `ref readonly Document`，exact identifier alias仍绑定inner `Document` TypeId。
- producer只规范化parsed type-use的局部副本，不修改AST或最终inferred wrapper。
- alias-table binding必须先成功；unbound、generic、qualified或malformed name不走该路径。
- 不为outer wrapper TypeId发布`Word`，不注册alias nominal identity，不改变compatibility或diagnostic。

## RED/GREEN

RED在独立compiler snapshots中注册`Word -> inferred Document`，再分别推断真实parser AST中的
三种wrapper。outer inference与canonical formatter均已正确，但第一个`readonly Word` inner exact
alias query返回`NULL`；GCC display真实exit 1，`21 Tests / 1 Failure`。

GREEN让专用type-value alias publisher清除局部type-use副本的wrapper flags，以resolved target和
identifier range发布fact；direct resolver在附加wrapper前调用。GCC/Clang display均为`21/21`、
真实exit 0。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `21/21`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity/source-contract真实exit 0、facts `15/15`、
  canonical consumers `21/21`、type inference `124/124`。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，失败集合严格保持fixed parent同一8个producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 06:10 +08:00。
- 状态：Task 5.15 reference/readonly type-value alias producer GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：readonly/ref/ref-readonly alias RED/GREEN；inner/outer TypeId与source alias分离；local
  wrapper normalization；GCC/Clang focused及expanded验证；graph/interface fixed-marker复核；模块、
  计划和子里程碑记录。
- 未完成项目：Shared/Weak/AtomicShared全变体display gate、LSP alias consumer、source
  receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
