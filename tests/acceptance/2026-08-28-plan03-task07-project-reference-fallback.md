---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_project_features.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-project-reference-fallback.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.4 Project Reference Fallback

## Required Results

- Project source-symbol reference fallback does not query the reference tracker.
- Local identity and ranges come from parser relations keyed by SymbolId.
- A valid zero-local-reference query can continue project aggregation.
- Invalid identity, missing semantic context, and cancellation fail closed.
- No module/member-name or source-text fallback is added by this slice.

## TDD Evidence

The source contract failed on the missing shared projector call and the
remaining tracker call. After replacement, review found that returning only an
"appended" bit would prematurely stop a valid project query with no local use.
The final API separates operation success from local append state.

## Final Evidence

GCC, Clang, and MSVC pass source contracts 56 and parity four with real exits.
The project runner exits zero on all three with `54 Pass / 6` unchanged markers;
GCC parent/overlay marker delta is zero. Full interface keeps the identical
`109 Pass / 4` marker set and exits one, so it is explicitly not GREEN.
Workspace, WSL, and MSVC bytes match `4/4`.

## Acceptance Decision

Accepted only for the project source-symbol fallback. Imported references still
aggregate by module/member spelling and remain open with binary/native external
references, rename, and other navigation consumers.

## 状态与产出记录

- 完成时间：2026-08-28 19:59 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：shared SymbolId fact projector、tracker removal、zero-local
  continuation、三工具链 focused exits、parent/overlay marker audit、byte
  audit。
