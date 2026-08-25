---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: milestone-record
---

# Plan 03 Task 3.1: Relation Graph Foundation

## Goal

Publish a snapshot-owned, read-only relation graph carrier before adding any
compiler or LSP relation recovery. Relations must identify both endpoints by
stable SymbolId and TypeId, retain exact ranges where they exist, and represent
external origins explicitly.

## Implementation

- Adds `SZrSemanticRelationFact` and `relationFacts` lifecycle ownership to
  `SZrSemanticContext`.
- Supports declaration/definition, override, implementation, base type,
  constructor, property accessor, alias target, and import/export-origin
  relation kinds.
- Adds `RelationsOfSymbol`, `ImplementationsOf`, `BaseTypesOf`, and
  `DerivedTypesOf`. Query results are copied values whose URI is borrowed from
  the snapshot, and reused output arrays are cleared before use.
- Sorts projection by relation kind, stable ids, and ranges. A node scope only
  exposes relations with an endpoint in the scope.

## Exclusions

This foundation does not yet publish compiler-produced source inheritance,
implementation, property, alias, or imported artifact edges. It does not
construct relationships by AST scans, names, type text, or LSP fallback, and it
does not complete Plan 03 Task 3.

## Verification

- RED: the new standalone relation target failed to compile because
  `semantic_relations.h` did not exist.
- GREEN: isolated MSVC build and direct execution report `3 Tests 0 Failures
  0 Ignored`. Adjacent direct tests report semantic query `29/0`, semantic-query
  symbols `19/0`, and semantic query contract `3/0`, all with process exit zero.

## 状态与产出记录

- 完成时间：2026-08-25 23:24 +08:00。
- 状态：已完成并随本提交精确提交；不声明 Plan 03 Task 3 完成。
- 完成项目：snapshot relation fact storage、external origin URI、稳定关系排序、
  symbol/type/implementation relation queries、node-scope isolation测试。
- 后续项目：compiler source/base/interface/property/alias producers、binary/native
  external origin projection、call graph 与 LSP relation consumers。
