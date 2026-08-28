---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/reference_tracker.h
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_source.c
tests:
  - tests/language_server/test_reference_tracker.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-reference-symbol-id-index.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.2 Reference SymbolId Index

## Required Results

- Group valid references only by canonical `SymbolId`.
- Isolate same-name symbols carrying different ids.
- Preserve pointer-local syntax recovery for invalid ids without indexing it.
- Bind source-less positions only at a proven single-snapshot analyzer boundary.
- Add no name, coordinate, type-text, or path fallback.

## TDD Evidence

The valid RED failed only because a second symbol wrapper with the same id
could not retrieve the first wrapper's references. The old name-keyed map plus
pointer filter caused the failure. Task 7.1's strict source check then exposed
seven analyzer callers that omitted the source despite the analyzer owning one
exact AST snapshot.

## Final Evidence

GCC, Clang, and MSVC pass tracker five, reaching definition `2/2`, semantic
query parity `3/3`, and 55 source-contract markers with real exits. Each full
analyzer process exits one with exactly the same two prior baseline failures
and no new Task 7.2 failure. Workspace, WSL, and MSVC bytes match `7/7`.

## Acceptance Decision

Accepted as the canonical reference-index boundary for Task 7. Relation-backed
navigation consumer migration remains open.

## 状态与产出记录

- 完成时间：2026-08-28 18:55 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：valid RED、SymbolId map、source snapshot binding、三工具链
  `5/2/3/55` 门禁、三处 `7/7` byte audit、既有 analyzer baseline 隔离。
