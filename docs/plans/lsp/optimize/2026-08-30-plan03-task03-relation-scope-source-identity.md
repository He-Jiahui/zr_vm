---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_relation_source_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.15: Relation Scope Source Identity

## Goal

让 relation graph 的 node-scope range filter 保持 snapshot source exactness。缺失 source 的
relation endpoint 不得仅凭重叠坐标进入已知文档的 node scope。

## Contract

- 两个缺失 source 相等，支持明确 source-unavailable 的 lower-layer relation facts。
- 两个非空 source 按字符串值比较，允许不同 `SZrString` 实例表达同一 identity。
- 一个缺失 source 与一个已知 source 永不相等。
- source 或 target endpoint 至少一条在同 source 的 node range 内，relation 才可进入 scoped
  `RelationsOfSymbol` / `ImplementationsOf` 结果。
- module-scope relation query 不受 node containment 约束，且不按 symbol name 或 URI text补造范围。

## RED/GREEN

新增独立 case header，append 一条 source/target endpoint 均缺失 source、但 offsets 落在
known-source root 内的 alias relation。旧 containment 把 `NULL` 当通配符，第一次 scoped
query错误返回true；GCC relation-query真实exit 1，`21 Tests / 1 Failure`，唯一失败为
`Expected FALSE Was TRUE`。

生产修复只收紧 `semantic_relations_range_contains` 的 source equality。known-source root 查询
转为空；将同一 root source 置为缺失后仍能读取一条 relation。GCC relation-query 转为
`21/21`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：relations `21/21`、semantic query `30/30`、symbols `21/21`、
  calls `14/14`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `21/21`、`30/30`、`21/21`、`14/14`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:23 +08:00。
- 状态：Task 3.15 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：relation-scope source RED；exact optional source containment；known/missing双向边界；
  GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、external virtual declaration producer、
  semantic-token canonical migration、MSVC与完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
