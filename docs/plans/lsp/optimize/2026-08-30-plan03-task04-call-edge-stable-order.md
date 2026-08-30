---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_edge_refinement_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.12: Call Edge Stable Order

## Goal

让 `CallEdgesAt`、`OutgoingCalls` 与 `IncomingCalls` 的 copied edge 顺序只由 canonical source、
range 和 stable ids决定，不受 producer append顺序影响；line-only facts必须使用line/column排序。

## Contract

- source identity 先按缺失/非缺失与字符串值确定稳定顺序。
- 任一 range 提供 offset 时，依次比较 start/end offsets。
- 双方均未提供 offset 时，依次比较 start line/column、end line/column。
- range 相同时再比较 caller SymbolId、target SymbolId 和 structured resolution。
- query只排序 copied values，不修改 snapshot facts，也不读取callee name、AST或display文本。

## RED/GREEN

Task 4.11 的两个 line-only facts 改为先发布第3行、后发布第2行，并要求 outgoing query 按
第2行、第3行返回。旧 comparator 只比较 start offset；两者均为零且其余ids相同，结果保留
append顺序。GCC call-query真实exit 1，`14 Tests / 1 Failure`，唯一失败为
`Expected 2 Was 3`。

生产修复增加 source-aware range comparator，并在既有 insertion sort 中先比较完整range。
GCC call-query转为`14/14`、真实exit 0；多callsite保留、resolved refinement与source
fail-closed cases继续通过。

## Verification

- 固定 GCC/Ninja snapshot：calls `14/14`、semantic query `30/30`、relations `20/20`、
  semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一五目标分别为 `14/14`、`30/30`、`20/20`、`15/15`、
  `70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:18 +08:00。
- 状态：Task 4.12 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：逆序producer RED；source/range deterministic comparator；line-only稳定顺序；
  GCC/Clang五目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、binary/native call-edge producer、
  semantic-token canonical migration、MSVC与完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
