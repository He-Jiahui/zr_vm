---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.8 Source Constructor Relations

## Scope

The source fixture compiles an explicit `Point.@constructor(x: int)`, resolves
the type and constructor solely by their exact declaration AST identities in
the canonical symbol registry, and requires one `CONSTRUCTOR` edge with exact
source/target `SymbolId`, `TypeId`, and declaration ranges. A separate
`Empty` fixture proves that a synthesized default constructor produces no
relation because it has no explicit member symbol or source range.

## Baseline And Diagnosis

Before production changes, the isolated MSVC relation runner reported `13
Tests 1 Failure`, with the constructor-edge assertion `expected TRUE, actual
FALSE`, and exited 1. The first producer placement also established that source
type symbols are registered during `ZrParser_Semantic_BuildSourceScopeFacts`;
publication before that pass correctly failed closed. The final bridge runs
after the scope-fact pass and consumes only pre-existing compiler prototype and
member facts.

## Test Inventory

- Explicit source constructor edge: stable ids, TypeIds, and endpoint ranges.
- Synthesized default constructor: no relation is emitted.
- Existing query, symbol, canonical-consumer, compiler-diagnostic, and
  compiler-integration regressions.

## Tooling Evidence

MSVC used the isolated native-generic static cache and direct executable
invocation. Each command ended with `exit $LASTEXITCODE`, so the test process
provided the result code. WSL GCC used the isolated
`build-lsp-plan03-override-gcc-wsl` cache and invoked the relation executable
directly after its target build.

## Results

- MSVC direct: relation 14/14, semantic query 30/30, symbols 19/19, canonical
  consumers 19/19, compiler diagnostics 46/46, and compiler integration
  127/127; every command exited 0.
- WSL GCC direct: relation 14/14; command exited 0.

## Acceptance Decision

Accepted for the narrow source-constructor producer. The implementation is
snapshot-backed and fail-closed: it cannot infer a constructor from spelling,
a token, an unregistered TypeId, AST traversal at query time, or LSP state.
Binary, native, imported, and synthesized constructor provenance remain out of
scope until their producers can provide stable endpoint identities.

## 状态与产出记录

- 完成时间：2026-08-26 09:01 +08:00。
- 状态：已完成。
- 完成项目：source explicit constructor canonical relation producer、exact
  endpoint identity/range projection、synthesized-constructor negative boundary
  与跨工具链定向验收。
- 验证：MSVC direct 14/14、30/30、19/19、19/19、46/46、127/127 均 exit 0；WSL
  GCC relation 14/14，exit 0。
