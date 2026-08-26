---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/acceptance/2026-08-26-plan03-task03-source-constructor-relations.md
doc_type: milestone-record
---

# Plan 03 Task 3.8: Source Constructor Relations

## 状态与产出记录

- 完成时间：2026-08-26 09:01 +08:00。
- 状态：已完成。
- 完成项目：在 source scope facts 已注册类型符号之后，仅从 compiler-owned
  `typePrototypes` 中的 exact type declaration node 和显式 `@constructor`
  member 的 canonical `SymbolId` 发布 `CONSTRUCTOR` relation。关系模块只按
  `symbol.astNode` 找到 source type endpoint，并由已注册 symbol record 投影
  endpoint `TypeId` 与 declaration range；缺失 id、type、range 或显式构造器
  identity 时 fail closed。合成默认构造器没有 source member identity，不发布
  relation。没有按名称、constructor token、TypeId-only、AST traversal 或 LSP
  状态重建关系。
- 验证：isolated MSVC direct process exits 0：relation 14/14、semantic query
  30/30、symbols 19/19、canonical consumers 19/19、compiler diagnostics
  46/46、compiler integration 127/127。isolated WSL GCC direct relation test
  14/14，exit 0。
- 未完成边界：binary/native/imported constructor provenance、synthesized
  constructor identity 和 LSP relation consumer 仍须由后续 canonical
  producer/consumer milestone 处理；没有完整 stable source endpoint 时保持
  unavailable。
