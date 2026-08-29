---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-30-plan03-task07-canonical-hover-documentation.md
doc_type: acceptance-record
---

# Plan 03 Task 7.32 Acceptance: Canonical Hover Documentation

## Scope

Source hover keeps its canonical `SymbolAt` identity/type/signature/range projection and now reads
documentation through `DocumentationOfSymbol(SymbolId)`. The projector merges the borrowed fact
into an LSP-owned string. Source comments and FFI metadata remain optional non-semantic enrichment.

## Evidence

- RED: the detached hover case published an exact documentation fact, detached analyzer state, and
  reported `13 Pass / 1 Fail` because the fact was not projected.
- GREEN: the same parity target reported `14/14` and real process exit `0` after the hover projector
  consumed the exact documentation fact.
- LSP source-contract target reported real process exit `0`.

## State

- Status: complete for this consumer slice.
- Completion time: `2026-08-30 05:12 +08:00`.
- No name, label, comment scan, symbol-table lookup, or rendered completion/signature text is used
  as documentation fallback.
- Full Plan 03 remains in progress; semantic-token and diagnostic consumers plus producer-side
  identity mismatches remain outside this slice.
