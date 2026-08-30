---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_super_constructor_semantic_facts.c
tests:
  - tests/parser/test_canonical_consumers.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.22: Super Constructor Argument Mapping

## Goal

让source `super(...)`与普通source constructor共用canonical argument mapping，并保证可选refinement
不完整时不会破坏既有base-constructor signature/navigation contract。

## Contract

- `super(...)` mapping绑定resolved base-constructor SymbolId、declaration range和closed callable TypeId。
- argument TypeId、parameter TypeId、passing mode与conversion来自parser producer，不从derived
  constructor名称、formatter text或LSP状态恢复。
- mapping是原子可选refinement：任一row为UNKNOWN/invalid时producer清空整个array。
- array缺失时`CallAt`仍返回基础call contract；non-empty malformed array继续由query fail closed。

## RED/GREEN

RED把base constructor参数设为source `float`（canonical `double`），derived `seed`保持`int`，并在
`super(seed)`断言`arg0 -> param0` implicit row及精确`seed` range。GCC canonical consumers真实
exit 1、`21 Tests / 1 Failure`，唯一失败为mapping NULL。

接入builder后canonical和focused门禁转绿，但GCC/Clang interface各新增
`LSP Signature Help Resolves Super Constructor`失败。support-first定位为LSP analyzer snapshot中参数
TypeId尚不可证明，UNKNOWN row使`CallAt`拒绝整个call。最终GREEN由producer原子清空不完整mapping，
super signature case恢复PASS；完整compiler snapshot仍发布implicit row。

## Verification

- GCC/Clang固定snapshot均通过canonical consumers `21/21`、calls `25/25`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`，
  全部真实exit 0。
- GCC、Clang type-inference独占串行均`124/124`、真实exit 0。
- 两套interface中super signature case均PASS；进程真实exit 1且只剩固定parent 8个既有producer
  marker，delta 0，不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 00:45 +08:00。
- 状态：Task 4.22 focused GREEN；Plan 03 Task 4/Task 7/Task 8总门禁仍进行中。
- 完成项目：super implicit mapping；base-constructor identity保持；atomic optional mapping producer；
  analyzer snapshot兼容回归；双工具链focused/type-inference/interface marker复核；模块与计划记录。
- 未完成项目：receiver/member mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  unresolved reason、Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
