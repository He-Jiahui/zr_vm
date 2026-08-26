---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/acceptance/2026-08-26-plan03-task03-external-relation-locations.md
doc_type: milestone-record
---

# Plan 03 Task 3.13: External Relation Locations

## 状态与产出记录

- 完成时间：2026-08-26 14:01 +08:00。
- 状态：已完成；不声明 Plan 03 Task 3 完成。
- 完成项目：`SZrSemanticRelationFact` 与
  `SZrParserSemanticRelationQuery` 新增 snapshot-owned
  `virtualDeclarationUri`。relation append 同时克隆 external origin 与
  virtual declaration URI；external fact 在没有 source range 时必须由
  metadata projection 同时提供两者，否则原子失败且不写 relation store。
  query 只投影 fact，不从 module text、symbol name、source path 或 LSP
  virtual-document scheme 构造 URI。
- RED：MSVC relation target 在新增 contract test 首次编译时因 fact/query
  均缺少 `virtualDeclarationUri` 失败；实现后一次测试修正移除了对 interned
  string 地址不等的错误假设。
- 验证：direct process exits 0。MSVC relation 18/18、semantic query 30/30、
  symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46；
  WSL GCC relation 18/18；WSL Clang14 relation 18/18。
- 未完成边界：binary/native metadata producer 尚未把具体 virtual URI 写入
  relation fact；有本地 alias source range 的 source import relation仍只携带
  external origin。LSP consumer 不得在 producer 完成前自行构造 URI。
