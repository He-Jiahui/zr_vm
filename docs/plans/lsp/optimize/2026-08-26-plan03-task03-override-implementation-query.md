---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/acceptance/2026-08-26-plan03-task03-override-implementation-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.11: Override Implementation Query

## 状态与产出记录

- 完成时间：2026-08-26 12:16 +08:00。
- 状态：已完成。
- 完成项目：`ZrParser_SemanticQuery_ImplementationsOf` 反向查询同时投影
  canonical `IMPLEMENTATION` 与 `OVERRIDE` facts，保留各自 kind、source→target
  方向、node scope 过滤和稳定排序。查询只消费 relation graph，不按 member
  name、inheritance syntax、virtual slot 或 AST 重建实现关系。
- RED：MSVC direct relation suite 为 16 tests / 2 failures；手工 mixed facts
  只返回 1 条，source override 对 base method 的 `ImplementationsOf` 返回 false。
- 验证：MSVC direct relation 16/16、semantic query 30/30、symbols 19/19，均
  真实 exit 0；WSL GCC direct relation 16/16、WSL Clang 14 direct relation
  16/16，均真实 exit 0。
- 未完成边界：binary/native/external override producer、跨 snapshot/project
  generation 聚合、transitive override closure 与 LSP implementation consumer
  仍由后续子里程碑处理；缺少 exact relation fact 时必须保持 unavailable。
