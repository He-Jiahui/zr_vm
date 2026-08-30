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

# Plan 03 Task 4.17: Call Reference Conflict

## Goal

让 `ZrParser_SemanticQuery_CallAt` 拒绝同一 call target 上互相矛盾的 resolved target facts，
防止 query 用完整度或发布顺序掩盖 canonical producer 冲突。

## Contract

- 同一 selected call target 内、同一精确 reference range 的 resolved callable references 必须
  具有相同有效 SymbolId。
- 两个不同 resolved SymbolIds 使 query 清零输出并返回 `ZR_FALSE`。
- 不同 nested reference ranges 保持独立，不互相制造 target 冲突。
- 同一 SymbolId 的重复事实仍可补充 declaration range 或 `signatureDisplay`。
- 冲突不得通过 name、AST、signature text、range宽度或 append顺序消解。

## RED/GREEN

新增 case 在同一精确 call target 上发布两个不同 registered function SymbolIds。旧实现按相同
完整度保留首项并返回成功，GCC call-query 真实 exit 1，`19 Tests / 1 Failure`，唯一失败为
`Expected FALSE Was TRUE`。

GREEN 在 callable fact 校验后执行 resolved SymbolId 一致性检查；发现不同目标立即 fail closed，
输出保持预清零。反向用例随后证明全 target 容器级冲突会误拒绝不同 nested ranges，旧中间实现
真实 exit 1、`20 Tests / 1 Failure`、唯一失败为 `Expected TRUE Was FALSE`。最终 GREEN 将
一致性门禁限定到完整 source/offset/line/column 相同的 reference range。该单遍实现随后又由
更高完整度的后续同 range 冲突 fact 触发 `20 Tests / 1 Failure`、`Expected FALSE Was TRUE`：
选择切换后没有回看较早事实。最终实现先完成稳定选择，再第二遍扫描 selected exact range 的所有
callable facts；GCC call-query 转为 `20/20`、真实 exit 0。

## Verification

- 固定 `1e8584c + 3 overlays` GCC/Ninja snapshot：calls `20/20`、semantic query `30/30`、
  relations `22/22`、symbols `21/21`、semantic-query parity `15/15`、source-contract `70/70`，
  全部真实 exit 0。
- 同一字节 Clang/Ninja snapshot：同一六目标分别为 `20/20`、`30/30`、`22/22`、`21/21`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 21:52 +08:00。
- 状态：Task 4.17 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：conflicting resolved target RED；nested-range boundary RED；refinement-order RED；
  two-pass exact-range SymbolId consistency fail-closed gate；输出预清零；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、argument-to-parameter mapping、
  binary/native call-edge producer、semantic-token canonical migration、MSVC与完整16-target matrix、
  三套stdio smoke、Plan 03 Task 8总门禁。
