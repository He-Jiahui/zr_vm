---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_display.c
  - tests/acceptance/2026-08-26-plan03-task05-documentation-facts.md
doc_type: milestone-record
---

# Plan 03 Task 5.4: Documentation Facts

## Goal

Publish symbol documentation as a parser-owned semantic fact so LSP hover,
completion, and signature help can query snapshot metadata without extracting
comments or borrowing another consumer's display text.

## Contract

- `ZrParser_SemanticDocumentation_Publish` accepts only an existing exact
  `SymbolId` and copies the documentation string into the semantic snapshot.
- Re-publishing equal text for an identity is idempotent. A conflicting value
  is rejected rather than silently replacing source, binary, or native data.
- `ZrParser_SemanticQuery_DocumentationOfSymbol` matches only the requested
  `SymbolId`; invalid, missing, or undocumented symbols return `NULL`.
- Returned strings are borrowed snapshot views. Reset or free invalidates the
  view, while callers that cross snapshots retain stable identity and copy
  text explicitly.
- Names, AST nodes, comments, URI spelling, and LSP symbol tables are not
  fallback inputs.

## Implementation

`SZrSemanticContext` owns a documentation-fact array. The array is initialized,
cleared on reset, and freed with the context. The field is appended after the
existing context fields so existing semantic array offsets remain stable for
incremental object caches. The display facade owns publication and exact-ID
lookup, while canonical display functions remain unchanged.

## Verification

The dedicated test registers two same-name symbols, publishes documentation to
only one, checks exact-ID lookup, equal republish, conflicting publish, invalid
identity, and reset clearing. Direct execution reports 4 tests and 0 failures
with process exit zero on fresh MSVC, WSL GCC 11.4, and WSL Clang 14 builds.
Adjacent fresh MSVC direct runs also report semantic query 30/0, symbols 19/0,
calls 10/0, contract 4/0, canonical consumers 19/0, and diagnostics 46/0.

## 状态与产出记录

- 完成时间：2026-08-26 15:46 +08:00。
- 状态：已完成并通过三工具链验收；不声明 Plan 03 Task 5 或 Task 4
  完成。
- 完成项目：exact SymbolId documentation fact、snapshot-owned text、
  idempotent/conflicting publication、reset lifecycle 和 fail-closed query。
- 后续项目：source/binary/native metadata producers、CallAt mapping and
  receiver projection、LSP hover/completion/signature migration。
