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

# Plan 03 Task 4.9: Call Edge Refinement Merge

## Goal

让同一 caller 与 exact callsite 在多轮 inference/reference producer 中只保留最完整的
canonical call edge。resolved identity 必须取代旧 unresolved edge，后续不完整 fact 不得降级
已有结果；合并不使用 callee name、display signature 或 AST 文本。

## Contract

- logical edge slot 由 caller SymbolId 与 exact call-site source/range 决定。
- completeness 只读取 caller/target SymbolId、target declaration range、callable TypeId 与
  structured resolution；resolved edge 的优先级最高。
- 同一 resolved target 可用后续 fact 刷新 closed callable TypeId 和 declaration range。
- unresolved fact 不得覆盖已有 resolved edge；重复 publish 保持单条结果。
- 同 callsite 的不同 resolved target SymbolId 仍分别保留，不静默掩盖矛盾 canonical producer。

## RED/GREEN

独立 parser case 先在同一 callsite append unresolved reference，再 append resolved target。
旧实现把 resolution 纳入 dedupe key，真实 exit 1，`12 Tests / 1 Failure`，唯一失败为
`Expected 1 Was 2`。生产层改为 completeness merge 后同一目标 `12/12`、真实 exit 0。
测试随后追加 closed callable TypeId refinement 和末尾 unresolved fact，重复 publish 仍仅返回
一条 resolved edge，并投影刷新后的 callable TypeId。

## Verification

- 固定 GCC/Ninja snapshot：calls `12/12`、semantic query `30/30`、relations `20/20`、
  semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一五目标分别为 `12/12`、`30/30`、`20/20`、`15/15`、
  `70/70`，全部真实 exit 0。
- GCC/Clang interface 均为 `105 Pass / 8` 个既有 producer marker；失败名称与固定 parent
  完全一致，delta 0。interface 真实 exit 1，因此不计 GREEN。
- Clang build 仅保留 Syntax05-owned `test_lsp_interface.c` 的既有 `providerPhase` initializer
  warning；本阶段不修改该路径。
- 本阶段未运行 MSVC、完整三工具链 16-target matrix 或三套 stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 17:56 +08:00。
- 状态：Task 4.9 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：callsite refinement RED；结构化 completeness merge；resolved 防降级；closed
  callable refresh；重复 publish 幂等；GCC/Clang 五目标与 interface marker 复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、binary/native call-edge producer、
  semantic-token canonical migration、MSVC 与完整 16-target matrix、三套 stdio smoke、Plan 03
  Task 8 总门禁。
