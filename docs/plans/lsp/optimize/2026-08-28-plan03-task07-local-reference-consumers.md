---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.3: Local Reference Consumers

## Scope

This slice migrates source-local find-references and document-highlight
projection from `SZrReferenceTracker` to parser `DeclarationOf/ReferencesOf`
facts. Cross-project imported-member aggregation and binary/native external
metadata branches remain separate open boundaries. Syntax05-owned interface,
property, symbol, metadata, and token paths are untouched.

## TDD And Root Cause

On fixed HEAD `b33221af0bcebc2dc9da91aeb3697b3ecb136101`, the new
fixture resolved a local symbol, detached its tracker, and required three exact
locations/highlights. Source, binary, and native parity cases remained green,
but the new source-local case failed because both consumer helpers required the
tracker and rebuilt zero-width ranges from `symbol->name`.

After the relation consumer was installed, lower-layer inspection showed three
facts for the SymbolId and two resolved uses. Their source field was null. The
only permitted completion is the existing single-snapshot analyzer source
binder; request URI and coordinate/name fallbacks remain forbidden.

## Implementation

The new cohesive reference-query module calls `DeclarationOf` and
`ReferencesOf` with the selected stable SymbolId. It projects exact fact ranges,
binds a missing source only from the owning analyzer AST snapshot, rejects
unresolved facts, preserves cancellation, de-duplicates locations, and maps
structured reference kinds to read/write highlights. The old tracker helpers
and name-based zero-width normalization were deleted from the 200KB semantic
query module.

The test detaches the tracker, asserts exactly three initial results and
read/write kinds, releases the borrowed query before a version-2 update,
re-resolves and asserts four results, then proves an invalid SymbolId fails
closed. Source contracts reject tracker/name reconstruction in this consumer.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for:

- semantic-query parity, four scenarios including the new local consumer;
- LSP source contracts, 56 pass markers;
- reaching-definition navigation `2/2`;
- reference tracker, five scenarios.

GCC parent and overlay full-interface runs both return exit one with the same
four pre-existing markers; Clang and MSVC overlays retain that exact marker
set. Marker delta is zero and these runs are not counted as GREEN. GCC/Clang
executables contain the new helper once and the deleted helper zero times.
Workspace, WSL, and MSVC bytes match `5/5`.

## 状态与产出记录

- 完成时间：2026-08-28 19:40 +08:00。
- 状态：Task 7.3 source-local consumer 子里程碑已完成；Plan 03 Task 7 继续。
- 完成项目：references/highlights canonical fact projection、tracker-detached
  RED/GREEN、exact 3→4 snapshot re-resolution、invalid-id fail-closed、三工具链
  `4/56/2/5` focused 门禁、interface A/B delta 0、三处 `5/5` byte audit。
- 未完成项目：跨项目 imported references、binary/native external reference
  consumers、rename、remaining definition/implementation consumers，以及
  Syntax05 仍占用的 property/symbol consumer paths。
