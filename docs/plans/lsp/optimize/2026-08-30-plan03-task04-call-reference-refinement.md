---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_selection_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.16: Call Reference Refinement

## Goal

让 `ZrParser_SemanticQuery_CallAt` 在同一 call target 存在多条 callable reference facts 时，
按 canonical identity 完整度选择事实，避免较早的展示文本遮蔽后续已解析目标。

## Contract

- `isResolved` 且具有有效 SymbolId 的 reference 优先于 unresolved display-only reference。
- declaration range 与 `signatureDisplay` 只增加同级事实完整度，文本不得超过 resolved identity。
- 相同完整度保留稳定发布顺序。
- query 不从 member/function name、AST 或 signature text 恢复目标身份。

## RED/GREEN

新增 case 在同一精确 call target 上先发布带 `signatureDisplay` 的 unresolved fact，再发布具有
SymbolId 与 declaration range 的 resolved fact。旧实现遇到首条 display fact 立即停止，GCC
call-query 真实 exit 1，`18 Tests / 1 Failure`，唯一失败为 `Expected TRUE Was FALSE`。

GREEN 使用结构化完整度扫描全部 callable references；resolved identity 取代较早 display-only
fact。GCC call-query 转为 `18/18`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：calls `18/18`、semantic query `30/30`、relations `22/22`、
  symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `18/18`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 19:14 +08:00。
- 状态：Task 4.16 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：display-first RED；resolved call-reference completeness selection；稳定同级选择；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
