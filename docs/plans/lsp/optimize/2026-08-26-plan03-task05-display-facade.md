---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_canonical_consumers.c
doc_type: milestone-record
---

# Plan 03 Task 5.1: Canonical Display Facade

## Goal

Provide a parser-owned, snapshot-scoped display facade over already-published
canonical TypeId, SymbolId, and property-contract facts. The facade must not
discover a symbol from a spelling or re-run type inference.

## Implementation

- Adds `semantic_display.h` and `semantic_display.c`.
- `FormatType` delegates directly to `ZrParser_CanonicalType_Format`, preserving
  canonical passing modes, owner/ref/readonly wrappers, generic arguments,
  receiver effects, and callable effects.
- `FormatSymbol` reads the exact registered SymbolId. When a matching resolved
  declaration fact has a signature display and the same TypeId, it returns that
  canonical fact unchanged. Otherwise it combines the registered display name
  and canonical TypeId; it never searches another same-name symbol.
- `FormatProperty` consumes a complete `SZrSemanticPropertyContract`, validates
  its property/accessor identities against the registry, then formats static,
  readonly, ref-access, type, and accessor availability from those fields.
- Existing `ZrParser_SemanticQuery_FormatCall` remains the call display entry
  point. This slice deliberately does not wrap or duplicate it.

## Exclusions

Documentation metadata, parameter-to-argument mappings, compatibility scores,
conversion exactness, receiver TypeId, binary/native callable fact parity, and
LSP presentation migration are not published by this slice. Missing canonical
identity or a missing registry row fails closed with an empty output buffer.

## Verification

- RED: the new target failed to compile because `semantic_display.h` did not
  exist. A second RED proved that generic symbol display fell back to
  `identity: fn() -> int` until a matching declaration signature fact was
  consumed.
- GREEN: the dedicated MSVC static cache directly ran `semantic_display` 3/0,
  semantic call query 3/0, and canonical consumers 19/0 with process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 06:00 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 5 或 Task 4 完成。
- 完成项目：canonical TypeId、SymbolId 和 PropertyContract display facade；
  matching declaration signature projection；missing identity fail-closed 回归。
- 后续项目：documentation metadata fact、CallAt unified display wrapper、
  binary/native display parity、LSP hover/signature/completion migration。
