---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_source_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.18: Call Source Identity

## Goal

让 `ZrParser_SemanticQuery_CallAt` 只消费与请求文档精确同源的 expression、call-target 与
callable reference facts，禁止 source-less fact 跨文档匹配有明确 source 的请求。

## Contract

- request position 与 selected call expression 使用 exact optional source identity。
- selected expression 与其 call-target range 使用 exact optional source identity。
- call-target range 与 callable reference 使用 exact optional source identity。
- 两侧 source 都缺失仍可相等；仅一侧缺失时不是通配符，query 清零并 fail closed。
- 本项不改变其他 semantic query API 的历史 containment 合同。

## RED/GREEN

新增三个 fixture，分别移除 call expression、callable reference 和 call-target range 的 source，
同时保留有明确 source 的请求。旧 `canonical_query_same_source` 将 `NULL` 视为通配符，GCC
call-query 真实 exit 1，`23 Tests / 3 Failures`，三项均为 `Expected FALSE Was TRUE`。

GREEN 在 `CallAt` 的三层选择边界使用 exact optional source helper，且保持全局 contains helper
不变。GCC call-query 转为 `23/23`、真实 exit 0。

## Verification

- 固定 `1e8584c + Task 4.17/4.18 overlays` GCC/Ninja snapshot：calls `23/23`、semantic query
  `30/30`、relations `22/22`、symbols `21/21`、semantic-query parity `15/15`、source-contract
  `70/70`，全部真实 exit 0。
- 同一字节 Clang/Ninja snapshot：同一六目标分别为 `23/23`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 21:58 +08:00。
- 状态：Task 4.18 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：expression/reference/callTarget source-less RED；exact optional source helper；
  三层 CallAt fail-closed gate；GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
