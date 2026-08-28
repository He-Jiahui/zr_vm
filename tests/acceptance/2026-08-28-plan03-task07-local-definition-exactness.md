---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
tests:
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-local-definition-exactness.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.5 Local Definition Exactness

## Required Results

- Reaching and branch definitions come from parser `DefinitionsOf`.
- A missing position fact may use `DeclarationOf` only by resolved SymbolId.
- Missing fact source binds only to the owning analyzer snapshot.
- Missing fact and snapshot sources return no definition.
- No request URI, LSP symbol location, enum-name scan, or source-text fallback
  remains in the local definition path.

## TDD Evidence

Source contracts initially reported all three forbidden source paths. The new
runtime fixture removed both fact and analyzer source and observed an invented
definition while the two existing reaching cases stayed green. Removing the
fallbacks then exposed three interface cases without position facts; exact
`DeclarationOf(SymbolId)` support restored them without spelling reconstruction.

## Final Evidence

GCC, Clang, and MSVC pass source contracts 57, reaching definition three, and
parity four with real exits. Full interface retains the same four pre-existing
markers and `109` passes on every toolchain; delta from Task 7.4 is zero and the
runner is not GREEN. Workspace, WSL, and MSVC bytes match `4/4`.

## Acceptance Decision

Accepted for source-local definition exactness only. Cross-project imported
target identity, binary/native external navigation, rename, and remaining
navigation consumers stay open.

## 状态与产出记录

- 完成时间：2026-08-28 20:23 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：canonical reaching/declaration relation projection、snapshot-only
  source binding、source-less fail-closed、legacy fallback deletion、三工具链
  focused exits、interface marker audit、byte audit。
