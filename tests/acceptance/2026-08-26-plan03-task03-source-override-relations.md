---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.7 Source Override Relations

The source fixture compiles an abstract `Base.ping` and an exact
`Derived.ping` override, then queries the derived member's stable SymbolId. It
requires one `OVERRIDE` edge to the base member with both canonical TypeIds and
both declaration ranges. The test never searches a member name or asks an LSP
consumer to recover an edge.

## 状态与产出记录

- 完成时间：2026-08-26 08:27 +08:00。
- 状态：已完成。
- 完成项目：direct isolated MSVC process exits 0：relation 12/12, semantic
  query 30/30, symbols 19/19, canonical consumers 19/19, compiler diagnostics
  46/46；compiler integration 日志为 127/127、0 failures。
- 未完成边界：binary/native/external override target identity、property/accessor
  override coverage 和 LSP relation projection 仍需独立 producer/consumer
  milestone；不存在 stable target SymbolId 时不发布 local relation。
