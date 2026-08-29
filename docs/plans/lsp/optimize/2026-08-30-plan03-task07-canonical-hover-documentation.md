---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.32: Canonical Hover Documentation

## Goal

Make source hover documentation a parser-owned semantic fact projection. Hover text may include
non-semantic source metadata, but canonical documentation must be keyed by the exact `SymbolId` and
must remain available when LSP analyzer state is detached.

## Contract

- `ZrParser_SemanticQuery_DocumentationOfSymbol` is queried with the `SymbolId` from `SymbolAt`.
- The parser result is borrowed only for the current semantic snapshot; the hover projector creates
  the merged LSP string before returning it.
- Missing documentation stays missing. No symbol-table, comment, label, or other consumer-text
  fallback is permitted.
- Leading comments and FFI decorator metadata remain optional non-semantic source enrichment and
  cannot change canonical identity, type, signature, or range.

## RED/GREEN

The detached source-hover parity case published an exact documentation fact for `result`, then
removed the analyzer symbol table, reference tracker, and AST. Before the change, hover still
formatted the canonical symbol but omitted the documentation fact. After the projector queried the
fact by `SymbolId`, the same detached case retained the documentation in hover contents.

## Verification

- GCC fixed-source parity: `14/14`, process exit `0`.
- The new detached documentation assertion passes while the existing canonical hover identity,
  type, and range assertions remain unchanged.
- LSP source-contract target: process exit `0`.

## 状态与产出记录

- 完成时间：2026-08-30 05:12 +08:00。
- 状态：已完成 LSP hover documentation consumer 子里程碑；未声明 Plan 03 Task 7 或 Task 8 完成。
- 完成项目：hover exact SymbolId documentation query、snapshot lifetime copy、detached analyzer
  regression，以及 documentation/leading-comment ownership boundary。
- 后续项目：semantic tokens、diagnostics、cross-project/binary/native navigation，以及
  Syntax05-owned producer identity mismatch 仍按计划继续处理。
