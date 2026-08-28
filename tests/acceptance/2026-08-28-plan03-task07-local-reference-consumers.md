---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-local-reference-consumers.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.3 Local Reference Consumers

## Required Results

- Source-local references and highlights work without `SZrReferenceTracker`.
- Identity comes from canonical SymbolId and parser relation queries.
- Ranges and read/write kinds come from structured reference facts.
- Version updates require re-resolution; invalid ids and unresolved facts fail
  closed.
- No symbol-name, request-URI, coordinate, or type-text reconstruction is added.

## TDD Evidence

With the tracker detached, the new source-local case failed while the prior
source/binary/native parity cases remained green. The old consumer required the
tracker and normalized zero-width ranges from symbol spelling. The replacement
initially exposed null source fields on two otherwise resolved use facts; the
single-snapshot source binder supplied their known owner without request-URI
fallback.

## Final Evidence

All three toolchains pass parity four, source contracts 56, reaching definition
`2/2`, and tracker five with real exits. The local fixture yields exactly three
initial and four version-2 results after re-resolution, with invalid SymbolId
returning no result. Full-interface parent/overlay marker delta is zero.
Workspace, WSL, and MSVC bytes match `5/5`.

## Acceptance Decision

Accepted for source-local references and highlights only. Cross-project,
binary/native external, rename, and remaining navigation consumers stay open.

## 状态与产出记录

- 完成时间：2026-08-28 19:40 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：tracker-detached canonical projection、snapshot re-resolution、
  unresolved exactness、三工具链 focused 门禁、A/B baseline isolation、byte
  audit。
