---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-30-plan03-task07-canonical-completion-documentation.md
doc_type: acceptance-record
---

# Plan 03 Task 7.31 Acceptance: Canonical Completion Documentation

## Scope

Completion candidates continue to come from parser `VisibleSymbols`. Their documentation is now
looked up by the candidate's exact `SymbolId` through `DocumentationOfSymbol`; the LSP projector
copies that snapshot-owned text into the protocol item.

## Evidence

- RED: with an exact documentation fact published and analyzer symbol table/AST detached, parity
  reported `13 Pass / 1 Fail` because completion documentation was absent.
- GREEN: the same parity target reported `14/14` and real process exit `0` after the canonical
  projector consumed and copied the fact.
- LSP source-contract target reported real process exit `0`.

## State

- Status: complete for this consumer slice.
- Completion time: `2026-08-30 05:08 +08:00`.
- The implementation does not infer by label/name, scan comments, or reuse hover/signature text.
- Full Plan 03 remains in progress; semantic-token and diagnostic consumers plus producer-side
  identity mismatches remain outside this slice.
