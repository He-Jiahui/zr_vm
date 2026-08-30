---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_order.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_relation_source_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.16: Relation Stable Output Order

## Goal

让 relation graph 的只读 query 输出只由完整 structured identity 决定。没有 offset 的
line-only ranges 不得退化为 fact append order。

## Contract

- 排序先比较 relation kind、source/target SymbolId 与 TypeId，再比较 optional module identity。
- range 先比较 source identity；存在 offset 时比较完整 start/end offsets，否则比较完整
  start/end line 和 column。
- source/target range presence、external classification、external origin URI 与 virtual
  declaration URI 都进入稳定排序键。
- 完全相等的 rows 保持稳定；consumer 不按 AST、名称、URI 文本或 append order 补造语义。
- 排序实现位于独立内部模块，`semantic_relations.c` 保持 publisher/query orchestration 边界。

## RED/GREEN

新增 relation case 逆序 append 第3行和第2行的同 identity line-only facts。旧 comparator 只看
零 offsets，保留错误 append order；GCC relation-query 真实 exit 1，`22 Tests / 1 Failure`，
唯一失败为 `Expected 2 Was 3`。

GREEN 将完整比较和稳定 insertion sort 提取到 `semantic_relations_order.c/.h`，四个 relation
query 共用同一排序入口。GCC relation-query 转为 `22/22`、真实 exit 0；主 relation
orchestrator 从约912行降至约860行。

## Verification

- 固定 GCC/Ninja snapshot：relations `22/22`、semantic query `30/30`、symbols `21/21`、
  calls `14/14`、semantic-query parity `15/15`、source-contract `70/70`，全部真实 exit 0。
- 固定 Clang/Ninja snapshot：同一六目标分别为 `22/22`、`30/30`、`21/21`、`14/14`、
  `15/15`、`70/70`，全部真实 exit 0。
- GCC/Clang interface 均真实 exit 1；失败名称与固定 parent 的8个既有producer marker完全一致，
  delta 0，因此只作为无新增回归证据，不计 GREEN。
- 本阶段未运行 MSVC、完整三工具链16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-30 18:33 +08:00。
- 状态：Task 3.16 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：line-only ordering RED；complete structured relation order；共享四查询排序入口；
  大文件排序职责拆分；GCC/Clang六目标与interface marker复核；模块与计划记录。
- 未完成项目：Syntax05 imported declaration/property producer、external virtual declaration producer、
  semantic-token canonical migration、MSVC与完整16-target matrix、三套stdio smoke、Plan 03
  Task 8总门禁。
