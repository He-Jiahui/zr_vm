---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_expression_refinement_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.19: Call Expression Refinement

## Goal

让 `ZrParser_SemanticQuery_CallAt` 的 duplicate expression-fact refinement 与 exactness 单调一致，
防止后发布 approximate fact 降级同 range 的 exact call。

## Contract

- containing call expression 仍先按最窄 range 选择。
- range等宽时 `EXACT` fact 优先于 non-exact fact，与 append顺序无关。
- width与exactness都相同时保留首个稳定发布项。
- `CallAt` 仍可借用 approximate fact；只有 value-only `FormatCall`/`CallCandidatesAt` fail closed。

## RED/GREEN

新增 fixture 在同一精确 call range 先发布 `EXACT` expression，再发布 `APPROXIMATE` expression。
旧 `width <= bestWidth` 选择后项，GCC call-query 真实 exit 1，`24 Tests / 1 Failure`，唯一失败为
selected expression pointer不一致，且后续 canonical format无法使用该 downgraded fact。

GREEN 以 width 为第一顺序、exactness为等宽refinement、首项为最终tie-break。GCC call-query
转为 `24/24`、真实 exit 0，`FormatCall` 保持 canonical display。

## Verification

- 固定 GCC/Ninja snapshot：calls `24/24`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 同一字节 Clang/Ninja snapshot：同一六目标分别为 `24/24`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 22:03 +08:00。
- 状态：Task 4.19 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：exact-then-approximate RED；width/exactness stable selection；canonical format保持；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
