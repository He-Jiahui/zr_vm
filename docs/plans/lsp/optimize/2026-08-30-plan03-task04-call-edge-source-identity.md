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

# Plan 03 Task 4.10: Call Edge Source Identity

## Goal

让 call graph 的 source identity 与 Plan 03 snapshot exactness 一致。缺失 source 的 call fact
可以保留为不可归属的 edge，但不得把已知文档 scope 或 query position 当作同一 source，亦不得
在相同 offsets 下与 source-bound edge 合并。

## Contract

- 两个缺失 source 相等，支持单 snapshot 中明确无 source 的 lower-layer facts。
- 两个非空 source 通过字符串值比较，允许不同 `SZrString` 实例表达同一 source identity。
- 一个缺失 source 与一个已知 source 永不相等。
- source 不匹配时 caller scope selection、expression association、node scope、`CallEdgesAt`
  和 callsite refinement 全部 fail closed。
- 不通过 caller name、callee name、AST 文本或相同 offsets 补造 source identity。

## RED/GREEN

新增 lower-layer case：函数 scope 带已知 source，而 resolved call reference 的 source 缺失。
旧实现把任一 `NULL` 当通配符，错误地把 edge 归属到该函数；GCC call-query 真实 exit 1，
`13 Tests / 1 Failure`，唯一失败为 `Expected 0 Was 1`。

生产修复只收紧 `semantic_calls_same_source`。修复后 source-bound outgoing query 和位置 query
均返回空；使用同样缺失 source 的位置仍可读取一条 `CALLER_UNAVAILABLE` edge。GCC call-query
转为 `13/13`、真实 exit 0。

## Verification

- 固定 GCC/Ninja snapshot：calls `13/13`、semantic query `30/30`、relations `20/20`、
  semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一五目标分别为 `13/13`、`30/30`、`20/20`、`15/15`、
  `70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:05 +08:00。
- 状态：Task 4.10 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：source-identity RED；exact optional source equality；caller/query/refinement
  fail-closed；GCC/Clang五目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、binary/native call-edge producer、
  semantic-token canonical migration、MSVC与完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
