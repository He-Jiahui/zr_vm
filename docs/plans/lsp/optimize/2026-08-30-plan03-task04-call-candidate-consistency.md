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

# Plan 03 Task 4.13: Call Candidate Consistency

## Goal

让 resolved `CallAt` target 与 `CallCandidatesAt` overload membership 保持原子一致。query 不得
返回一个没有任何 `isSelected` row 的非空候选集合。

## Contract

- `CallCandidatesAt` 仍只消费 exact call fact、resolved target SymbolId 和 structured
  overload-set members。
- selected target 必须是输出候选之一，且由 SymbolId 精确匹配，不按名称、位置或 callable text补入。
- overload-set 未包含 selected target 时清空可复用输出并返回 `ZR_FALSE`。
- 正常单函数与完整 overload set 的排序、declaration TypeId/range、closed call TypeId保持不变。

## RED/GREEN

新增 case 编译真实双重载调用，保留 resolved target fact，但把 snapshot overload members 全部改为
另一有效 candidate。旧 query 返回一个非空、零 `isSelected` 的集合；GCC call-query 真实
exit 1，`15 Tests / 1 Failure`，唯一失败为 `Expected FALSE Was TRUE`。

GREEN 在 structured candidates 投影结束后验证 selected SymbolId membership；缺失时清空输出并
fail closed。GCC call-query 转为 `15/15`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：calls `15/15`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `15/15`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:44 +08:00。
- 状态：Task 4.13 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：inconsistent overload-set RED；selected membership fail-closed；reused output清空；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
