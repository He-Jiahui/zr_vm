---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_token_canonical.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_token_entries.c
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.58: Canonical Semantic Token Specialization

## Scope

Make source semantic tokens consume exact parser semantic facts for specialized
symbols and owner types. Resolved members must retain declaration kind when a
use-site callable TypeId differs from the declaration TypeId; unresolved
members must remain unavailable.

## TDD And Implementation

The first RED separated two failures. A resolved method reference had a valid
SymbolId and closed callable TypeId, but `SymbolAt` withheld declaration kind
because the declaration record carried an open TypeId. An unresolved member
correctly had no `SymbolAt`; its test failed only because the resolved control
member was not classified. The owner case already had an exact resolved type
reference and canonical owner TypeId, but the token consumer queried only
symbols after removal of the old type reconstruction path.

`SymbolAt` now uses exact SymbolId equality to project declaration kind and AST
identity while preserving the reference's specialized TypeId. A parser test
freezes this open-declaration/closed-use contract. The LSP resolver consumes
`SymbolAt` first, then accepts `CanonicalTypeAt` only for an exact source/byte
range, resolved type reference with the same TypeId, and canonical owner kind.
It never calls request-time type inference or scans names/text.

Canonical classification moved to `lsp_semantic_token_canonical.c`; token entry
conversion, deduplication, overlap preference, sorting, and delta encoding moved
to `lsp_semantic_token_entries.c`. The source scanner is now 890 lines instead
of 1265.

## Verification

On isolated GCC and Clang builds:

- semantic-query symbols pass `23/23`;
- all 70 LSP source-contract checks pass;
- LSP semantic diagnostics pass `19/19`, parity passes `15/15`, and property
  consumer contracts pass `11/11`;
- both interface executables retain expected real exit 1, while owner and
  unresolved semantic-token cases pass and the failure set drops from fixed5
  to fixed3.

The remaining failures are project references, module-link-chain references,
and import-chain semantic tokens. MSVC, the full 16-target matrix, and stdio
smoke were not run for this narrow consumer milestone.

## 状态与产出记录

- 完成时间：2026-08-31 14:17 +08:00。
- 状态：Task 7.58 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：specialized `SymbolAt` identity；exact canonical owner token；unresolved
  fail-closed；semantic-token canonical/entry模块化；GCC/Clang focused门禁；interface
  fixed5降为fixed3。
- 后续项目：在Syntax05释放import metadata producer后修复剩余导入链/项目引用标记，并完成
  source/binary/native parity、MSVC、16-target matrix与stdio smoke总门禁。
