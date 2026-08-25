---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: milestone-record
---

# Plan 03 Task 3.4: Source Import Origin Relations

## Goal

Publish source import/export origin relation facts from compiler-owned scope
facts. An LSP consumer must never recover an import target by matching the
local alias spelling to a native or source symbol.

## Implementation

- Extends `SZrSemanticVisibleSymbolFact` with a snapshot-owned
  `externalOriginUri` copied by the semantic context publisher.
- Source-scope construction records the normalized URI from each canonical
  `import(...)` expression for direct and destructured import aliases.
- Adds `ZrParser_SemanticRelations_PublishImportOrigins`, which consumes only
  `isImport`, URI, local SymbolId, canonical TypeId, and canonical declaration
  range already present in source scope facts and the symbol registry.
- Publishes an external `ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN` edge from
  the local alias SymbolId. The external endpoint has no invented SymbolId or
  source range; it retains the canonical TypeId and URI instead.
- Invokes the producer only after `ZrParser_Semantic_BuildSourceScopeFacts`.
- Deduplicates by local SymbolId, target TypeId, and URI. Missing identity,
  URI, TypeId, or range is omitted without a name or AST fallback.

## Exclusions

This slice does not publish alias-target, export-member, binary/native origin,
inheritance, implementation, override, constructor, or call graph relations.
It does not migrate an LSP consumer or add a token/name fallback.

## Verification

- RED: the relation test failed to link because
  `ZrParser_SemanticRelations_PublishImportOrigins` did not exist.
- The first producer test exposed an invalid name-based test lookup: a native
  `Vec3` had the same spelling as the local destructured import alias. The
  corrected test uses the registered binding AST identity and proves that the
  producer does not select the same-name native symbol.
- GREEN: the dedicated MSVC static cache directly executed relation graph
  `8 Tests 0 Failures 0 Ignored`, visible symbols `19 Tests 0 Failures 0
  Ignored`, semantic query `29 Tests 0 Failures 0 Ignored`, property consumer
  contracts `11 Tests 0 Failures 0 Ignored`, and compiler semantic diagnostics
  `46 Tests 0 Failures 0 Ignored`, all with process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 00:51 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 3 完成。
- 完成项目：source import URI 的 snapshot-owned carrier、direct/destructured
  alias origin producer、external TypeId/URI endpoint、name-collision
  fail-closed、幂等 late publication，以及关系/visible/query/property/diagnostic
  回归。
- 后续项目：alias-target、export member、source base/interface/override、
  binary/native origin、CFG 多定义读点关联、call graph 与 LSP relation consumers。
