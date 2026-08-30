---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_generic_alias_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 5.10: Generic Type-Use Alias Range

## Goal

为generic type use发布包含closing angle的authoritative whole-use range，使canonical TypeId文本与
source alias保持分离，并在nested `>>`场景保留每层generic use的精确边界。

## Contract

- `SZrGenericType.wholeRange`从generic name起点延伸到其exact closing angle。
- nested `Box<Box<i64>>`的outer/inner range分别结束于各自的closing angle；`>>`拆分不合并边界。
- legacy generic `SZrAstNode.location`保持不变，避免改变既有reference producer合同。
- alias仅在structured generic inference形成canonical TypeId后按wholeRange发布；不切source文本猜边界。
- `Box<i64>` canonical display为`Box<int>`，exact use-site alias为`Box<i64>`。
- wholeRange缺失、为空或source identity不一致时fail closed，不改变inference、TypeId或diagnostic。

## RED/GREEN

RED为`Box<i64>`查询exact whole-use alias。旧generic node location从argument起点开始，期望offset 11、
实际offset 15；GCC display真实exit 1，`15 Tests / 1 Failure`。

初版直接扩大generic node location后display转GREEN，但expanded interface新增
`LSP Closed Generic Type Display And Definition`失败marker，证明该location仍被既有reference producer
消费，方案被拒绝。最终parser单独保存wholeRange，alias producer仅消费该sidecar；legacy node location
恢复原合同。`Box<i64>`与nested `Box<Box<i64>>` outer/inner exact alias均GREEN，display为`16/16`。

## Verification

- GCC/Clang固定snapshot均通过parser `74/74`、display `16/16`、calls `26/26`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts
  `15/15`、canonical consumers `21/21`、type inference `124/124`，全部真实exit 0。
- canonical graph在两套工具链均真实exit 1，仅保留同一既有
  `test_tuple_ast_projects_to_tuple_type_id` marker，delta 0且不计GREEN。
- interface在两套工具链均真实exit 1，最终失败集合严格恢复fixed parent同一8个producer marker，
  delta 0且不计GREEN；被拒绝的node-location初版曾产生第9个closed-generic marker。
- Clang首次增量构建因`wsl.exe`压缩`-S/-B`参数触发两次configure timeout；显式configure后直接
  Ninja重建并取得上述真实exit证据。未残留本任务进程。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 04:50 +08:00。
- 状态：Task 5.10 generic type-use alias range GREEN；Plan 03 Task 5/Task 7/Task 8总门禁仍进行中。
- 完成项目：generic wholeRange AST sidecar；nested split-angle exact range；canonical/source display
  分离；legacy generic node-location兼容；RED/被拒绝方案/最终GREEN闭环；GCC/Clang focused及expanded
  验证；graph/interface fixed-marker复核；模块、计划和子里程碑记录。
- 未完成项目：const-expression/nominal/GcBridge source alias producer、LSP alias consumer、owner全变体
  display gate、source receiver/member argument mapping、receiver `TypeId`、`.zro`/native callable mapping
  parity、Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、三套stdio smoke、
  Plan 03 Task 8总门禁。
