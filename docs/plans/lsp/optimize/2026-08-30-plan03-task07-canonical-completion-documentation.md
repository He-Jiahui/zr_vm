---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.31: Canonical Completion Documentation

## Goal

Make lexical completion documentation a canonical semantic-query projection. A completion item
must not require the LSP symbol table or recover documentation from a label, comment scan, hover,
or signature display.

## Contract

- `VisibleSymbols` supplies the exact `SymbolId` and display fields for each candidate.
- `ZrParser_SemanticQuery_DocumentationOfSymbol` is queried only with that exact `SymbolId`.
- The parser documentation result is borrowed from the current semantic snapshot; the LSP completion
  item copies the text before returning it to the protocol layer.
- Missing documentation remains unavailable. No same-name symbol, AST/comment, or other consumer's
  rendered text is a fallback.

## RED/GREEN

The parity case published documentation for the exact `canonicalLocal` symbol, then detached the
analyzer symbol table and document AST before collecting completion. Before the projector change,
the completion item had no documentation. The GREEN projector now consumes the documentation fact
from the canonical visible-symbol row and passes a copied string to the completion item.

## Verification

- GCC fixed-source parity: `14/14`, process exit `0`.
- GCC LSP source-contract suite: process exit `0`; all source-contract checks passed.
- The existing completion assertions for canonical visible symbols, unresolved type detail, and
  callable signature detail remain passing; the new exact documentation assertion also passes.

## 状态与产出记录

- 完成时间：2026-08-30 05:08 +08:00。
- 状态：已完成 LSP consumer 子里程碑；未声明 Plan 03 Task 7 或 Task 8 完成。
- 完成项目：canonical completion documentation projection、exact SymbolId lookup、snapshot
  lifetime copy、detached symbol-table regression。
- 后续项目：semantic tokens、diagnostics、cross-project/binary/native navigation，以及
  Syntax05-owned producer identity mismatch 仍按计划继续处理。
