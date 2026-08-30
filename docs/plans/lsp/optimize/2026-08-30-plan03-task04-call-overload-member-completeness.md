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

# Plan 03 Task 4.15: Call Overload Member Completeness

## Goal

让 `CallCandidatesAt` 对 structured overload-set members 执行原子完整投影。query 不得静默跳过
缺失或 malformed member 后返回截断候选集。

## Contract

- overload-set 中每个 SymbolId 必须解析为 registered function 且具有有效 callable TypeId。
- 任一 member 不可投影时清空可复用输出并返回 `ZR_FALSE`。
- 重复的同一 SymbolId 仍按幂等 membership 处理，不制造重复 candidate。
- selected membership 与 overload-set row exactness 继续分别由 Task 4.13/4.14 门禁约束。

## RED/GREEN

新增 case 编译真实双重载调用，保留 selected member，把另一 member 改为 snapshot 中不存在的
SymbolId。旧 query 静默跳过该 row 并返回 selected-only；GCC call-query 真实 exit 1，
`17 Tests / 1 Failure`，唯一失败为 `Expected FALSE Was TRUE`。

GREEN 让 candidate append 报告结构化投影失败；overload loop 在任一 member 失败时清空整体
输出。GCC call-query 转为 `17/17`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：calls `17/17`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `17/17`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:56 +08:00。
- 状态：Task 4.15 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：missing overload member RED；atomic complete projection；duplicate idempotence；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
