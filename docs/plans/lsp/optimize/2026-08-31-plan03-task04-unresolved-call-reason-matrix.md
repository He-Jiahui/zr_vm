---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_unresolved_reason_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.25: Unresolved Call Reason Matrix

## Goal

冻结无法完整解析的call edge结构化reason，证明query不会从同名函数、AST或文本替换缺失target。

## Contract

- 未解析reference或resolved id不存在/不是function时，target id清零并返回
  `TARGET_UNRESOLVED`。
- target function identity有效但没有精确declaration range时，保留target SymbolId并返回
  `TARGET_DECLARATION_UNAVAILABLE`，`hasTargetDeclarationRange=false`。
- caller scope缺失使用`CALLER_UNAVAILABLE`；已有测试继续冻结该边界。
- 同名function registry row不参与target选择，incoming/outgoing query只消费call-edge stable ids。

## Characterization

新增两条fact-only测试：第一条发布有效target function但不提供任何declaration coordinates，验证
outgoing和incoming query保留同一target identity并返回declaration-unavailable reason；第二条把
resolved reference指向variable SymbolId，同时注册同名function，验证publisher清零target并返回
target-unresolved reason。

现有production首轮即满足合同，GCC calls为`28 Tests / 0 Failures`，因此本项没有制造生产补丁。

## Verification

- GCC/Clang固定独立snapshot均通过 parser/display/calls/query/relations/symbols/parity/
  source-contract/facts/canonical/type-inference
  `74/22/28/30/22/21/15/70/15/21/124`，真实exit 0。
- 两套type-inference串行运行，均为`124/124`。
- GCC/Clang interface均真实exit 1，失败集合精确保持fixed parent同一8个既有producer marker，
  delta 0且不计GREEN。
- 本阶段未运行MSVC、完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 07:55 +08:00。
- 状态：Task 4.25 characterization GREEN；Plan 03 Task 4/Task 7/Task 8总门禁仍进行中。
- 完成项目：target-unresolved reason；target-declaration-unavailable reason；non-function identity
  rejection；same-name no-fallback；incoming/outgoing stable-id projection；GCC/Clang expanded gate；
  interface fixed-marker复核；模块与计划记录。
- 未完成项目：receiver/member mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  Syntax05 imported declaration/property producer、MSVC、完整16-target matrix、三套stdio smoke、
  Plan 03 Task 8总门禁。
