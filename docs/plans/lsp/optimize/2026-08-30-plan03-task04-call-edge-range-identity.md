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

# Plan 03 Task 4.11: Call Edge Range Identity

## Goal

让 repeated call-edge publication 按完整可用 callsite range 合并。合法的 line-only facts 不得
因为 start/end offsets 都是零而被视为同一调用点。

## Contract

- source identity 继续使用 Task 4.10 的 exact optional identity。
- 任一 range 提供 offset 时，start/end offsets 是该 snapshot 的 authoritative coordinate key。
- 双方均未提供 offset 时，start/end line 与 column 全部参与 identity。
- 同 caller、同 target 但不同 line-only callsite 必须保留为不同 outgoing/incoming edge。
- 同一 exact callsite 的 unresolved-to-resolved refinement 与 closed callable refresh 继续合并。

## RED/GREEN

新增 lower-layer case，在同一 function scope 内发布两个 source 相同、target 相同但位于第2行和
第3行的 resolved call facts；两条 fact 均只有 line/column。旧 merge 只比较 offsets，GCC
call-query 真实 exit 1，`14 Tests / 1 Failure`，唯一失败为 `Expected 2 Was 1`。

生产修复新增完整 range equality，并仅替换 merge 的 callsite identity 判断。GCC call-query
转为 `14/14`、真实 exit 0；既有 resolved refinement、source fail-closed 与 normal offset
callsite cases全部保持通过。

## Verification

- 固定 GCC/Ninja snapshot：calls `14/14`、semantic query `30/30`、relations `20/20`、
  semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一五目标分别为 `14/14`、`30/30`、`20/20`、`15/15`、
  `70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:11 +08:00。
- 状态：Task 4.11 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：line-only callsite RED；offset/line-column dual range identity；多调用点保留；
  GCC/Clang五目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、binary/native call-edge producer、
  semantic-token canonical migration、MSVC与完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
