---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_candidate_consistency_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.14: Call Overload-Set Exactness

## Goal

让 `CallCandidatesAt` 区分“声明没有 overload set”和“声明引用的 overload-set row 缺失”。
后者是 snapshot 不一致，不能降级为 selected-only 结果。

## Contract

- `overloadSetId == INVALID` 表示明确的 single-candidate 调用，可投影 resolved selected symbol。
- non-invalid `overloadSetId` 必须精确命中 snapshot overload-set row。
- row 缺失时清空可复用输出并返回 `ZR_FALSE`，不按名称搜索另一 set，也不忽略该 id。
- row 存在时继续执行 Task 4.13 的 selected membership 原子校验。

## RED/GREEN

新增 case 编译真实双重载调用，再把 selected symbol 的 non-invalid `overloadSetId` 改为 snapshot
中不存在的 id。旧 query 将 `overloads == NULL` 当作无重载并返回 selected-only；GCC call-query
真实 exit 1，`16 Tests / 1 Failure`，唯一失败为 `Expected FALSE Was TRUE`。

GREEN 在 structured row lookup 后立即拒绝 non-invalid id 的 lookup miss。GCC call-query 转为
`16/16`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：calls `16/16`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `16/16`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:49 +08:00。
- 状态：Task 4.14 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：missing overload-set RED；single/missing set语义区分；lookup miss fail-closed；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
