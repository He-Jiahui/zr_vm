---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_type_hierarchy.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c
  - zr_vm_language_server/stdio/stdio_hierarchy.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.7: Local Type Hierarchy

## Scope

This slice migrates source-local type hierarchy requests from document-symbol
and inheritance-header scans to parser-owned base/derived relations. It covers
types available in one analyzer snapshot. Cross-project, binary, and native
external hierarchy expansion remains open. Syntax05-owned property consumers
and parser property support are untouched.

## TDD And Root Cause

On fixed baseline `7c416d8ea362bb52cecd6a24c78f4e739fbcf87b`, the type
hierarchy reconstructed base names from source headers and matched symbols by
display name. The RED fixture changed returned hierarchy item names to an
unrelated same-name type and required exact results after the mutation. It also
required stale version-one items to return no result after a version-two
document update.

The parser already published canonical `BaseTypesOf(TypeId)` and
`DerivedTypesOf(TypeId)` relations. The missing boundary was an LSP projection
that retained SymbolId, TypeId, and document version across the prepare and
follow-up requests.

## Implementation

`lsp_semantic_type_hierarchy` resolves the prepare position to one local type,
projects parser relations, binds ranges only through the owning analyzer
snapshot, and finds targets by exact SymbolId. Hierarchy items carry stable
SymbolId, TypeId, and source version in their protocol `data`. Follow-up
requests re-resolve the selection position in the current snapshot and require
all three values to match before querying relations.

The old type-header colon scan, identifier extraction, document-symbol lookup,
and name comparison were deleted from `lsp_hierarchy.c`. Call hierarchy remains
unchanged for the next slice. Missing, malformed, ambiguous, cancelled, and
stale identity fails closed. The stdio parser validates integral IDs and
rejects overflow instead of truncating JSON numbers.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for
semantic relations `19/19`, semantic-query parity `6/6`, source contracts
`59/59`, advanced editor features `73/73`, and the type-hierarchy stdio smoke.
The smoke mutates the display name while preserving protocol data and still
returns the exact base/subtype.

The full interface runner returns exit one on every toolchain with the same
`109 Pass / 4` pre-existing markers as Task 7.6; marker delta is zero and the
runner is not counted as GREEN. Workspace, WSL, and MSVC bytes match `10/10`;
`git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 22:15 +08:00。
- 状态：Task 7.7 local type hierarchy 子里程碑已完成；Plan 03 Task 7
  继续。
- 完成项目：canonical `BaseTypesOf/DerivedTypesOf` projection、hierarchy
  item SymbolId/TypeId/version transport、stale snapshot fail-closed、same-name
  exclusion、legacy type-header/name scan deletion、strict stdio identity
  parsing、三工具链 focused/stdio 门禁、interface marker delta 0、三处
  `10/10` byte audit。
- 未完成项目：call hierarchy、rename、cross-project/binary/native external
  hierarchy and implementations，以及其余 consumer migrations。
