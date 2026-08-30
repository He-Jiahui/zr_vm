---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
tests:
  - tests/parser/test_canonical_consumers.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.21: Constructor Argument Mapping

## Goal

把 Task 4.20 的 canonical argument mapping 扩展到 source class/struct constructor，保持
constructor signature、target identity、argument binding与conversion来自同一parser snapshot。

## Contract

- class `new` 与 struct `init` 复用同一 `SZrSemanticCallArgumentFact` row，不建立constructor特例API。
- source parameter declaration/metadata name决定named mapping；不从formatter text或LSP symbol table配对。
- constructor path缺少exact argument expression fact时，只在parser producer阶段调用canonical expression
  inference；随后必须通过resolved parameter compatibility才发布conversion。
- query仍借用snapshot-owned array；UNKNOWN/invalid payload继续清零并fail closed。

## RED/GREEN

首个RED在既有canonical constructor fixture增加mapping断言：GCC真实exit 1，`21 Tests / 1 Failure`，
class query的`argumentMappings`为NULL。接入builder后第二级RED仍为`21/1`，失败前移到`CallAt == false`；
GDB证明row的index/range正确，但argument/parameter `TypeId`均为0、conversion为UNKNOWN。根因是
non-generic constructor只复制signature，不推断实参。

GREEN让builder仅在exact expression fact缺失时执行parser expression inference；constructor调用选择
compatibility gate。class `new Hero(42)`对canonical `double`参数发布implicit row，struct
`init Point(y: 2, x: 1)`发布两个named exact reorder rows和精确argument ranges。

## Verification

- GCC/Clang固定snapshot均通过canonical consumers `21/21`、calls `25/25`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、parity `15/15`、source-contract `70/70`、facts `15/15`，
  全部真实exit 0。
- type-inference目标因共享binary-import临时artifact先作废并行轮；随后GCC、Clang独占串行均
  `124/124`、真实exit 0。
- GCC/Clang interface均真实exit 1；失败名称与固定parent的8个既有producer marker完全一致，
  delta 0，不计GREEN。
- 本阶段未运行MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 00:28 +08:00。
- 状态：Task 4.21 focused GREEN；Plan 03 Task 4/Task 7/Task 8总门禁仍进行中。
- 完成项目：class implicit conversion mapping；struct named reorder exact mapping；缺失argument fact的
  parser producer inference；constructor compatibility gate；双工具链focused/type-inference/interface
  marker复核；模块与计划记录。
- 未完成项目：receiver/member mapping、receiver `TypeId`、`.zro`/native callable mapping parity、
  unresolved reason、Syntax05 imported property/declaration producer、MSVC、完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
