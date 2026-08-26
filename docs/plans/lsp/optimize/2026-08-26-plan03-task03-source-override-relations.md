---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: milestone-record
---

# Plan 03 Task 3.7: Source Override Relations

## 状态与产出记录

- 完成时间：2026-08-26 08:27 +08:00。
- 状态：已完成。
- 完成项目：class override validation 在 source/target member 均已注册
  canonical `SymbolId` 后发布 `OVERRIDE` fact；endpoint 的 `TypeId` 和 exact
  declaration range 只从现有 symbol record 投影。`PublishSymbolRelation`
  只接受 stable ids，缺失 identity、type 或 range 时 fail closed；不会按成员
  名称、virtual slot、property accessor spelling、AST traversal 或 LSP 状态补边。
- 验证：isolated MSVC direct process exits 0：relation 12/12、semantic query
  30/30、symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46。
  既有 compiler integration 日志为 127/127、0 failures。
- 未完成边界：source property/accessor override 的 relation coverage、alias
  chain，以及 binary/native/external override provenance 和 LSP consumer 仍需
  由对应 canonical producer 发布；缺失 target identity 时保持 unavailable。
