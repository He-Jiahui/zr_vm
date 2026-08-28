---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
tests:
  - tests/language_server/test_reference_tracker.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-reference-source-identity.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.1 Reference Source Identity

## Required Results

- Never equate ranges when either source URI is missing.
- Preserve equality for distinct string objects containing the same exact URI.
- Add no symbol-name, coordinate-only, or path-text fallback.
- Preserve reaching-definition and source/binary/native query behavior.

## TDD Evidence

The valid RED retained all three legacy tracker passes and failed only the new
case because a null-source query matched a source-bound reference. The same
test also freezes the opposite direction, two-null comparison, and positive
exact URI text equality.

## Final Evidence

All three toolchains pass tracker four, reaching definition `2/2`, semantic
query parity `3/3`, and 54 source-contract markers with real exits. Workspace,
WSL, and MSVC bytes match `2/2`.

## Acceptance Decision

Accepted as the source-identity boundary for Task 7 reference consumers. The
full stable-id consumer migration remains open.

## 状态与产出记录

- 完成时间：2026-08-28 18:35 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：valid RED、null URI fail-closed、exact URI text equality、三工具链
  `4/2/3/54` 门禁、三处 `2/2` byte audit。
