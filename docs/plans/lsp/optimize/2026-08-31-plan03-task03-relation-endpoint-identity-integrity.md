---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_relation_endpoint_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.17: Relation Endpoint Identity Integrity

## Goal

禁止缺少source或target canonical identity的单边relation进入semantic snapshot，确保consumer
不需要按名称、range或URI补造另一个endpoint。

## Contract

- source和target各自至少提供一个有效`SymbolId`或`TypeId`。
- symbol-to-symbol override/implementation、symbol-to-type alias/import以及type-to-type base edge
  均继续合法。
- 只有source或只有target的row在append前fail closed，relation store保持不变。
- relation kind、URI、range、stable order、scope与idempotence合同不变。

## RED/GREEN

- RED：既有22项全通过；新增source-only override edge被错误接受，relation target为
  `23 Tests / 1 Failure`，真实exit 1。
- GREEN：公共append增加endpoint identity predicate；GCC/Clang focused均`23/23`、真实exit 0。

## Verification

- 固定GCC snapshot：relations/query/symbols/calls/parity/source-contract
  `23/30/21/28/15/70`，全部真实exit 0。
- 固定Clang snapshot：同一六目标`23/30/21/28/15/70`，全部真实exit 0。
- 本子里程碑未运行MSVC、完整16-target matrix或三套stdio smoke。

## 状态与产出记录

- 完成时间：2026-08-31 08:19 +08:00。
- 状态：Task 3.17 focused GREEN；Plan 03总体仍进行中。
- 完成项目：单边edge RED；双endpoint structured identity门禁；合法symbol/type组合回归；
  GCC/Clang六目标验证；module contract；Task3前两项checkbox对齐。
- 未完成项目：external/virtual metadata producer；跨provider generation矩阵；Task4
  receiver/member与binary/native parity；Task7其余consumer；Task8总验收。
